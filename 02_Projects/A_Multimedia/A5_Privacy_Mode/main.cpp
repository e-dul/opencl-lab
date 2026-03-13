// A5_Privacy_Mode — Face detection + ROI blur via two GPU paths.
//
// Pipeline per frame:
//   1. Face detection via cv::FaceDetectorYN (YuNet, DNN_TARGET_OPENCL).
//   2. Path A: T-API blur — cv::blur on cv::UMat submat; zero custom kernel code.
//   3. Path B: Custom kernel — roi_blur.cl with global_work_offset; explicit NDRange.
//
// Core teaching point:
//   FaceDetectorYN + cv::UMat give GPU acceleration without writing a single
//   OpenCL kernel. Path B shows the explicit alternative, letting the student
//   judge the trade-off in lines-of-code vs. flexibility.
//
// WHY cv::ocl::attachContext: T-API must share our cl_context so cv::UMat
// operations dispatch on the same device without creating a second context
// that would force expensive cross-context copies.

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image.h>
#include <stb_image_write.h>
#include "image_utils.hpp"

#include "ocl_wrapper.hpp"    // create_context(), OclContext
#include "opencl_utils.hpp"   // CL_CHECK, load_kernel_source, duration_ms

// OpenCV T-API and DNN — must come after opencl_utils.hpp so cl_context is defined.
#include <opencv2/core.hpp>
#include <opencv2/core/ocl.hpp>      // cv::ocl::attachContext
#include <opencv2/imgproc.hpp>
#include <opencv2/dnn.hpp>
#include <opencv2/objdetect.hpp>  // cv::FaceDetectorYN
#include <opencv2/highgui.hpp>
#include <opencv2/videoio.hpp>
#include <opencv2/imgcodecs.hpp>

#include <CLI/CLI.hpp>

#include <algorithm>
#include <chrono>
#include <climits>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Helper: chrono duration → milliseconds as double.
// ---------------------------------------------------------------------------
static double chrono_ms(std::chrono::steady_clock::time_point t0,
                        std::chrono::steady_clock::time_point t1)
{
    return std::chrono::duration<double, std::milli>(t1 - t0).count();
}

// ---------------------------------------------------------------------------
// Helper: build a cl::Program, printing build log on failure before re-throw.
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

// ---------------------------------------------------------------------------
// Face ROI from FaceDetectorYN output.
// ---------------------------------------------------------------------------
struct FaceRect {
    int x, y, width, height;
};

// ---------------------------------------------------------------------------
// Parse FaceDetectorYN (YuNet) output.
//
// WHY YuNet column layout (cv::FaceDetectorYN docs):
//   Output: cv::Mat [N, 15], each row per detection:
//   [x1, y1, w, h, re_x, re_y, le_x, le_y, nt_x, nt_y,
//    rcm_x, rcm_y, lcm_x, lcm_y, score]
//   Columns 0-3: bounding box in pixel coords (already scaled to input_size).
//   Column 14: confidence score. No normalisation needed.
//
// Returns the highest-confidence detection above conf_threshold,
// or std::nullopt if none found.
// ---------------------------------------------------------------------------
static std::optional<FaceRect> parse_yunet(const cv::Mat& faces,
                                            float conf_threshold,
                                            int frame_w, int frame_h)
{
    if (faces.empty()) return std::nullopt;

    int   best_row   = -1;
    float best_score = conf_threshold;
    for (int i = 0; i < faces.rows; ++i) {
        float score = faces.at<float>(i, 14);
        if (score > best_score) { best_score = score; best_row = i; }
    }
    if (best_row < 0) return std::nullopt;

    int x = static_cast<int>(faces.at<float>(best_row, 0));
    int y = static_cast<int>(faces.at<float>(best_row, 1));
    int w = static_cast<int>(faces.at<float>(best_row, 2));
    int h = static_cast<int>(faces.at<float>(best_row, 3));

    // Clamp to frame bounds.
    x = std::max(0, std::min(x, frame_w - 1));
    y = std::max(0, std::min(y, frame_h - 1));
    w = std::min(w, frame_w - x);
    h = std::min(h, frame_h - y);

    if (w <= 0 || h <= 0) return std::nullopt;
    return FaceRect{x, y, w, h};
}

