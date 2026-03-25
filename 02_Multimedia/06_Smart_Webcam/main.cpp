// A4_Smart_Webcam — Zero-copy AI segmentation + bokeh blur via shared OpenCL/OpenVINO context.
//
// Pipeline per frame:
//   1. Capture: copy BGR image into gpu bgr_buf via enqueueWriteBuffer.
//   2. Preprocess (cl::Event): GPU preprocess_nchw — resize + BGR→NCHW float32 with B↔R swap.
//   3. Inference (steady_clock): OpenVINO req.infer() via RemoteTensor (zero copy).
//   4. Blur (cl::Event): GPU bokeh_blur — bilinear mask interp + 5×5 box blur.
//   5. queue.finish() gate — mandatory before loop restart (§constraints).
//   6. Display: enqueueReadBuffer → cv::imshow (webcam) or stb_image_write (offline).
//
// WHY shared context: OpenVINO and OpenCL kernels share the same cl_context so that
// cl::Buffer handles are valid in both runtimes without extra copies.
//
// WHY CL_MEM_READ_WRITE for all buffers: OpenVINO GPU plugin rejects
// CL_MEM_READ_ONLY / CL_MEM_WRITE_ONLY at runtime when binding RemoteTensors.
//
// WHY pre-bind output tensor: if output is not pre-bound before infer(), the GPU
// plugin returns a host Tensor; .as<ClBufferTensor>() would throw.
//
// WHY BGR (3 bytes/pixel): cv::VideoCapture delivers BGR natively. Using 3-byte
// stride avoids an alpha channel copy on every frame. Channel swap BGR→RGB is
// handled inside preprocess_nchw.cl at zero extra cost.

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

// OpenCV: ONLY for webcam capture and display — no image processing.
#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/videoio.hpp>

#include <CLI/CLI.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <climits>
#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <numeric>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>


