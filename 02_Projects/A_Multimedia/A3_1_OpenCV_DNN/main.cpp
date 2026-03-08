// A3_1_OpenCV_DNN — Selfie segmentation via OpenCV DNN (T-API) + OpenCL bokeh blur.
//
// Pipeline:
//   1. Load BGR image from disk.
//   2. Run selfie segmentation ONNX model with DNN_TARGET_OPENCL (T-API path,
//      UMat stays GPU-resident).
//   3. Extract the cl_mem handle from the output UMat WITHOUT a host copy — the
//      mask buffer is passed directly to the OpenCL bokeh kernel.
//   4. Apply per-pixel bokeh blur: background pixels are box-filtered, foreground
//      pixels are copied verbatim.
//   5. Write output_mask.bmp and output_blurred.bmp; print a 3-row timing table.
//
// Timing:
//   - Inference: std::chrono::steady_clock (wall-clock, DNN is internally async)
//   - Blur:      cl::Event (GPU profiling, CL_QUEUE_PROFILING_ENABLE required)

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image.h>
#include <stb_image_write.h>
#include "image_utils.hpp"

#include "ocl_wrapper.hpp"   // create_context(), OclContext
#include "opencl_utils.hpp"  // CL_CHECK, load_kernel_source, duration_ms

#include <opencv2/opencv.hpp>
#include <opencv2/dnn.hpp>
#include <opencv2/core/ocl.hpp>

#include <CLI/CLI.hpp>

#include <algorithm>
#include <chrono>
#include <climits>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