int main(int argc, char* argv[]) try {
    // ── CLI11 ─────────────────────────────────────────────────────────────────
    CLI::App app{"A5 Privacy Mode — Face detection + ROI blur (T-API vs custom kernel)"};

    std::string input_path;
    int         device_idx     = 0;
    int         cap_width      = 640;
    int         cap_height     = 480;
    std::string output_path    = "output.bmp";
    std::string face_model     = "assets/face_detection_yunet_2023mar.onnx";
    float       conf_threshold = 0.6f;
    int         blur_radius    = 15;
    bool        loop_mode      = false;
    int         exposure       = -1;

    auto* opt_input  = app.add_option("--input",  input_path,  "Input image file (mutually exclusive with --device)");
    auto* opt_device = app.add_option("--device", device_idx,  "Webcam device index (mutually exclusive with --input)")
        ->default_val(0);
    opt_input->excludes(opt_device);
    opt_device->excludes(opt_input);

    app.add_option("--width",          cap_width,      "Webcam capture width (webcam mode only)")  ->default_val(640);
    app.add_option("--height",         cap_height,     "Webcam capture height (webcam mode only)") ->default_val(480);
    app.add_option("--output",         output_path,    "Output BMP path (file mode only)")         ->default_val("output.bmp");
    app.add_option("--face-model",     face_model,     "Path to YuNet ONNX model") ->default_val("assets/face_detection_yunet_2023mar.onnx");
    app.add_option("--conf-threshold", conf_threshold, "Detection confidence threshold")            ->default_val(0.6f);
    app.add_option("--blur-radius",    blur_radius,    "Box blur size for both paths")              ->default_val(15);
    app.add_flag  ("--loop",           loop_mode,      "Replay --input in a live preview window (requires --input)");
    app.add_option("--exposure",       exposure,
        "Manual V4L2 absolute exposure (100 µs units). Webcam only. -1 = auto.")
        ->default_val(-1);

    CLI11_PARSE(app, argc, argv);

    // One of --input or --device must be given; neither is an error.
    const bool has_input  = !input_path.empty();
    const bool has_device = (opt_device->count() > 0);  // true if user explicitly passed --device
    if (!has_input && !has_device) {
        throw std::runtime_error("One of --input or --device must be provided.");
    }

    const bool file_mode = has_input;

    // ── OpenCL / T-API availability guards ───────────────────────────────────
    if (!cv::ocl::haveOpenCL()) {
        std::cout << "[A5] OpenCV was not built with OpenCL support. Exiting.\n";
        return 0;
    }

    // ── OpenCL context ────────────────────────────────────────────────────────
    OclContext ocl = create_context();

    // WHY CL_QUEUE_PROFILING_ENABLE: required to read cl::Event timestamps for
    // Path B kernel timing. T-API uses its own internal queue; Path A is synced
    // via cv::ocl::finish() (not this queue) before the chrono timer stops.
    cl::CommandQueue queue(ocl.context, ocl.device, CL_QUEUE_PROFILING_ENABLE);
    // Replace ocl.queue with profiling-enabled queue for all CL_CHECK calls below.
    // WHY not modify OclContext: OclContext is a value type with no virtual dtor;
    // we keep the profiling queue local and use it directly.

    // WHY attachContext: T-API must share our cl_context so cv::UMat operations
    // dispatch on the same device without creating a second context.
    // WHY operator(): cl::Platform::operator() returns the raw cl_platform_id handle.
    // .get() in this wrapper returns the wrapper object itself, not the raw pointer.
    cv::ocl::attachContext(
        ocl.platform.getInfo<CL_PLATFORM_NAME>(),
        static_cast<void*>(ocl.platform()),
        static_cast<void*>(ocl.context()),
        static_cast<void*>(ocl.device()));

    // ── Build custom blur kernel ──────────────────────────────────────────────
    const std::string bin_dir =
        std::filesystem::path(argv[0]).parent_path().string() + "/";

    auto prog_blur = build_program(ocl.context, ocl.device,
                                   bin_dir + "kernels/roi_blur.cl");
    cl::Kernel k_roi(prog_blur, "roi_blur");

    // ── Webcam / file capture setup ───────────────────────────────────────────
    std::optional<cv::VideoCapture> cap_opt;
    int frame_w = cap_width;
    int frame_h = cap_height;
    cv::Mat file_frame;

    if (file_mode) {
        // Load image via OpenCV (handles BGR natively).
        file_frame = cv::imread(input_path);
        if (file_frame.empty()) {
            throw std::runtime_error("Failed to load image: " + input_path);
        }
        frame_w = file_frame.cols;
        frame_h = file_frame.rows;
    } else {
        cap_opt.emplace(device_idx, cv::CAP_V4L2);
        cv::VideoCapture& cap = *cap_opt;
        if (!cap.isOpened()) {
            throw std::runtime_error("Cannot open webcam device " +
                                     std::to_string(device_idx));
        }
        cap.set(cv::CAP_PROP_FOURCC, cv::VideoWriter::fourcc('M','J','P','G'));
        cap.set(cv::CAP_PROP_FPS, 30);

        // WHY set exposure before resolution: some drivers reset auto-exposure
        // when frame size changes; setting it first keeps the manual value stable.
        if (exposure >= 0) {
            cap.set(cv::CAP_PROP_AUTO_EXPOSURE, 1);  // 1 = manual (V4L2_EXPOSURE_MANUAL)
            cap.set(cv::CAP_PROP_EXPOSURE, static_cast<double>(exposure));
        }
        cap.set(cv::CAP_PROP_FRAME_WIDTH,  cap_width);
        cap.set(cv::CAP_PROP_FRAME_HEIGHT, cap_height);

        frame_w = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_WIDTH));
        frame_h = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_HEIGHT));
        std::cout << "Webcam: " << frame_w << "×" << frame_h << "\n";
    }

    // Integer safety (§7.1): promote int before multiplying buffer size.
    if (static_cast<size_t>(frame_w) * static_cast<size_t>(frame_h) >
            static_cast<size_t>(INT_MAX)) {
        throw std::runtime_error("Frame too large: pixel count exceeds INT_MAX");
    }
    const size_t bgr_bytes = static_cast<size_t>(frame_w) * frame_h * 3;

    // GPU buffer for Path B: kernel operates in-place on this buffer.
    // WHY CL_MEM_READ_WRITE: kernel reads neighbours and writes the blurred pixel.
    cl::Buffer bgr_buf(ocl.context, CL_MEM_READ_WRITE, bgr_bytes);

    // ── Face detector (YuNet via FaceDetectorYN) ─────────────────────────────
    // WHY FaceDetectorYN: canonical OpenCV API for YuNet; handles pre/post-
    // processing internally and supports DNN_TARGET_OPENCL for GPU inference
    // without writing a single OpenCL kernel — the core teaching point of Path A.
    // WHY 320×320 fixed input_size: YuNet requires sizes that are multiples of 32
    // (stride of the skip-connection). Arbitrary frame sizes cause shape mismatch
    // in the Eltwise layer of OpenCV 4.6.0. FaceDetectorYN rescales the input frame
    // to input_size internally, then scales bbox coords back to the original frame
    // resolution — so detection output is always in display frame coordinates.
    static constexpr int DET_W = 320;
    static constexpr int DET_H = 320;
    cv::Ptr<cv::FaceDetectorYN> detector;
    bool detector_on_cpu = false;
    try {
        detector = cv::FaceDetectorYN::create(
            face_model, "",
            cv::Size(DET_W, DET_H),
            conf_threshold, 0.3f, 5000,
            cv::dnn::DNN_BACKEND_OPENCV,
            cv::dnn::DNN_TARGET_OPENCL);
    } catch (const cv::Exception& e) {
        throw std::runtime_error(std::string("FaceDetectorYN init failed: ") + e.what());
    }

    // ── Main loop ─────────────────────────────────────────────────────────────
    std::cout << std::fixed << std::setprecision(3);

    auto last_no_face_log = std::chrono::steady_clock::now() - std::chrono::seconds(2);

    int frame_num = 0;
    bool running  = true;

    while (running) {
        ++frame_num;
        const bool is_warmup = (frame_num == 1);

        // ── Acquire frame ─────────────────────────────────────────────────────
        cv::Mat frame_bgr;
        if (file_mode) {
            frame_bgr = file_frame.clone();
        } else {
            *cap_opt >> frame_bgr;
            if (frame_bgr.empty()) {
                std::cerr << "Webcam returned empty frame — stopping.\n";
                break;
            }
        }

        // ── Face detection (YuNet, DNN_TARGET_OPENCL) ────────────────────────
        // WHY resize to DET_W×DET_H: FaceDetectorYN asserts that the input image
        // matches input_size exactly; it does NOT auto-resize. After detection,
        // bbox coords (cols 0-3) are in 320×320 space and must be scaled back.
        cv::Mat frame_det;
        cv::resize(frame_bgr, frame_det, cv::Size(DET_W, DET_H));

        const auto t_det0 = std::chrono::steady_clock::now();
        cv::Mat faces;
        try {
            detector->detect(frame_det, faces);
        } catch (const std::exception& e) {
            // WHY catch std::exception: ocl4dnn JIT failures surface as cv::Exception
            // (not cl::Error) with messages like "Layer with requested id=-1".
            // cl::Error also derives from std::exception, so both are caught here.
            // Graceful CPU fallback; detector is re-created once and reused.
            if (!detector_on_cpu) {
                std::cerr << "[A5] detect() failed (" << e.what()
                          << "). Falling back to CPU.\n";
                detector = cv::FaceDetectorYN::create(
                    face_model, "",
                    cv::Size(DET_W, DET_H),
                    conf_threshold, 0.3f, 5000,
                    cv::dnn::DNN_BACKEND_OPENCV,
                    cv::dnn::DNN_TARGET_CPU);
                detector_on_cpu = true;
                detector->detect(frame_det, faces);
            } else {
                throw std::runtime_error(std::string("detect() failed on CPU: ") + e.what());
            }
        }
        const auto t_det1 = std::chrono::steady_clock::now();
        const double det_ms = chrono_ms(t_det0, t_det1);

        // Scale bbox coords from DET_W×DET_H to display frame resolution.
        if (!faces.empty()) {
            const float sx = static_cast<float>(frame_w) / DET_W;
            const float sy = static_cast<float>(frame_h) / DET_H;
            for (int i = 0; i < faces.rows; ++i) {
                faces.at<float>(i, 0) *= sx;
                faces.at<float>(i, 1) *= sy;
                faces.at<float>(i, 2) *= sx;
                faces.at<float>(i, 3) *= sy;
            }
        }

        // ── Parse detection ───────────────────────────────────────────────────
        auto face_opt = parse_yunet(faces, conf_threshold, frame_w, frame_h);

        if (!face_opt) {
            // Rate-limit "No face detected" log to once/second.
            auto now = std::chrono::steady_clock::now();
            if (chrono_ms(last_no_face_log, now) >= 1000.0) {
                std::cout << "No face detected\n";
                last_no_face_log = now;
            }

            // Show unmodified frame and continue.
            if (!file_mode || loop_mode) {
                cv::imshow("A5 Privacy Mode", frame_bgr);
                const int key = cv::waitKey(file_mode ? 30 : 1);
                if (key == 'q' || key == 27) break;
            } else {
                // File mode, no loop: save unmodified and exit.
                cv::imwrite(output_path, frame_bgr);
                std::cout << "Saved (no face): " << output_path << "\n";
                break;
            }
            continue;
        }

        const FaceRect& face = *face_opt;

        // Save original (pre-blur) for Path B fair comparison.
        // WHY clone here: frame_bgr is modified in-place by Path A below;
        // Path B must start from the same unmodified frame.
        cv::Mat frame_bgr_orig = frame_bgr.clone();

        // ── Path A: T-API blur ────────────────────────────────────────────────
        // cv::UMat shares the GPU context attached above; no extra buffer upload.
        cv::UMat frame_umat;
        frame_bgr.copyTo(frame_umat);  // uploads once per frame to GPU memory

        cv::Rect roi_rect(face.x, face.y, face.width, face.height);
        cv::UMat roi_view = frame_umat(roi_rect);

        const auto t_tapi0 = std::chrono::steady_clock::now();
        cv::blur(roi_view, roi_view, cv::Size(blur_radius, blur_radius));
        // WHY cv::ocl::finish(): T-API enqueues work on OpenCV's internal queue,
        // NOT on our local `queue` object. cv::ocl::finish() flushes the T-API
        // queue so the chrono timer stops after the GPU work is actually complete.
        cv::ocl::finish();
        const auto t_tapi1 = std::chrono::steady_clock::now();
        const double tapi_ms = chrono_ms(t_tapi0, t_tapi1);

        frame_umat.copyTo(frame_bgr);  // download T-API result for display

        // ── Path B: custom kernel with global_work_offset ─────────────────────
        // WHY frame_bgr_orig: Path A blurred frame_bgr in-place; Path B must
        // operate on the same pre-blur input for a fair visual comparison.
        // frame_bgr_orig was captured before Path A for both file and webcam modes.
        const cv::Mat& b_src = frame_bgr_orig;

        CL_CHECK(queue.enqueueWriteBuffer(
            bgr_buf, CL_TRUE, 0, bgr_bytes, b_src.data));

        // Set kernel args for this frame.
        CL_CHECK(k_roi.setArg(0, bgr_buf));
        CL_CHECK(k_roi.setArg(1, static_cast<cl_int>(frame_w)));
        CL_CHECK(k_roi.setArg(2, static_cast<cl_int>(frame_h)));
        CL_CHECK(k_roi.setArg(3, static_cast<cl_int>(blur_radius)));

        // WHY global_work_offset: work-item IDs start at (face.x, face.y) —
        // no offset arithmetic needed inside the kernel body.
        cl::NDRange offset(static_cast<size_t>(face.x),
                           static_cast<size_t>(face.y));
        cl::NDRange gws(static_cast<size_t>(face.width),
                        static_cast<size_t>(face.height));
        cl::Event ev;
        CL_CHECK(queue.enqueueNDRangeKernel(k_roi, offset, gws, cl::NullRange,
                                             nullptr, &ev));
        CL_CHECK(ev.wait());
        const double kernel_ms = (ev.getProfilingInfo<CL_PROFILING_COMMAND_END>() -
                                   ev.getProfilingInfo<CL_PROFILING_COMMAND_START>()) * 1e-6;

        // ── Per-frame console output ───────────────────────────────────────────
        if (is_warmup) {
            std::cout << "\nFrame " << frame_num
                      << " (JIT warm-up — exclude from averages):\n";
        } else {
            std::cout << "\nFrame " << frame_num << ":\n";
        }
        std::cout << "  Detection:   " << std::setw(8) << det_ms
                  << " ms   (DNN_TARGET_OPENCL, std::chrono)\n"
                  << "  Blur T-API:  " << std::setw(8) << tapi_ms
                  << " ms   (cv::blur on cv::UMat)\n"
                  << "  Blur Kernel: " << std::setw(8) << kernel_ms
                  << " ms   (roi_blur.cl, cl::Event)\n";

        // ── Display / save ────────────────────────────────────────────────────
        // Display the T-API result (Path A is the primary output).
        cv::rectangle(frame_bgr,
                      cv::Point(face.x, face.y),
                      cv::Point(face.x + face.width, face.y + face.height),
                      cv::Scalar(0, 255, 0), 2);

        if (!file_mode || loop_mode) {
            cv::imshow("A5 Privacy Mode", frame_bgr);
            const int key = cv::waitKey(file_mode ? 30 : 1);
            if (key == 'q' || key == 27) {
                running = false;
            }
            if (file_mode && loop_mode) {
                // Keep looping until user quits.
                if (!running) break;
                continue;
            } else if (!file_mode) {
                // Webcam: keep running.
                continue;
            }
        }

        // File mode, no loop: save on first (only) frame and exit.
        // Convert BGR→RGB for stbi output.
        cv::Mat rgb_out;
        cv::cvtColor(frame_bgr, rgb_out, cv::COLOR_BGR2RGB);
        std::vector<uint8_t> rgb_data(rgb_out.data,
            rgb_out.data + static_cast<size_t>(frame_w) * static_cast<size_t>(frame_h) * 3);
        save_bmp(output_path, rgb_data, frame_w, frame_h, 3);
        std::cout << "Saved: " << output_path << "\n";
        break;
    }

    return 0;
} catch (const std::exception& e) {
    std::cerr << "[A5] Fatal: " << e.what() << "\n";
    return 1;
}