int main(int argc, char* argv[]) {
    // ── CLI11 ─────────────────────────────────────────────────────────────────
    CLI::App app{"A4 Smart Webcam — AI segmentation + bokeh blur via shared OpenCL/OpenVINO context"};

    std::string input_path;
    std::string model_path = "assets/selfie_segmentation.onnx";
    bool        loop_mode  = false;
    int         device_idx = 0;
    int         width      = 1920;
    int         height     = 1080;
    int         runs       = 30;
    int         exposure   = -1;   // -1 = auto; >0 = manual V4L2 absolute exposure (100 µs units)

    app.add_option("--input",  input_path, "Path to input image (offline mode; omit for webcam)");
    app.add_option("--model",  model_path, "Path to ONNX model")
        ->default_val("assets/selfie_segmentation.onnx");
    app.add_flag  ("--loop",   loop_mode,  "Replay --input in a loop for --runs frames");
    app.add_option("--device", device_idx, "Webcam device index (ignored when --input is set)")
        ->default_val(0);
    app.add_option("--width",  width,      "Webcam capture width (default 1920; ignored in offline mode)")
        ->default_val(1920);
    app.add_option("--height", height,     "Webcam capture height (default 1080; ignored in offline mode)")
        ->default_val(1080);
    app.add_option("--runs",   runs,       "Number of frames to process in offline/loop/webcam mode")
        ->default_val(30);
    app.add_option("--exposure", exposure,
        "Manual V4L2 absolute exposure in 100 µs units (e.g. 333 = 33 ms = 30 fps cap). "
        "-1 = auto (default)")
        ->default_val(-1);

    CLI11_PARSE(app, argc, argv);

    const bool offline_mode = !input_path.empty();

    // ── Resolve pipeline dimensions ───────────────────────────────────────────
    // Offline: read image dimensions from disk without loading pixels.
    // WHY stbi_info before stbi_load: avoids allocating a full-res RGBA buffer
    // just to learn the dimensions — important for 1080p+ images.
    std::vector<uint8_t> bgr_host;  // will be populated in offline or webcam init

    if (offline_mode) {
        int img_w = 0, img_h = 0, img_ch = 0;
        if (!stbi_info(input_path.c_str(), &img_w, &img_h, &img_ch)) {
            throw std::runtime_error("Cannot read image info: " + input_path);
        }
        // Override --width/--height with actual image dimensions.
        width  = img_w;
        height = img_h;

        // Load as RGB (stbi gives RGB), then swap R↔B to produce BGR once on host.
        // WHY swap on host (not GPU): this is a one-time load; the preprocess kernel
        // expects BGR-ordered bytes. Swapping here avoids a conditional in the kernel.
        int loaded_w = 0, loaded_h = 0, loaded_ch = 0;
        bgr_host = load_rgb_image(input_path, loaded_w, loaded_h, loaded_ch);
        // Swap R↔B in place: index 0 = R, index 2 = B in RGB layout.
        for (size_t i = 0; i < bgr_host.size(); i += 3) {
            std::swap(bgr_host[i + 0], bgr_host[i + 2]);  // R↔B → now BGR
        }
    }
    // ── OpenCL context ────────────────────────────────────────────────────────
    auto ocl = create_context();

    // WHY CL_QUEUE_PROFILING_ENABLE: cl::Event timestamps for preprocess and blur.
    cl::CommandQueue queue(ocl.context, ocl.device, CL_QUEUE_PROFILING_ENABLE);

    const std::string bin_dir =
        std::filesystem::path(argv[0]).parent_path().string() + "/";

    // ── Build kernels once (reused every frame) ───────────────────────────────
    auto prog_pre  = build_program(ocl.context, ocl.device,
                                   bin_dir + "kernels/preprocess_nchw.cl");
    auto prog_blur = build_program(ocl.context, ocl.device,
                                   bin_dir + "kernels/bokeh_blur.cl");

    cl::Kernel k_pre (prog_pre,  "preprocess_nchw");
    cl::Kernel k_blur(prog_blur, "bokeh_blur");

    // ── OpenCV webcam init (webcam mode only) ─────────────────────────────────
    // WHY optional: avoids constructing VideoCapture in offline mode where
    // the library might not find a camera and print spurious warnings.
    std::optional<cv::VideoCapture> cap_opt;
    if (!offline_mode) {
        // WHY CAP_V4L2: GStreamer backend ignores CAP_PROP_FOURCC (silently falls
        // back to MJPEG). V4L2 backend supports YUYV natively, eliminating the
        // ~44ms per-frame software MJPEG decode.
        cap_opt.emplace(device_idx, cv::CAP_V4L2);
        cv::VideoCapture& cap = *cap_opt;
        if (!cap.isOpened()) {
            throw std::runtime_error("Cannot open webcam device " +
                                     std::to_string(device_idx));
        }
        // WHY MJPG: YUYV is only available at ~9 fps on this camera (hardware limit).
        // MJPEG runs at 30 fps; decoding happens in the async capture thread
        // (overlapped with GPU work) so it doesn't block the main loop.
        // V4L2 backend is kept (lower overhead than GStreamer for MJPEG decode).
        cap.set(cv::CAP_PROP_FOURCC, cv::VideoWriter::fourcc('M','J','P','G'));
        // WHY no BUFFERSIZE=1 with V4L2: a single DMA buffer stalls the camera
        // after each frame (it must wait for the buffer to be returned), throttling
        // from 30 fps to ~10 fps. The async capture thread drains frames
        // continuously, so the default 4-frame queue doesn't cause stale frames.
        cap.set(cv::CAP_PROP_FPS, 30);

        // Manual exposure — must be set before resolution to avoid driver reset.
        // WHY CAP_PROP_AUTO_EXPOSURE=1: V4L2 encodes exposure mode in V4L2_CID_EXPOSURE_AUTO;
        // value 1 = V4L2_EXPOSURE_MANUAL, value 3 = V4L2_EXPOSURE_APERTURE_PRIORITY (auto).
        // WHY set before resolution: some drivers reset auto-exposure when changing
        // frame size, so the order matters to keep the manual value stable.
        if (exposure > 0) {
            cap.set(cv::CAP_PROP_AUTO_EXPOSURE, 1);   // 1 = manual
            cap.set(cv::CAP_PROP_EXPOSURE, static_cast<double>(exposure));
        }

        // Request the desired resolution; driver may round to nearest supported mode.
        cap.set(cv::CAP_PROP_FRAME_WIDTH,  width);
        cap.set(cv::CAP_PROP_FRAME_HEIGHT, height);

        // Read actual negotiated resolution and pixel format after set().
        width  = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_WIDTH));
        height = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_HEIGHT));
        const int fourcc = static_cast<int>(cap.get(cv::CAP_PROP_FOURCC));
        char fcc[5] = {
            static_cast<char>( fourcc        & 0xFF),
            static_cast<char>((fourcc >>  8) & 0xFF),
            static_cast<char>((fourcc >> 16) & 0xFF),
            static_cast<char>((fourcc >> 24) & 0xFF), '\0'
        };
        const double actual_exposure    = cap.get(cv::CAP_PROP_EXPOSURE);
        const double actual_auto_exp    = cap.get(cv::CAP_PROP_AUTO_EXPOSURE);
        std::cout << "Webcam: " << width << "×" << height
                  << "  format: " << fcc
                  << "  auto_exposure: " << static_cast<int>(actual_auto_exp)
                  << "  exposure: "      << static_cast<int>(actual_exposure)
                  << " (×100µs)\n";

    }

    // Final dimensions now known (offline: stbi_info, webcam: driver negotiation).
    // WHY here: buffers must be sized to actual resolution, not CLI defaults.
    if (static_cast<size_t>(width) * static_cast<size_t>(height) > static_cast<size_t>(INT_MAX)) {
        throw std::runtime_error("Frame too large: pixel count exceeds INT_MAX");
    }
    const size_t bgr_bytes = static_cast<size_t>(width) * height * 3;  // 3 bytes/pixel BGR

    // WHY CL_MEM_READ_WRITE for ALL buffers: OpenVINO GPU plugin rejects
    // CL_MEM_READ_ONLY / CL_MEM_WRITE_ONLY when binding RemoteTensors.
    cl::Buffer bgr_buf(ocl.context, CL_MEM_READ_WRITE, bgr_bytes);
    cl::Buffer out_buf(ocl.context, CL_MEM_READ_WRITE, bgr_bytes);

    // ── OpenVINO pipeline ─────────────────────────────────────────────────────
    ov::Core core;
    std::optional<ov::intel_gpu::ocl::ClContext> remote_ctx_opt;

    try {
        // WHY pass ocl.context: OpenVINO must share our cl_context so that
        // cl::Buffer handles remain valid across both runtimes.
        remote_ctx_opt.emplace(core, ocl.context.get());
        ov::intel_gpu::ocl::ClContext& remote_ctx = *remote_ctx_opt;

        auto model    = core.read_model(model_path);
        auto compiled = core.compile_model(model, remote_ctx);

        // Read input/output shapes dynamically — no hardcoded dimensions.
        auto input_port  = compiled.input();
        auto output_port = compiled.output();

        ov::PartialShape in_ps = input_port.get_partial_shape();
        ov::Shape input_shape  = in_ps.is_static()
            ? in_ps.to_shape()
            : ov::Shape{1, 3, 256, 256};

        ov::PartialShape out_ps = output_port.get_partial_shape();
        ov::Shape output_shape  = out_ps.is_static()
            ? out_ps.to_shape()
            : ov::Shape{1, 1, static_cast<size_t>(input_shape[2]),
                              static_cast<size_t>(input_shape[3])};

        const cl_int model_h = static_cast<cl_int>(input_shape[2]);
        const cl_int model_w = static_cast<cl_int>(input_shape[3]);

        const size_t input_floats =
            static_cast<size_t>(3) * static_cast<size_t>(model_h) * model_w;
        const size_t output_floats = std::accumulate(
            output_shape.begin(), output_shape.end(),
            size_t{1}, std::multiplies<size_t>{});

        // WHY CL_MEM_READ_WRITE: OpenVINO GPU plugin writes here; our kernel reads.
        cl::Buffer nchw_buf(ocl.context, CL_MEM_READ_WRITE,
                            input_floats * sizeof(float));
        cl::Buffer mask_buf(ocl.context, CL_MEM_READ_WRITE,
                            output_floats * sizeof(float));

        // ── Pre-bind output tensor before first infer() ───────────────────────
        // WHY pre-bind: if output is not pre-bound, the GPU plugin returns a host
        // Tensor after infer(); .as<ClBufferTensor>() would then throw.
        // Pre-binding guarantees the plugin writes directly into our cl::Buffer.
        auto req           = compiled.create_infer_request();
        auto input_tensor  = remote_ctx.create_tensor(
            input_port.get_element_type(), input_shape, nchw_buf.get());
        auto output_tensor = remote_ctx.create_tensor(
            output_port.get_element_type(), output_shape, mask_buf.get());

        req.set_input_tensor(input_tensor);
        // WHY set_output_tensor before infer: mandatory pre-binding per task spec.
        req.set_output_tensor(output_tensor);

        // ── Pre-set static kernel args (indices that never change per frame) ──
        // Preprocess kernel: bgr_in(0), in_w(1), in_h(2), nchw_out(3), model_w(4), model_h(5)
        CL_CHECK(k_pre.setArg(0, bgr_buf));
        CL_CHECK(k_pre.setArg(1, static_cast<cl_int>(width)));
        CL_CHECK(k_pre.setArg(2, static_cast<cl_int>(height)));
        CL_CHECK(k_pre.setArg(3, nchw_buf));
        CL_CHECK(k_pre.setArg(4, model_w));
        CL_CHECK(k_pre.setArg(5, model_h));

        // Blur kernel: bgr_in(0), mask(1), width(2), height(3), model_w(4), model_h(5), bgr_out(6)
        CL_CHECK(k_blur.setArg(0, bgr_buf));
        CL_CHECK(k_blur.setArg(1, mask_buf));
        CL_CHECK(k_blur.setArg(2, static_cast<cl_int>(width)));
        CL_CHECK(k_blur.setArg(3, static_cast<cl_int>(height)));
        CL_CHECK(k_blur.setArg(4, model_w));
        CL_CHECK(k_blur.setArg(5, model_h));
        CL_CHECK(k_blur.setArg(6, out_buf));

        // In offline mode: loop_mode means run for `runs` frames; else single frame.
        // In webcam mode: run until 'q'/'ESC' or `runs` frames (if runs > 0).
        const int total_frames = offline_mode ? (loop_mode ? runs : 1) : runs;

        std::vector<double> total_times;  // accumulates frames 2+ for FPS avg
        total_times.reserve(total_frames);

        // ── Async capture thread (webcam mode only) ───────────────────────────
        // WHY thread: overlap camera grab with GPU work. Without it, `cap >> f`
        // blocks ~33 ms per frame (camera interval), serialising capture + GPU.
        // With the thread, the next frame is grabbed while the GPU processes
        // the current one. Per-iteration time → max(GPU_ms, camera_interval_ms).
        std::mutex              cap_mutex;
        std::condition_variable cap_cv;
        cv::Mat                 cap_latest;
        bool                    cap_frame_ready = false;
        std::atomic<bool>       cap_running{true};
        std::thread             cap_thread;

        if (!offline_mode) {
            // TODO: update this pattern!
            cap_thread = std::thread([&] {
                while (cap_running.load(std::memory_order_relaxed)) {
                    cv::Mat f;
                    *cap_opt >> f;
                    {
                        std::lock_guard<std::mutex> lk(cap_mutex);
                        cap_latest      = std::move(f);
                        cap_frame_ready = true;
                    }
                    cap_cv.notify_one();
                    if (cap_latest.empty()) break;  // camera disconnected
                }
            });
        }

        std::cout << std::fixed << std::setprecision(1);

        for (int frame = 1; frame <= total_frames || (!offline_mode); ++frame) {
            const bool is_warmup = (frame == 1);

            // ── 1. Capture ────────────────────────────────────────────────────
            const auto t_cap0 = std::chrono::steady_clock::now();

            if (offline_mode) {
                // Offline: upload pre-loaded BGR host buffer.
                CL_CHECK(queue.enqueueWriteBuffer(
                    bgr_buf, CL_FALSE, 0, bgr_bytes, bgr_host.data()));
                CL_CHECK(queue.finish());
            } else {
                // Pull latest frame from the capture thread.
                // WHY nearly instant: capture thread grabbed the frame while GPU
                // was processing the previous one — we just take the result.
                cv::Mat frame_mat;
                {
                    std::unique_lock<std::mutex> lk(cap_mutex);
                    cap_cv.wait(lk, [&] {
                        return cap_frame_ready || !cap_running.load();
                    });
                    if (!cap_frame_ready) {
                        std::cerr << "Webcam returned empty frame — stopping.\n";
                        break;
                    }
                    frame_mat       = std::move(cap_latest);
                    cap_frame_ready = false;
                }
                if (frame_mat.empty()) {
                    std::cerr << "Webcam returned empty frame — stopping.\n";
                    break;
                }
                // Guard: driver must not change resolution mid-stream.
                const size_t actual_bytes =
                    static_cast<size_t>(frame_mat.cols) * frame_mat.rows * 3;
                if (actual_bytes != bgr_bytes) {
                    throw std::runtime_error(
                        "Webcam frame size changed mid-stream: expected " +
                        std::to_string(bgr_bytes) + " bytes, got " +
                        std::to_string(actual_bytes));
                }
                CL_CHECK(queue.enqueueWriteBuffer(
                    bgr_buf, CL_FALSE, 0, bgr_bytes, frame_mat.data));
                CL_CHECK(queue.finish());
            }

            const auto t_cap1 = std::chrono::steady_clock::now();
            const double cap_ms = std::chrono::duration<double, std::milli>(
                t_cap1 - t_cap0).count();

            // ── 2. Preprocess (cl::Event) ─────────────────────────────────────
            cl::Event ev_pre;
            CL_CHECK(queue.enqueueNDRangeKernel(
                k_pre, cl::NullRange,
                cl::NDRange(static_cast<size_t>(model_w), static_cast<size_t>(model_h)),
                cl::NullRange,
                nullptr, &ev_pre));
            // No finish() here: nchw_buf flows to infer on same in-order queue.

            // ── 3. Inference (steady_clock) ───────────────────────────────────
            // WHY finish before infer: the in-order queue implies ordering, but
            // OpenVINO schedules infer on a separate internal thread. finish()
            // ensures the preprocess kernel has completed before infer() reads
            // nchw_buf — avoids a data race on first call.
            CL_CHECK(queue.finish());
            const auto t_inf0 = std::chrono::steady_clock::now();
            req.infer();
            const auto t_inf1 = std::chrono::steady_clock::now();
            const double inf_ms = std::chrono::duration<double, std::milli>(
                t_inf1 - t_inf0).count();

            const double pre_ms = duration_ms(ev_pre);

            // ── 4. Bokeh blur (cl::Event) ─────────────────────────────────────
            cl::Event ev_blur;
            CL_CHECK(queue.enqueueNDRangeKernel(
                k_blur, cl::NullRange,
                cl::NDRange(static_cast<size_t>(width), static_cast<size_t>(height)),
                cl::NullRange,
                nullptr, &ev_blur));

            // ── 5. Mandatory finish gate ──────────────────────────────────────
            // WHY finish here: ensures blur kernel has written out_buf before we
            // read it back (display step) and before the next frame's capture
            // overwrites bgr_buf. Omitting causes undefined behaviour (§constraints).
            CL_CHECK(queue.finish());
            const double blur_ms = duration_ms(ev_blur);

            // ── 6. Display (steady_clock) ─────────────────────────────────────
            const auto t_disp0 = std::chrono::steady_clock::now();

            if (offline_mode) {
                // Read back and save on last frame to avoid spamming disk.
                const bool save_now = (!loop_mode) || (frame == total_frames);
                if (save_now) {
                    std::vector<uint8_t> blurred_host(bgr_bytes);
                    CL_CHECK(queue.enqueueReadBuffer(
                        out_buf, CL_TRUE, 0, bgr_bytes, blurred_host.data()));
                    // Swap BGR→RGB before stb_image_write so the saved BMP has
                    // correct colors (stb writes RGB order).
                    for (size_t i = 0; i < blurred_host.size(); i += 3) {
                        std::swap(blurred_host[i + 0], blurred_host[i + 2]);
                    }
                    save_bmp("output_blurred.bmp", blurred_host, width, height, 3);
                }
                if (frame >= total_frames) break;
            } else {
                // Webcam: read back into cv::Mat and display via imshow.
                // WHY cv::Mat wrapping: zero-copy display — cv::imshow reads
                // directly from the vector without an extra allocation.
                std::vector<uint8_t> out_host(bgr_bytes);
                CL_CHECK(queue.enqueueReadBuffer(
                    out_buf, CL_TRUE, 0, bgr_bytes, out_host.data()));
                cv::Mat disp(height, width, CV_8UC3, out_host.data());
                cv::imshow("A4 Smart Webcam", disp);
                // WHY waitKey(1): minimum delay needed for imshow to render;
                // check for 'q' (113) or ESC (27) to exit cleanly.
                const int key = cv::waitKey(1);
                if (key == 'q' || key == 27) {
                    std::cout << "\nExit key pressed — stopping.\n";
                    break;
                }
            }

            const auto t_disp1 = std::chrono::steady_clock::now();
            const double disp_ms = std::chrono::duration<double, std::milli>(
                t_disp1 - t_disp0).count();

            const double total_ms = cap_ms + pre_ms + inf_ms + blur_ms + disp_ms;

            // ── Per-frame timing output ───────────────────────────────────────
            if (is_warmup) {
                std::cout << "\nFrame 1 (JIT warm-up — do not measure FPS here):\n";
            } else {
                std::cout << "\nFrame " << frame << " (stable):\n";
            }
            std::cout << "  Capture:    " << std::setw(7) << cap_ms  << " ms\n"
                      << "  Preprocess: " << std::setw(7) << pre_ms  << " ms\n"
                      << "  Inference:  " << std::setw(7) << inf_ms  << " ms\n"
                      << "  Kernel:     " << std::setw(7) << blur_ms << " ms\n"
                      << "  Display:    " << std::setw(7) << disp_ms << " ms\n";

            if (!is_warmup) {
                std::cout << "  Total:      " << std::setw(7) << total_ms << " ms\n";
                total_times.push_back(total_ms);

                const double avg_fps = 1000.0 * static_cast<double>(total_times.size()) /
                    std::accumulate(total_times.begin(), total_times.end(), 0.0);
                std::cout << "FPS avg (frames 2–" << frame << "): "
                          << std::setprecision(1) << avg_fps << "\n";
            }
        }

        // Stop capture thread cleanly before buffers go out of scope.
        cap_running = false;
        cap_cv.notify_all();
        if (cap_thread.joinable()) cap_thread.join();

        if (offline_mode) {
            std::cout << "\nSaved: output_blurred.bmp\n";
        }

    } catch (const std::exception& e) {
        // Graceful fallback: GPU plugin unavailable or model incompatible.
        // Exit 0 signals "skip" to callers (master_specs §4).
        std::cout << "[A4] Pipeline failed: " << e.what()
                  << "\nOpenVINO GPU plugin required. Exiting.\n";
        return 0;
    }

    return 0;
}
