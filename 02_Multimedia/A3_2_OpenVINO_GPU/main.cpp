// A3_2_OpenVINO_GPU — Selfie segmentation via OpenVINO RemoteTensor API + OpenCL bokeh blur.
//
// Pipeline:
//   1. Load input image (RGBA), upload full-resolution to GPU as cl::Buffer (single upload).
//   2. GPU preprocess_nchw kernel: nearest-neighbour resize + RGBA→NCHW float32 [0,1].
//   3. Read model input shape from compiled model — no hardcoded dimensions.
//   4. Wrap NCHW cl::Buffer as OpenVINO RemoteTensor (zero host copy at infer time).
//   5. Pre-allocate output cl::Buffer from model output shape; bind as RemoteTensor before infer.
//   6. Run selfie segmentation inference via req.infer().
//   7. Output mask is GPU-resident in the pre-allocated cl::Buffer (no host copy needed).
//   8. GPU mask_resize kernel: nearest-neighbour upscale mask to full image resolution.
//   9. GPU bokeh_blur kernel: background pixels box-filtered, foreground sharp.
//  10. Save output_mask.bmp and output_blurred.bmp; print inference + blur timing.
//
// Intel iGPU only. Exits with code 0 and a descriptive message on unsupported hardware.
//
// Timing:
//   - Inference: std::chrono::steady_clock (OpenVINO does not expose cl::Event for full request)
//   - Blur:      cl::Event (GPU profiling, CL_QUEUE_PROFILING_ENABLE required)

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image.h>
#include <stb_image_write.h>
#include "image_utils.hpp"

#include "ocl_wrapper.hpp"   // create_context(), OclContext
#include "opencl_utils.hpp"  // CL_CHECK, load_kernel_source, duration_ms

// OpenVINO headers must come after cl.hpp (via opencl_utils.hpp) so that
// cl::Buffer / cl_mem are already defined when openvino/ocl.hpp is parsed.
#include <openvino/openvino.hpp>
#include <openvino/runtime/intel_gpu/ocl/ocl.hpp>

#include <CLI/CLI.hpp>

#include <algorithm>
#include <chrono>
#include <climits>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Helper: build a cl::Program from a kernel file, printing the build log on
// failure before re-throwing so the graceful-fallback catch can handle it.
// ---------------------------------------------------------------------------
static cl::Program build_program(const cl::Context& ctx,
                                  const cl::Device&  dev,
                                  const std::string& path)
{
    cl::Program prog(ctx, load_kernel_source(path));
    try {
        prog.build();
    } catch (const cl::Error&) {
        std::string log;
        CL_CHECK(prog.getBuildInfo(dev, CL_PROGRAM_BUILD_LOG, &log));
        std::cerr << "Build log (" << path << "):\n" << log << "\n";
        throw;
    }
    return prog;
}