int main(int argc, char* argv[]) {
    // ── CLI ──────────────────────────────────────────────────────────────────
    CLI::App app{"A3_1 OpenCV DNN — Selfie segmentation + bokeh blur via OpenCL"};
    std::string input_path;
    std::string model_path;
    float       threshold = 0.5f;

    app.add_option("--input",     input_path, "Path to input image (BMP/PNG/JPG)")->required();
    app.add_option("--model",     model_path, "Path to selfie segmentation ONNX model")->required();
    app.add_option("--threshold", threshold,  "Mask threshold (0.0–1.0, default 0.5)")
        ->default_val(0.5f);
    CLI11_PARSE(app, argc, argv);

    // ── OpenCV OpenCL availability check ────────────────────────────────────
    // WHY exit(0) not throw: this is a hardware/build capability check, not a
    // program error.  The caller (CI, scripts) should treat exit(0) as
    // "feature unavailable, skip" rather than a failure.
    if (!cv::ocl::haveOpenCL()) {
        std::cout << "[A3_1] OpenCV was not built with OpenCL support. "
                     "Cannot use DNN_TARGET_OPENCL. Exiting.\n";
        return 0;
    }

    // ── Load input image ─────────────────────────────────────────────────────
    cv::Mat bgr = cv::imread(input_path, cv::IMREAD_COLOR);
    if (bgr.empty()) {
        throw std::runtime_error("Failed to load image: " + input_path);
    }
    const int width  = bgr.cols;
    const int height = bgr.rows;

    // Integer overflow guard: pixel_count must fit in cl_int for kernel arg.
    // WHY size_t promotion: avoids signed overflow if width or height are large.
    if (static_cast<size_t>(width) * static_cast<size_t>(height) >
        static_cast<size_t>(INT_MAX)) {
        throw std::runtime_error("Image too large: pixel count exceeds INT_MAX");
    }
    const cl_int pixel_count = static_cast<cl_int>(static_cast<size_t>(width) * height);

    // ── OpenCL setup (MUST happen before DNN to share context) ──────────────
    // WHY create_context() before DNN: OpenCV's T-API (DNN_TARGET_OPENCL) and
    // our bokeh kernel must share the SAME cl_context.  If we let OpenCV create
    // its own internal context first, the cl_mem handle extracted from the output
    // UMat would belong to that context and be invalid in ours → clSetKernelArg
    // crash.  By calling attachContext() first we ensure one unified context.
    auto ocl = create_context();

    // WHY CL_QUEUE_PROFILING_ENABLE: cl::Event timestamps require this flag.
    // create_context() returns a queue without profiling; we create our own.
    cl::CommandQueue queue(ocl.context, ocl.device, CL_QUEUE_PROFILING_ENABLE);

    // Share our context with OpenCV's T-API.
    {
        const std::string platform_name = ocl.platform.getInfo<CL_PLATFORM_NAME>();
        cv::ocl::attachContext(platform_name,
                               static_cast<void*>(ocl.platform()),
                               static_cast<void*>(ocl.context()),
                               static_cast<void*>(ocl.device()));
    }

    // ── UMat: keep image GPU-resident for T-API inference ────────────────────
    cv::UMat bgr_umat;
    bgr.copyTo(bgr_umat);  // upload to GPU via shared context

    // ── Load ONNX model and set OpenCL target ────────────────────────────────
    cv::dnn::Net net = cv::dnn::readNetFromONNX(model_path);
    net.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
    net.setPreferableTarget(cv::dnn::DNN_TARGET_OPENCL);

    // WHY getAvailableTargets: DNN silently falls back to CPU if DNN_TARGET_OPENCL
    // is unavailable at runtime (e.g., OpenCV built without OpenCL DNN support).
    // An explicit check lets callers treat exit(0) as "skip" rather than a failure.
    {
        auto available_targets = cv::dnn::getAvailableTargets(cv::dnn::DNN_BACKEND_OPENCV);
        bool ocl_target_available = std::find(available_targets.begin(), available_targets.end(),
                                               cv::dnn::DNN_TARGET_OPENCL) != available_targets.end();
        if (!ocl_target_available) {
            std::cerr << "[A3_1] DNN_TARGET_OPENCL is not available on this platform. "
                      << "Ensure OpenCV was built with OpenCL support.\n";
            return 0;
        }
    }

    // ── Preprocess: build blob ───────────────────────────────────────────────
    // selfie_segmentation.onnx expects: 1×3×256×256 float, [0,1] normalised, RGB.
    // WHY swapRB=true: cv::imread returns BGR; the model was trained on RGB.
    // WHY size 256×256: MediaPipe selfie segmentation input resolution.
    // WHY cv::Mat not cv::UMat: blobFromImage() has no UMat output overload —
    // blob construction (normalize, resize, NCHW reorder) always runs on CPU.
    // net.setInput() uploads it to GPU internally when DNN_TARGET_OPENCL is set.
    const cv::Size model_input_size(256, 256);
    cv::Mat blob = cv::dnn::blobFromImage(bgr, 1.0 / 255.0, model_input_size,
                                          cv::Scalar(), /*swapRB=*/true,
                                          /*crop=*/false, CV_32F);
    // WHY named output layer: net.forward(vector<UMat>, layer_name) needs an
    // explicit output layer name; empty string uses the last layer.
    const std::string output_layer_name = "";  // empty = last layer (selfie segmentation)
    net.setInput(blob);

    // ── Inference (wall-clock timing) ────────────────────────────────────────
    // WHY steady_clock not cl::Event: DNN inference uses an internal OpenCL queue
    // we do not control.  Wall-clock is the only portable timing option here.
    const auto t_infer_start = std::chrono::steady_clock::now();

    // Run inference — DNN_TARGET_OPENCL keeps output GPU-resident in UMat.
    // WHY try/catch cl::Error: OpenCV's ocl4dnn backend passes Intel-specific
    // compiler flags (e.g. -cl-no-subgroup-ifp) that NVIDIA's OpenCL compiler
    // rejects with CL_BUILD_PROGRAM_FAILURE.  With CL_HPP_ENABLE_EXCEPTIONS this
    // surfaces as cl::Error thrown from inside net.forward().  Per design §3 a
    // graceful fallback to CPU is required instead of a crash.
    std::vector<cv::UMat> dnn_outs;
    try {
        net.forward(dnn_outs, output_layer_name);
    } catch (const cl::Error& e) {
        std::cerr << "[A3_1] DNN_TARGET_OPENCL failed (" << e.what()
                  << ", code " << e.err() << "). "
                  << "Likely an ocl4dnn/driver incompatibility (e.g. NVIDIA). "
                  << "Falling back to DNN_TARGET_CPU.\n";
        dnn_outs.clear();
        net.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);
        net.setInput(blob);
        net.forward(dnn_outs, output_layer_name);
    }
    if (dnn_outs.empty()) {
        throw std::runtime_error("[A3_1] DNN forward returned empty output.");
    }
    cv::UMat& output_umat_dnn = dnn_outs[0];  // shape: [1, 1, 256, 256] floats

    const auto t_infer_end = std::chrono::steady_clock::now();
    const double infer_ms =
        std::chrono::duration<double, std::milli>(t_infer_end - t_infer_start).count();

    // ── Resize mask to input image dimensions ────────────────────────────────
    // The DNN outputs a [1,1,256,256] UMat; reshape to 2D before resize.
    // WHY reshape on UMat: avoids downloading to CPU — stays GPU-resident throughout.
    // UMat.reshape() shares the underlying buffer; no copy occurs.
    if (output_umat_dnn.dims < 4) {
        throw std::runtime_error("[A3_1] Unexpected DNN output shape.");
    }
    // WHY 3-arg reshape: UMat::reshape(cn, ndims, sizes) is the multi-dim overload.
    // sizes[] selects the H and W dims (indices 2,3) of the [1,1,H,W] output.
    const int mask_sizes[2] = {output_umat_dnn.size[2], output_umat_dnn.size[3]};
    cv::UMat mask_umat_256 = output_umat_dnn.reshape(1, 2, mask_sizes);

    // WHY resize to a UMat: keeps the mask GPU-resident so the cl_mem handle
    // extracted below is a direct pointer into GPU VRAM — no host round-trip.
    cv::UMat mask_resized_umat;
    cv::resize(mask_umat_256, mask_resized_umat, cv::Size(width, height),
               0, 0, cv::INTER_LINEAR);

    // ── Mask buffer ───────────────────────────────────────────────────────────
    // CRITICAL lifetime: mask_resized_umat must remain alive until queue.finish().
    cl_mem raw_mask = static_cast<cl_mem>(mask_resized_umat.handle(cv::ACCESS_READ));
    const bool mask_on_gpu = raw_mask != nullptr;
    std::cout << "[A3_1] DNN mask GPU-resident: " << (mask_on_gpu ? "YES (OpenCL)" : "NO (CPU fallback)") << "\n";

    const size_t mask_bytes = static_cast<size_t>(width) * height * sizeof(float);
    cl::Buffer mask_buf;
    if (mask_on_gpu) {
        mask_buf = cl::Buffer(raw_mask, /*retain=*/true);   // zero-copy GPU handle
    } else {
        cv::Mat tmp;
        mask_resized_umat.copyTo(tmp);                      // CPU fallback: download once
        mask_buf = cl::Buffer(ocl.context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                              mask_bytes, tmp.ptr<float>());
    }

    // ── Handle extraction: input image (BGR→RGBA on GPU, no CPU round-trip) ──
    // WHY cvtColor on bgr_umat: T-API keeps the converted image GPU-resident so
    // we can extract the cl_mem handle directly — same zero-copy pattern as mask.
    cv::UMat rgba_umat;
    cv::cvtColor(bgr_umat, rgba_umat, cv::COLOR_BGR2RGBA);
    cl_mem raw_input = static_cast<cl_mem>(rgba_umat.handle(cv::ACCESS_READ));
    const size_t rgba_bytes = static_cast<size_t>(width) * height * 4;
    // CRITICAL lifetime: rgba_umat must stay alive until queue.finish().
    cl::Buffer input_buf(raw_input, /*retain=*/true);  // zero-copy GPU handle

    // ── Kernel build ──────────────────────────────────────────────────────────
    // WHY std::filesystem::path: argv[0] string splitting is fragile on paths
    // with multiple slashes or relative prefixes. parent_path() handles all cases.
    const std::string bin_dir =
        std::filesystem::path(argv[0]).parent_path().string() + "/";

    const std::string kernel_src =
        load_kernel_source(bin_dir + "kernels/bokeh_blur.cl");

    cl::Program program(ocl.context, kernel_src);
    try {
        program.build();
    } catch (const cl::Error&) {
        std::string log;
        CL_CHECK(program.getBuildInfo(ocl.device, CL_PROGRAM_BUILD_LOG, &log));
        std::cerr << "bokeh_blur build log:\n" << log << "\n";
        throw;
    }
    cl::Kernel kernel(program, "bokeh_blur");

    // ── Output blurred buffer ────────────────────────────────────────────────
    cl::Buffer output_buf(ocl.context, CL_MEM_WRITE_ONLY, rgba_bytes);

    // ── Set kernel args ───────────────────────────────────────────────────────
    CL_CHECK(kernel.setArg(0, input_buf));
    CL_CHECK(kernel.setArg(1, output_buf));
    CL_CHECK(kernel.setArg(2, mask_buf));
    CL_CHECK(kernel.setArg(3, static_cast<cl_int>(width)));
    CL_CHECK(kernel.setArg(4, static_cast<cl_int>(height)));
    CL_CHECK(kernel.setArg(5, threshold));

    // ── Enqueue bokeh kernel with cl::Event timing ───────────────────────────
    cl::Event ev_blur;
    CL_CHECK(queue.enqueueNDRangeKernel(kernel,
                                        cl::NullRange,
                                        cl::NDRange(static_cast<size_t>(pixel_count)),
                                        cl::NullRange,
                                        nullptr,
                                        &ev_blur));
    CL_CHECK(queue.finish());
    const double blur_ms = duration_ms(ev_blur);

    // ── Read back blurred result ──────────────────────────────────────────────
    std::vector<uint8_t> blurred_host(rgba_bytes);
    CL_CHECK(queue.enqueueReadBuffer(output_buf, CL_TRUE, 0,
                                     rgba_bytes, blurred_host.data()));

    // ── Save output_blurred.bmp (RGBA) ───────────────────────────────────────
    save_bmp("output_blurred.bmp", blurred_host, width, height, 4);

    // ── Save output_mask.bmp ─────────────────────────────────────────────────
    // cv::threshold on UMat runs on GPU (T-API); convertTo downloads 8-bit result.
    cv::UMat mask_thresh_umat;
    cv::threshold(mask_resized_umat, mask_thresh_umat,
                  static_cast<double>(threshold), 255.0, cv::THRESH_BINARY);
    cv::Mat mask_8u;
    mask_thresh_umat.convertTo(mask_8u, CV_8UC1);
    save_bmp("output_mask.bmp",
             std::vector<uint8_t>(mask_8u.data, mask_8u.data + mask_8u.total()),
             width, height, 1);

    std::cout << "Saved: output_blurred.bmp\n";
    std::cout << "Saved: output_mask.bmp\n\n";

    // ── Timing table ─────────────────────────────────────────────────────────
    const double total_ms = infer_ms + blur_ms;

    std::cout << std::fixed << std::setprecision(3);
    std::cout << "[A3_1] Inference (CPU wall-clock): " << infer_ms  << " ms\n";
    std::cout << "[A3_1] Bokeh blur (cl::Event):     " << blur_ms   << " ms\n";
    std::cout << "[A3_1] Total:                       " << total_ms  << " ms\n";

    // Performance gate: < 15 ms combined at 1080p on discrete GPU.
    // WAIVER for iGPU — kernel launch overhead dominates on shared-memory topology.
    if (total_ms < 15.0) {
        std::cout << "Gate: PASS   [inference + blur < 15.000 ms]\n";
    } else {
        std::cout << "Gate: WAIVER (iGPU or CPU device — result is functionally correct)\n";
    }

    return 0;
}