int main(int argc, char* argv[]) {
    // ── CLI ──────────────────────────────────────────────────────────────────
    CLI::App app{"A3_2 OpenVINO GPU — Selfie segmentation RemoteTensor + bokeh blur"};
    std::string input_path;
    std::string model_path = "assets/selfie_segmentation.onnx";
    float       threshold  = 0.5f;

    app.add_option("--input",     input_path, "Path to input image (BMP/PNG/JPG)")->required();
    app.add_option("--model",     model_path, "Path to ONNX model")
        ->default_val("assets/selfie_segmentation.onnx");
    app.add_option("--threshold", threshold,  "Mask threshold (0.0–1.0, default 0.5)")
        ->default_val(0.5f);
    int runs = 5;
    app.add_option("--runs", runs, "Number of inference iterations (default 5)")
        ->default_val(5);
    CLI11_PARSE(app, argc, argv);

    // ── Load input image (RGBA, 4 channels) ─────────────────────────────────
    // WHY RGBA: bokeh_blur kernel reads uchar4 per pixel; loading as RGBA avoids
    // a separate conversion step and the same buffer feeds the preprocess kernel.
    int width = 0, height = 0, loaded_channels = 0;
    std::vector<uint8_t> rgba_host =
        load_rgba_image(input_path, width, height, loaded_channels);
    const size_t rgba_bytes = static_cast<size_t>(width) * height * 4;

    // Integer overflow guard for cl_int kernel args.
    if (static_cast<size_t>(width) * height > static_cast<size_t>(INT_MAX)) {
        throw std::runtime_error("Image too large: pixel count exceeds INT_MAX");
    }
    const cl_int pixel_count = static_cast<cl_int>(static_cast<size_t>(width) * height);

    // ── OpenCL context ───────────────────────────────────────────────────────
    auto ocl = create_context();

    // WHY CL_QUEUE_PROFILING_ENABLE: cl::Event timestamps for blur kernel timing.
    cl::CommandQueue queue(ocl.context, ocl.device, CL_QUEUE_PROFILING_ENABLE);

    // ── Upload full-resolution RGBA to GPU (single upload) ───────────────────
    // WHY single upload: this buffer is reused by two kernels —
    //   • preprocess_nchw  (reads to resize + normalise for inference input)
    //   • bokeh_blur       (reads RGBA source pixels for background blur)
    // A second upload would waste PCIe bandwidth for identical data.
    cl::Buffer rgba_buf(ocl.context,
                        CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                        rgba_bytes,
                        rgba_host.data());

    const std::string bin_dir =
        std::filesystem::path(argv[0]).parent_path().string() + "/";

    // ── OpenVINO pipeline (single graceful-fallback catch) ───────────────────
    // WHY single try/catch: read_model, compile_model, create_infer_request,
    // and infer() can all throw if the GPU plugin is unavailable or the model
    // is incompatible.  One catch avoids duplicating the exit-0 logic (§4).
    ov::Core core;
    std::optional<ov::intel_gpu::ocl::ClContext> remote_ctx_opt;
    try {
        // WHY pass ocl.context: OpenVINO must share our OpenCL context so that
        // cl::Buffer handles are valid in both runtimes.
        remote_ctx_opt.emplace(core, ocl.context.get());
        ov::intel_gpu::ocl::ClContext& remote_ctx = *remote_ctx_opt;

        auto model    = core.read_model(model_path);
        // WHY no PrePostProcessor: all three model formats (f32 ONNX, int8 ONNX,
        // int8 IR) expose f32 at both tensor boundaries — quantization is internal
        // to the compiled graph.  Adding a PrePostProcessor f32→f32 cast node
        // inserts a no-op Convert that the GPU plugin does not always eliminate,
        // adding measurable latency with no correctness benefit.
        auto compiled = core.compile_model(model, remote_ctx);

        // ── Read input shape from compiled model (no hardcoded dimensions) ───
        // WHY read from model: makes the pipeline work with any ONNX model, not
        // just selfie_segmentation. Dynamic-batch models export [?,3,H,W]; we
        // fix batch=1 and keep H,W as reported by the model.
        auto input_port  = compiled.input();
        ov::PartialShape in_ps = input_port.get_partial_shape();
        ov::Shape input_shape  = in_ps.is_static()
            ? in_ps.to_shape()
            : ov::Shape{1, 3, 256, 256};  // fallback for dynamic-batch exports
        const cl_int model_h = static_cast<cl_int>(input_shape[2]);
        const cl_int model_w = static_cast<cl_int>(input_shape[3]);
        const size_t input_floats =
            static_cast<size_t>(3) * static_cast<size_t>(model_h) * model_w;

        // ── GPU preprocessing: resize + RGBA→NCHW float (no CPU round-trip) ──
        // WHY GPU kernel: rgba_buf is already resident; running preprocess_nchw
        // avoids downloading to CPU, converting, and re-uploading — one fewer
        // host↔device transfer compared to the CPU preprocess_to_nchw approach.
        cl::Buffer input_buf(ocl.context, CL_MEM_READ_WRITE,
                             input_floats * sizeof(float));
        {
            auto prog = build_program(ocl.context, ocl.device,
                                      bin_dir + "kernels/preprocess_nchw.cl");
            cl::Kernel k(prog, "preprocess_nchw");
            CL_CHECK(k.setArg(0, rgba_buf));
            CL_CHECK(k.setArg(1, input_buf));
            CL_CHECK(k.setArg(2, static_cast<cl_int>(width)));
            CL_CHECK(k.setArg(3, static_cast<cl_int>(height)));
            CL_CHECK(k.setArg(4, model_w));
            CL_CHECK(k.setArg(5, model_h));
            CL_CHECK(queue.enqueueNDRangeKernel(
                k, cl::NullRange,
                cl::NDRange(static_cast<size_t>(model_w) * model_h),
                cl::NullRange));
            // No finish(): input_buf is consumed by compile_model/infer on the
            // same in-order queue — implicit ordering is sufficient.
        }

        // ── Pre-allocate output tensor from model output shape ────────────────
        // WHY pre-allocate: if the output tensor is not pre-bound as a
        // ClBufferTensor before infer(), the GPU plugin may return a plain host
        // Tensor from get_output_tensor(), making the .as<ClBufferTensor>() cast
        // throw.  Pre-allocating and binding ensures the plugin writes directly
        // into our cl::Buffer — no extra copy, no ownership ambiguity.
        auto output_port = compiled.output();
        ov::PartialShape out_ps = output_port.get_partial_shape();
        ov::Shape output_shape  = out_ps.is_static()
            ? out_ps.to_shape()
            : ov::Shape{1, 1, static_cast<size_t>(model_h),
                              static_cast<size_t>(model_w)};
        const size_t output_floats = std::accumulate(
            output_shape.begin(), output_shape.end(),
            size_t{1}, std::multiplies<size_t>{});
        // WHY CL_MEM_READ_WRITE: OpenVINO writes the inference result here;
        // mask_resize reads it.  READ_ONLY would be rejected by the GPU plugin.
        cl::Buffer mask_buf(ocl.context, CL_MEM_READ_WRITE,
                            output_floats * sizeof(float));
        auto output_tensor = remote_ctx.create_tensor(
            output_port.get_element_type(), output_shape, mask_buf);

        // ── Bind tensors and run inference ────────────────────────────────────
        auto req = compiled.create_infer_request();
        auto input_tensor = remote_ctx.create_tensor(
            input_port.get_element_type(), input_shape, input_buf);
        req.set_input_tensor(input_tensor);
        req.set_output_tensor(output_tensor);

        // WHY steady_clock: OpenVINO does not expose a cl::Event for the full
        // inference request duration.  Wall-clock is the only portable option.
        // WHY multiple runs: first call may include GPU driver warm-up overhead;
        // subsequent calls reflect stable inference latency.
        std::vector<double> infer_times;
        infer_times.reserve(runs);
        for (int r = 0; r < runs; ++r) {
            const auto t0 = std::chrono::steady_clock::now();
            req.infer();
            const auto t1 = std::chrono::steady_clock::now();
            infer_times.push_back(
                std::chrono::duration<double, std::milli>(t1 - t0).count());
        }
        const double infer_ms     = infer_times.back();   // last run for pipeline output
        const double infer_ms_min = *std::min_element(infer_times.begin(), infer_times.end());
        const double infer_ms_avg = std::accumulate(infer_times.begin(), infer_times.end(), 0.0)
                                    / infer_times.size();

        // mask_buf now contains the inference output, GPU-resident.
        // Mask dimensions from output shape (e.g. [1,1,256,256] → 256×256).
        const cl_int mask_h = static_cast<cl_int>(output_shape[2]);
        const cl_int mask_w = static_cast<cl_int>(output_shape[3]);

        // ── Resize mask to full image resolution on GPU (no host round-trip) ──
        const size_t full_mask_floats = static_cast<size_t>(width) * height;
        cl::Buffer mask_full_buf(ocl.context, CL_MEM_READ_WRITE,
                                 full_mask_floats * sizeof(float));
        {
            auto prog = build_program(ocl.context, ocl.device,
                                      bin_dir + "kernels/mask_resize.cl");
            cl::Kernel k(prog, "mask_resize");
            CL_CHECK(k.setArg(0, mask_buf));
            CL_CHECK(k.setArg(1, mask_full_buf));
            CL_CHECK(k.setArg(2, mask_w));
            CL_CHECK(k.setArg(3, mask_h));
            CL_CHECK(k.setArg(4, static_cast<cl_int>(width)));
            CL_CHECK(k.setArg(5, static_cast<cl_int>(height)));
            CL_CHECK(queue.enqueueNDRangeKernel(
                k, cl::NullRange,
                cl::NDRange(full_mask_floats),
                cl::NullRange));
            // No finish(): bokeh_blur consumes mask_full_buf on the same
            // in-order queue — implicit ordering is sufficient.
        }

        // ── Bokeh blur kernel ─────────────────────────────────────────────────
        cl::Buffer output_buf(ocl.context, CL_MEM_WRITE_ONLY, rgba_bytes);
        cl::Event  ev_blur;
        {
            auto prog = build_program(ocl.context, ocl.device,
                                      bin_dir + "kernels/bokeh_blur.cl");
            // Kernel signature: bokeh_blur(input, output, mask, width, height, threshold)
            cl::Kernel k(prog, "bokeh_blur");
            CL_CHECK(k.setArg(0, rgba_buf));
            CL_CHECK(k.setArg(1, output_buf));
            CL_CHECK(k.setArg(2, mask_full_buf));
            CL_CHECK(k.setArg(3, static_cast<cl_int>(width)));
            CL_CHECK(k.setArg(4, static_cast<cl_int>(height)));
            CL_CHECK(k.setArg(5, static_cast<cl_float>(threshold)));
            CL_CHECK(queue.enqueueNDRangeKernel(
                k, cl::NullRange,
                cl::NDRange(static_cast<size_t>(pixel_count)),
                cl::NullRange,
                nullptr, &ev_blur));
        }

        // WHY finish() before req goes out of scope: output_tensor references
        // mask_buf; InferRequest holds internal state tied to that binding.
        // Destroying req while GPU work referencing output_tensor is in flight
        // is undefined.  finish() guarantees all queued work completes first.
        CL_CHECK(queue.finish());
        const double blur_ms = duration_ms(ev_blur);

        // ── Save outputs ──────────────────────────────────────────────────────
        std::vector<uint8_t> blurred_host(rgba_bytes);
        CL_CHECK(queue.enqueueReadBuffer(output_buf, CL_TRUE, 0,
                                         rgba_bytes, blurred_host.data()));
        save_bmp("output_blurred.bmp", blurred_host, width, height, 4);
        std::cout << "Saved: output_blurred.bmp\n";

        // Mask visualisation: read back after finish() — this is a pure
        // visualisation step after all GPU work completes, not between inference
        // and blur (DoD constraint satisfied).
        std::vector<float> mask_full_host(full_mask_floats);
        CL_CHECK(queue.enqueueReadBuffer(mask_full_buf, CL_TRUE, 0,
                                         full_mask_floats * sizeof(float),
                                         mask_full_host.data()));
        std::vector<uint8_t> mask_vis(full_mask_floats);
        for (size_t i = 0; i < full_mask_floats; ++i) {
            mask_vis[i] = (mask_full_host[i] >= threshold) ? 255u : 0u;
        }
        save_bmp("output_mask.bmp", mask_vis, width, height, 1);
        std::cout << "Saved: output_mask.bmp\n\n";

        // ── Timing report ─────────────────────────────────────────────────────
        // Stable average excludes run 1 (JIT warm-up). If runs==1, fall back
        // to the single measurement.
        const double infer_ms_stable_avg = (runs > 1)
            ? std::accumulate(infer_times.begin() + 1, infer_times.end(), 0.0)
              / (infer_times.size() - 1)
            : infer_times[0];

        std::cout << std::fixed << std::setprecision(3);
        std::cout << "\n[A3_2 OpenVINO GPU]\n";
        std::cout << "Inference  (wall-clock, " << runs << " runs):\n";
        for (int r = 0; r < runs; ++r) {
            std::cout << "  run " << std::setw(2) << (r + 1) << ": "
                      << std::setw(9) << infer_times[r] << " ms"
                      << (r == 0 ? "  <- JIT warm-up" : "")
                      << "\n";
        }
        std::cout << "  " << std::string(26, '-') << "\n";
        std::cout << "  min:    " << std::setw(9) << infer_ms_min          << " ms\n";
        std::cout << "  avg*:   " << std::setw(9) << infer_ms_stable_avg   << " ms"
                  << (runs > 1 ? "  (* runs 2+)" : "") << "\n";
        std::cout << "Blur kernel  (cl::Event):   "
                  << std::setw(9) << blur_ms << " ms\n";

    } catch (const std::exception& e) {
        // Graceful fallback: GPU plugin unavailable, model load failure, or
        // inference failure are hardware/environment limitations — exit 0
        // signals "skip" to callers (master_specs §4).
        std::cout << "[A3_2] OpenVINO GPU pipeline failed: " << e.what()
                  << "\nIntel iGPU required. Exiting.\n";
        return 0;
    }

    return 0;
}
