// main.cpp — FFmpeg OpenCL Transcoder
//
// Pipeline per frame (zero-copy path when cl_intel_va_api_media_sharing available):
//   Decode (VAAPI) → import VAAPI surface as CL image (zero-copy)
//                  → NV12→RGBA kernel → Filter kernel → Re-encode (h264_vaapi)
//
// Fallback (CPU copy path):
//   Decode (VAAPI/NVDEC or SW) → av_hwframe_transfer_data → sws_scale → enqueueWriteImage
//                              → Filter kernel → Re-encode
//
// Per-frame timing:
//   - Decode / Encode: std::chrono::steady_clock (FFmpeg stages are CPU-bound)
//   - Map / Filter:    cl::Event profiling (GPU stages)

// WHY #ifndef guards: CMakeLists.txt passes -DCL_HPP_* via compile definitions;
// guards prevent "macro redefined" warnings when opencl_utils.hpp also defines them.
#ifndef CL_HPP_ENABLE_EXCEPTIONS
#define CL_HPP_ENABLE_EXCEPTIONS
#endif
#ifndef CL_HPP_TARGET_OPENCL_VERSION
#define CL_HPP_TARGET_OPENCL_VERSION  120
#endif
#ifndef CL_HPP_MINIMUM_OPENCL_VERSION
#define CL_HPP_MINIMUM_OPENCL_VERSION 120
#endif

#include "ocl_wrapper.hpp"   // create_context(), OclContext; also pulls in opencl_utils.hpp
// opencl_utils.hpp is included transitively — do not include again to avoid duplicate symbols

#include <CLI/CLI.hpp>

extern "C" {
#include <va/va.h>                       // VASurfaceID, VADisplay
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/hwcontext.h>
#include <libavutil/hwcontext_vaapi.h>   // AVVAAPIDeviceContext
#include <libavutil/avutil.h>
#include <libswscale/swscale.h>
}

// WHY CL_NO_NON_ICD_DISPATCH_EXTENSION_PROTOTYPES: the Intel VAAPI sharing functions
// (clCreateFromVA_APIMediaSurfaceINTEL etc.) are NOT in the ICD dispatch table and
// therefore not exported by libOpenCL.so — linking against the declarations would
// produce undefined-reference errors. We suppress them and load via
// clGetExtensionFunctionAddressForPlatform at runtime instead.
#define CL_NO_NON_ICD_DISPATCH_EXTENSION_PROTOTYPES
#include <CL/cl_va_api_media_sharing_intel.h>

#include <array>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;

using Clock = std::chrono::steady_clock;
using Ms    = std::chrono::duration<double, std::milli>;

// NV12 plane indices: Y (luma) is plane 0, UV (interleaved chroma) is plane 1.
// These map directly to the plane_index argument of clCreateFromVA_APIMediaSurfaceINTEL.
static constexpr int Y_PLANE  = 0;
static constexpr int UV_PLANE = 1;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Case-sensitive substring search in the device extension string.
static bool device_has_ext(const cl::Device& dev, const std::string& ext)
{
    return dev.getInfo<CL_DEVICE_EXTENSIONS>().find(ext) != std::string::npos;
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

static int run(int argc, char** argv);

int main(int argc, char** argv)
{
    try {
        return run(argc, argv);
    } catch (const std::exception& e) {
        std::cerr << "[ERROR] " << e.what() << "\n";
        return 1;
    }
}

static int run(int argc, char** argv)
{
    // ── CLI ──────────────────────────────────────────────────────────────────
    CLI::App app{"FFmpeg OpenCL Transcoder — HW decode → OpenCL filter → re-encode"};

    std::string input_path;
    std::string output_path = "filtered.mp4";
    std::string effect      = "blur";

    app.add_option("--input",  input_path,  "Input video file (.mp4)")->required();
    app.add_option("--output", output_path, "Output video file")->default_str("filtered.mp4");
    app.add_option("--effect", effect,      "Visual effect: blur | sepia")
        ->default_str("blur")
        ->check(CLI::IsMember({"blur", "sepia"}));

    CLI11_PARSE(app, argc, argv);

    // ── Asset guard ──────────────────────────────────────────────────────────
    if (!fs::exists(input_path))
        throw std::runtime_error("Input file not found: " + input_path);

    // ── OpenCL device selection (context created later after VAAPI init) ────────
    OclContext ocl = create_context();

    // ── Hardware interop detection ───────────────────────────────────────────
    // cl_intel_va_api_media_sharing → Intel/AMD VAAPI zero-copy (NV12 surfaces ↔ CL images)
    bool hw_interop = device_has_ext(ocl.device, "cl_intel_va_api_media_sharing");

    // ── VAAPI device (shared for HW decoder + encoder) ───────────────────────
    // WHY shared: h264_vaapi decoder, h264_vaapi encoder, and CL interop context
    // all need the same VADisplay; creating it once avoids double driver init.
    AVBufferRef* vaapi_dev_ctx = nullptr;
    if (av_hwdevice_ctx_create(&vaapi_dev_ctx, AV_HWDEVICE_TYPE_VAAPI,
                               nullptr, nullptr, 0) < 0) {
        std::cout << "[INFO] VAAPI device init failed; HW codec paths disabled.\n";
        vaapi_dev_ctx = nullptr;
        hw_interop = false;
    }

    // ── OpenCL context (with VAAPI interop properties when available) ─────────
    // WHY rebuild context: clCreateContext must receive CL_CONTEXT_VA_API_DISPLAY_INTEL
    // at creation time — it cannot be added to an existing context.
    if (hw_interop && vaapi_dev_ctx) {
        auto* hw_av  = reinterpret_cast<AVHWDeviceContext*>(vaapi_dev_ctx->data);
        auto* va_av  = reinterpret_cast<AVVAAPIDeviceContext*>(hw_av->hwctx);

        cl_context_properties props[] = {
            CL_CONTEXT_PLATFORM,           (cl_context_properties)ocl.platform(),
            CL_CONTEXT_VA_API_DISPLAY_INTEL,(cl_context_properties)va_av->display,
            CL_CONTEXT_INTEROP_USER_SYNC,   CL_FALSE,
            0
        };
        cl_int err = CL_SUCCESS;
        cl_context raw = clCreateContext(props, 1, &ocl.device(), nullptr, nullptr, &err);
        if (err == CL_SUCCESS) {
            // WHY retainObject=false: clCreateContext returns refcount=1; the
            // cl::Context wrapper will release it on destruction — no extra retain.
            ocl.context = cl::Context(raw, false);
            std::cout << "[INFO] CL-VAAPI interop context: zero-copy enabled.\n";
        } else {
            std::cout << "[INFO] CL-VAAPI interop context failed (err=" << err
                      << "); using copy path.\n";
            hw_interop = false;
        }
    }
    // ── Load VAAPI-sharing extension function pointers ────────────────────────
    // WHY runtime load: these symbols are not in the ICD dispatch table and are
    // absent from libOpenCL.so; clGetExtensionFunctionAddressForPlatform resolves
    // them through the platform-specific ICD at runtime.
    clCreateFromVA_APIMediaSurfaceINTEL_fn    clCreateFromVA_surf   = nullptr;
    clEnqueueAcquireVA_APIMediaSurfacesINTEL_fn clAcquireVA_surfs   = nullptr;
    clEnqueueReleaseVA_APIMediaSurfacesINTEL_fn clReleaseVA_surfs   = nullptr;

    if (hw_interop) {
        clCreateFromVA_surf = reinterpret_cast<clCreateFromVA_APIMediaSurfaceINTEL_fn>(
            clGetExtensionFunctionAddressForPlatform(
                ocl.platform(), "clCreateFromVA_APIMediaSurfaceINTEL"));
        clAcquireVA_surfs = reinterpret_cast<clEnqueueAcquireVA_APIMediaSurfacesINTEL_fn>(
            clGetExtensionFunctionAddressForPlatform(
                ocl.platform(), "clEnqueueAcquireVA_APIMediaSurfacesINTEL"));
        clReleaseVA_surfs = reinterpret_cast<clEnqueueReleaseVA_APIMediaSurfacesINTEL_fn>(
            clGetExtensionFunctionAddressForPlatform(
                ocl.platform(), "clEnqueueReleaseVA_APIMediaSurfacesINTEL"));

        if (!clCreateFromVA_surf || !clAcquireVA_surfs || !clReleaseVA_surfs) {
            std::cout << "[INFO] VAAPI sharing entry points not found; "
                         "falling back to copy path.\n";
            hw_interop = false;
        }
    }

    if (!hw_interop)
        std::cout << "[INFO] Using software copy path (VAAPI→CPU→CL).\n";

    // WHY separate profiling queue: must be created AFTER the final context is set;
    // CL_QUEUE_PROFILING_ENABLE must be passed at queue creation time.
    cl::CommandQueue prof_queue(ocl.context, ocl.device, CL_QUEUE_PROFILING_ENABLE);

    // ── Build kernels ─────────────────────────────────────────────────────────
    // WHY get_binary_dir(): the binary may be invoked from any working directory;
    // cmake's copy_kernels() places .cl files next to the executable, so the exe
    // path is the only reliable anchor for finding them at runtime.
    const fs::path kernel_dir = get_binary_dir() / "kernels";

    // filter.cl — blur or sepia on RGBA images.
    // WHY pass build_opts: the same source file implements both effects via
    // preprocessor macros; the build option selects the active code path.
    std::string build_opts = (effect == "sepia") ? "-D EFFECT_SEPIA" : "-D EFFECT_BLUR";
    cl::Program program    = build_program(ocl.context, ocl.device,
                                           (kernel_dir / "filter.cl").string(),
                                           build_opts);
    cl::Kernel kernel(program, "apply_filter");

    // nv12_to_rgba.cl — used by both the zero-copy VA path and the GPU-assisted SW map path.
    cl::Program nv12_prog = build_program(ocl.context, ocl.device,
                                          (kernel_dir / "nv12_to_rgba.cl").string());
    cl::Kernel nv12_kernel(nv12_prog, "nv12_to_rgba");

    // rgba_to_nv12.cl — used by both the zero-copy VA encode path and the GPU-assisted SW encode path.
    cl::Program rgba_nv12_prog = build_program(ocl.context, ocl.device,
                                               (kernel_dir / "rgba_to_nv12.cl").string());
    cl::Kernel rgba_nv12_y_kernel(rgba_nv12_prog, "rgba_to_nv12_y");
    cl::Kernel rgba_nv12_uv_kernel(rgba_nv12_prog, "rgba_to_nv12_uv");

    // ── FFmpeg: open input ───────────────────────────────────────────────────
    AVFormatContext* fmt_ctx = nullptr;
    if (avformat_open_input(&fmt_ctx, input_path.c_str(), nullptr, nullptr) < 0)
        throw std::runtime_error("avformat_open_input failed for: " + input_path);

    if (avformat_find_stream_info(fmt_ctx, nullptr) < 0)
        throw std::runtime_error("avformat_find_stream_info failed");

    int video_idx = -1;
    for (unsigned i = 0; i < fmt_ctx->nb_streams; ++i) {
        if (fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            video_idx = static_cast<int>(i);
            break;
        }
    }
    if (video_idx < 0)
        throw std::runtime_error("No video stream in: " + input_path);

    AVStream*          video_stream = fmt_ctx->streams[video_idx];
    AVCodecParameters* codecpar     = video_stream->codecpar;

    // ── FFmpeg: decoder selection ────────────────────────────────────────────
    // VAAPI: use the standard named decoder (e.g. "h264") but attach a
    //   hw_device_ctx + get_format callback — no separate vaapi codec exists.
    // NVDEC: use the named cuvid codec which requires a CUDA device ctx.
    const AVCodec* decoder  = nullptr;
    bool           using_hw = false;

    if (vaapi_dev_ctx) {
        decoder  = avcodec_find_decoder(codecpar->codec_id);
        using_hw = (decoder != nullptr);
    } else {
        if (codecpar->codec_id == AV_CODEC_ID_H264)
            decoder = avcodec_find_decoder_by_name("h264_cuvid");
        else if (codecpar->codec_id == AV_CODEC_ID_HEVC)
            decoder = avcodec_find_decoder_by_name("hevc_cuvid");
        if (decoder) using_hw = true;
    }

    if (!decoder) {
        decoder  = avcodec_find_decoder(codecpar->codec_id);
        using_hw = false;
    }
    if (!decoder)
        throw std::runtime_error("No decoder found for codec id: "
                                 + std::to_string(codecpar->codec_id));

    // Allocate and open decoder context; if HW open fails, retry with SW.
    AVCodecContext* dec_ctx = avcodec_alloc_context3(decoder);
    if (!dec_ctx) throw std::runtime_error("avcodec_alloc_context3 failed (decoder)");

    if (avcodec_parameters_to_context(dec_ctx, codecpar) < 0)
        throw std::runtime_error("avcodec_parameters_to_context failed (decoder)");

    // Inject hw acceleration for the decoder.
    if (using_hw) {
        if (vaapi_dev_ctx) {
            // WHY get_format callback: without it the decoder ignores hw_device_ctx
            // and silently outputs SW pixel formats even with a VAAPI device set.
            dec_ctx->hw_device_ctx = av_buffer_ref(vaapi_dev_ctx);
            dec_ctx->get_format = [](AVCodecContext*, const AVPixelFormat* fmts) -> AVPixelFormat {
                for (const AVPixelFormat* f = fmts; *f != AV_PIX_FMT_NONE; ++f)
                    if (*f == AV_PIX_FMT_VAAPI) return AV_PIX_FMT_VAAPI;
                return fmts[0];
            };
        } else {
            // cuvid path: needs a CUDA device context
            AVBufferRef* cuda_dev = nullptr;
            if (av_hwdevice_ctx_create(&cuda_dev, AV_HWDEVICE_TYPE_CUDA,
                                       nullptr, nullptr, 0) >= 0) {
                dec_ctx->hw_device_ctx = av_buffer_ref(cuda_dev);
                av_buffer_unref(&cuda_dev);
            } else {
                std::cout << "[WARN] Could not create CUDA hw device context\n";
            }
        }
    }

    if (avcodec_open2(dec_ctx, decoder, nullptr) < 0) {
        if (using_hw) {
            // HW decoder failed — fall back to software.
            avcodec_free_context(&dec_ctx);
            decoder  = avcodec_find_decoder(codecpar->codec_id);
            using_hw = false;
            dec_ctx  = avcodec_alloc_context3(decoder);
            if (avcodec_parameters_to_context(dec_ctx, codecpar) < 0)
                throw std::runtime_error("avcodec_parameters_to_context failed (SW fallback)");
            if (avcodec_open2(dec_ctx, decoder, nullptr) < 0)
                throw std::runtime_error("Failed to open SW fallback decoder");
            std::cout << "[INFO] HW decoder open failed; fell back to: "
                      << decoder->name << "\n";
        } else {
            throw std::runtime_error("Failed to open decoder: "
                                     + std::string(decoder->name));
        }
    }

    std::cout << "[INFO] Decoder: " << decoder->name
              << (using_hw ? " (hardware)" : " (software)") << "\n";

    const int frame_w = dec_ctx->width;
    const int frame_h = dec_ctx->height;

    // ── FFmpeg: encoder / output context ────────────────────────────────────
    AVFormatContext* out_ctx = nullptr;
    avformat_alloc_output_context2(&out_ctx, nullptr, nullptr, output_path.c_str());
    if (!out_ctx) throw std::runtime_error("Failed to alloc output format context");

    // Prefer h264_vaapi when a VAAPI device is available; libx264 as fallback.
    const AVCodec* encoder        = nullptr;
    bool           enc_using_vaapi = false;
    bool           enc_using_nvenc = false;

    if (vaapi_dev_ctx)
        encoder = avcodec_find_encoder_by_name("h264_vaapi");
    if (encoder) {
        enc_using_vaapi = true;
    } else {
        // Try NVIDIA hardware encoder before SW fallback — independent of interop.
        encoder = avcodec_find_encoder_by_name("h264_nvenc");
        if (encoder)
            enc_using_nvenc = true;
        else {
            encoder = avcodec_find_encoder_by_name("libx264");
            if (!encoder) encoder = avcodec_find_encoder(AV_CODEC_ID_H264);
        }
    }
    if (!encoder) throw std::runtime_error("No H.264 encoder found");

    AVStream* out_stream = avformat_new_stream(out_ctx, nullptr);
    if (!out_stream) throw std::runtime_error("avformat_new_stream failed");

    AVCodecContext* enc_ctx = avcodec_alloc_context3(encoder);
    if (!enc_ctx) throw std::runtime_error("avcodec_alloc_context3 failed (encoder)");

    enc_ctx->width        = frame_w;
    enc_ctx->height       = frame_h;
    // WHY: VAAPI encoder needs AV_PIX_FMT_VAAPI surfaces; NVENC and SW both take YUV420P.
    enc_ctx->pix_fmt      = enc_using_vaapi ? AV_PIX_FMT_VAAPI : AV_PIX_FMT_YUV420P;
    enc_ctx->time_base    = {1, 25};
    enc_ctx->bit_rate     = 2'000'000;
    enc_ctx->gop_size     = 12;
    enc_ctx->max_b_frames = 0; // WHY 0: avoids packet reordering complexity at flush

    if (enc_using_vaapi) {
        // WHY hw_frames_ctx: h264_vaapi requires frames in VAAPI memory.
        // The frames context wraps the VAAPI device and defines the NV12-backed
        // surface pool that the encoder reads from.
        AVBufferRef* hw_frames_ref = av_hwframe_ctx_alloc(vaapi_dev_ctx);
        if (!hw_frames_ref) throw std::runtime_error("av_hwframe_ctx_alloc failed");
        auto* fctx              = reinterpret_cast<AVHWFramesContext*>(hw_frames_ref->data);
        fctx->format            = AV_PIX_FMT_VAAPI;
        fctx->sw_format         = AV_PIX_FMT_NV12;
        fctx->width             = frame_w;
        fctx->height            = frame_h;
        fctx->initial_pool_size = 20;
        if (av_hwframe_ctx_init(hw_frames_ref) < 0) {
            av_buffer_unref(&hw_frames_ref);
            throw std::runtime_error("av_hwframe_ctx_init failed");
        }
        enc_ctx->hw_frames_ctx = av_buffer_ref(hw_frames_ref);
        av_buffer_unref(&hw_frames_ref);
    }

    if (out_ctx->oformat->flags & AVFMT_GLOBALHEADER)
        enc_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    if (avcodec_open2(enc_ctx, encoder, nullptr) < 0) {
        if (enc_using_vaapi) {
            // VAAPI encoder open failed — fall back to libx264.
            avcodec_free_context(&enc_ctx);
            enc_using_vaapi = false;
            encoder = avcodec_find_encoder_by_name("libx264");
            if (!encoder) encoder = avcodec_find_encoder(AV_CODEC_ID_H264);
            if (!encoder) throw std::runtime_error("No SW H.264 encoder found");
            enc_ctx = avcodec_alloc_context3(encoder);
            if (!enc_ctx) throw std::runtime_error("avcodec_alloc_context3 failed (encoder SW fallback)");
            enc_ctx->width        = frame_w;
            enc_ctx->height       = frame_h;
            enc_ctx->pix_fmt      = AV_PIX_FMT_NV12;   // matches rgba_to_nv12 kernel output
            enc_ctx->time_base    = {1, 25};
            enc_ctx->bit_rate     = 2'000'000;
            enc_ctx->gop_size     = 12;
            enc_ctx->max_b_frames = 0;
            if (out_ctx->oformat->flags & AVFMT_GLOBALHEADER)
                enc_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
            if (avcodec_open2(enc_ctx, encoder, nullptr) < 0)
                throw std::runtime_error("Failed to open SW fallback encoder");
            std::cout << "[INFO] VAAPI encoder open failed; fell back to: "
                      << encoder->name << "\n";
        } else {
            throw std::runtime_error("Failed to open encoder: "
                                     + std::string(encoder->name));
        }
    }

    std::cout << "[INFO] Encoder: " << encoder->name
              << (enc_using_vaapi ? " (hardware/vaapi)"
                 : enc_using_nvenc ? " (hardware/nvenc)"
                 : " (software)") << "\n";

    avcodec_parameters_from_context(out_stream->codecpar, enc_ctx);
    out_stream->time_base = enc_ctx->time_base;

    if (!(out_ctx->oformat->flags & AVFMT_NOFILE)) {
        if (avio_open(&out_ctx->pb, output_path.c_str(), AVIO_FLAG_WRITE) < 0)
            throw std::runtime_error("Failed to open output file: " + output_path);
    }

    if (avformat_write_header(out_ctx, nullptr) < 0)
        throw std::runtime_error("avformat_write_header failed");

    // WHY always NV12: RGBA→NV12 conversion is done on GPU (rgba_to_nv12 kernels) for
    // all paths. NVENC and libx264 both accept NV12 natively; h264_vaapi requires it.
    // This avoids CPU sws_scale and shrinks the encode-side PCIe readback by ~2.67×.
    AVPixelFormat enc_sw_fmt = AV_PIX_FMT_NV12;

    // WHY lazy sws_to_rgba: the decoded frame's pixel format is unknown until the
    // first frame arrives — HW decoders may output NV12/CUDA surfaces while SW
    // outputs YUV420P. Creating the context on the first frame avoids a mismatch
    // that would silently produce corrupt colour output.
    SwsContext* sws_to_rgba = nullptr;

    // ── OpenCL image objects (RGBA UNORM_INT8) ────────────────────────────────
    cl::ImageFormat img_fmt(CL_RGBA, CL_UNORM_INT8);

    // WHY READ_WRITE for cl_src: zero-copy path writes RGBA into cl_src via nv12_to_rgba
    // kernel; copy path writes via enqueueWriteImage. Both are device writes;
    // the filter kernel reads. READ_ONLY would block the kernel write.
    cl::Image2D cl_src(ocl.context, CL_MEM_READ_WRITE, img_fmt, frame_w, frame_h);
    // WHY READ_WRITE for cl_dst: the filter kernel writes cl_dst; the zero-copy encode path
    // reads it via the RGBA→NV12 kernel. WRITE_ONLY would block that read.
    cl::Image2D cl_dst(ocl.context, CL_MEM_READ_WRITE, img_fmt, frame_w, frame_h);

    // Integer safety: promote to size_t before multiply.
    const size_t rgba_bytes = static_cast<size_t>(frame_w) * frame_h * 4;
    std::vector<uint8_t> rgba_in(rgba_bytes);   // fallback: YUV420P→RGBA on CPU

    // Persistent NV12 images shared by the GPU-assisted SW map (NV12 upload→nv12_to_rgba)
    // and SW encode (rgba_to_nv12→NV12 readback) paths.
    // WHY READ_WRITE: nv12_to_rgba writes cl_nv12_y/uv (decode side);
    // rgba_to_nv12_y/uv reads them are separate images so no conflict — still READ_WRITE
    // to keep the flag consistent with cl_src/cl_dst convention.
    cl::Image2D cl_nv12_y (ocl.context, CL_MEM_READ_WRITE,
                            cl::ImageFormat(CL_R,  CL_UNORM_INT8), frame_w,      frame_h);
    cl::Image2D cl_nv12_uv(ocl.context, CL_MEM_READ_WRITE,
                            cl::ImageFormat(CL_RG, CL_UNORM_INT8), frame_w / 2, frame_h / 2);

    // ── Allocate frame / packet objects ──────────────────────────────────────
    AVFrame*  dec_frame = av_frame_alloc();
    AVFrame*  enc_frame = av_frame_alloc();
    AVPacket* pkt       = av_packet_alloc();

    enc_frame->format = static_cast<int>(enc_sw_fmt);
    enc_frame->width  = frame_w;
    enc_frame->height = frame_h;
    if (av_frame_get_buffer(enc_frame, 0) < 0)
        throw std::runtime_error("av_frame_get_buffer failed for encode frame");

    // ── Per-frame loop (with SW-fallback retry) ───────────────────────────────
    // WHY retry loop: some HW decoders (e.g. h264_cuvid) open successfully via
    // avcodec_open2 but silently fail to produce frames at decode time (e.g.
    // CUDA_ERROR_NOT_SUPPORTED on unsupported NVDEC hardware). After attempt 0
    // produces 0 frames, attempt 1 reinitialises the decoder as software.
    std::printf("\n%-6s | %-9s | %-9s | %-9s | %-9s | %s\n",
                "Frame", "Decode", "Map", "Filter", "Encode", "Total");
    std::printf("-------|-----------|-----------|-----------|-----------|----------\n");

    int    frame_idx   = 0;
    double fps_acc     = 0.0;
    int    fps_count   = 0;

    const std::array<size_t, 3> img_origin = {0, 0, 0};
    const std::array<size_t, 3> img_region = {static_cast<size_t>(frame_w),
                                               static_cast<size_t>(frame_h), 1};
    const std::array<size_t, 3> uv_region  = {static_cast<size_t>(frame_w / 2),
                                               static_cast<size_t>(frame_h / 2), 1};

    for (int attempt = 0; attempt < 2; ++attempt) {
        if (attempt == 1) {
            if (!using_hw) break;   // already on SW — no retry needed
            // HW produced 0 frames: reinit decoder as SW and seek to start.
            avcodec_free_context(&dec_ctx);
            decoder  = avcodec_find_decoder(codecpar->codec_id);
            using_hw = false;
            dec_ctx  = avcodec_alloc_context3(decoder);
            if (avcodec_parameters_to_context(dec_ctx, codecpar) < 0)
                throw std::runtime_error("avcodec_parameters_to_context failed (SW retry)");
            if (avcodec_open2(dec_ctx, decoder, nullptr) < 0)
                throw std::runtime_error("Failed to open SW decoder on retry");
            av_seek_frame(fmt_ctx, video_idx, 0, AVSEEK_FLAG_BACKWARD);
            std::cout << "[INFO] HW decoder produced no frames; retrying with SW decoder: "
                      << decoder->name << "\n";
            if (sws_to_rgba) { sws_freeContext(sws_to_rgba); sws_to_rgba = nullptr; }
        }

    while (av_read_frame(fmt_ctx, pkt) >= 0) {
        if (pkt->stream_index != video_idx) {
            av_packet_unref(pkt);
            continue;
        }

        // Start decode timer before sending the packet.
        auto t_dec_start = Clock::now();

        if (avcodec_send_packet(dec_ctx, pkt) < 0) {
            av_packet_unref(pkt);
            continue;
        }
        av_packet_unref(pkt);

        while (avcodec_receive_frame(dec_ctx, dec_frame) == 0) {
            auto t_dec_end  = Clock::now();
            double decode_ms = Ms(t_dec_end - t_dec_start).count();

            // ── Map: VAAPI surface / YUV → RGBA cl::Image2D ────────────────
            auto t_map_start = Clock::now();

            cl::Event map_event;
            if (hw_interop && dec_frame->format == AV_PIX_FMT_VAAPI) {
                // ── Zero-copy path ───────────────────────────────────────────
                // Import the VAAPI surface directly as two read-only CL images;
                // the NV12→RGBA kernel converts on-GPU — no CPU readback.
                VASurfaceID va_surf = static_cast<VASurfaceID>(
                    reinterpret_cast<uintptr_t>(dec_frame->data[3]));

                cl_int err = CL_SUCCESS;
                cl_mem y_mem = clCreateFromVA_surf(
                    ocl.context(), CL_MEM_READ_ONLY, &va_surf, Y_PLANE, &err);
                if (err != CL_SUCCESS)
                    throw std::runtime_error("clCreateFromVA_surf(Y) failed: err=" + std::to_string(err));
                cl_mem uv_mem = clCreateFromVA_surf(
                    ocl.context(), CL_MEM_READ_ONLY, &va_surf, UV_PLANE, &err);
                if (err != CL_SUCCESS) { clReleaseMemObject(y_mem); throw std::runtime_error("clCreateFromVA_surf(UV) failed: err=" + std::to_string(err)); }

                cl_mem mems[2] = {y_mem, uv_mem};
                CL_CHECK(clAcquireVA_surfs(prof_queue(), 2, mems, 0, nullptr, nullptr));

                // Wrap in cl::Image2D for setArg (retainObject=true: we own the
                // underlying cl_mem, but the wrapper adds an extra retain here
                // which we balance by releasing the raw handles after the kernel).
                cl::Image2D y_img(y_mem,   true);
                cl::Image2D uv_img(uv_mem, true);

                CL_CHECK(nv12_kernel.setArg(0, y_img));
                CL_CHECK(nv12_kernel.setArg(1, uv_img));
                CL_CHECK(nv12_kernel.setArg(2, cl_src));
                CL_CHECK(nv12_kernel.setArg(3, frame_w));
                CL_CHECK(nv12_kernel.setArg(4, frame_h));

                CL_CHECK(prof_queue.enqueueNDRangeKernel(
                    nv12_kernel, cl::NullRange,
                    cl::NDRange(static_cast<size_t>(frame_w),
                                static_cast<size_t>(frame_h)),
                    cl::NullRange, nullptr, &map_event));
                CL_CHECK(prof_queue.finish());

                CL_CHECK(clReleaseVA_surfs(prof_queue(), 2, mems, 0, nullptr, nullptr));
                clReleaseMemObject(y_mem);
                clReleaseMemObject(uv_mem);
            } else {
                // ── GPU-assisted copy path ────────────────────────────────────
                // Transfer HW surface to CPU as NV12 (VAAPI/CUDA → system RAM).
                AVFrame* sw_frame = dec_frame;
                AVFrame* hw_tmp   = nullptr;
                if (dec_frame->format == AV_PIX_FMT_VAAPI ||
                    dec_frame->format == AV_PIX_FMT_CUDA) {
                    hw_tmp = av_frame_alloc();
                    if (av_hwframe_transfer_data(hw_tmp, dec_frame, 0) < 0)
                        throw std::runtime_error("av_hwframe_transfer_data failed");
                    sw_frame = hw_tmp;
                }

                if (sw_frame->format == AV_PIX_FMT_NV12) {
                    // WHY upload NV12 instead of RGBA: NV12 is ~3 MB vs 8 MB RGBA
                    // (2.67× smaller PCIe transfer). The nv12_to_rgba kernel runs
                    // the colour conversion on GPU, eliminating CPU sws_scale.
                    CL_CHECK(prof_queue.enqueueWriteImage(
                        cl_nv12_y, CL_TRUE, img_origin, img_region,
                        static_cast<size_t>(sw_frame->linesize[0]), 0,
                        sw_frame->data[0]));
                    CL_CHECK(prof_queue.enqueueWriteImage(
                        cl_nv12_uv, CL_TRUE, img_origin, uv_region,
                        static_cast<size_t>(sw_frame->linesize[1]), 0,
                        sw_frame->data[1]));
                    CL_CHECK(nv12_kernel.setArg(0, cl_nv12_y));
                    CL_CHECK(nv12_kernel.setArg(1, cl_nv12_uv));
                    CL_CHECK(nv12_kernel.setArg(2, cl_src));
                    CL_CHECK(nv12_kernel.setArg(3, frame_w));
                    CL_CHECK(nv12_kernel.setArg(4, frame_h));
                    CL_CHECK(prof_queue.enqueueNDRangeKernel(
                        nv12_kernel, cl::NullRange,
                        cl::NDRange(static_cast<size_t>(frame_w),
                                    static_cast<size_t>(frame_h)),
                        cl::NullRange, nullptr, &map_event));
                    CL_CHECK(prof_queue.finish());
                } else {
                    // CPU fallback for YUV420P (SW decoder retry path).
                    if (!sws_to_rgba) {
                        sws_to_rgba = sws_getContext(
                            frame_w, frame_h,
                            static_cast<AVPixelFormat>(sw_frame->format),
                            frame_w, frame_h, AV_PIX_FMT_RGBA,
                            SWS_BILINEAR, nullptr, nullptr, nullptr);
                        if (!sws_to_rgba)
                            throw std::runtime_error("sws_getContext (->RGBA) failed");
                    }
                    uint8_t* dst_data[4]   = {rgba_in.data(), nullptr, nullptr, nullptr};
                    int      dst_stride[4] = {frame_w * 4, 0, 0, 0};
                    sws_scale(sws_to_rgba,
                              sw_frame->data, sw_frame->linesize, 0, frame_h,
                              dst_data, dst_stride);
                    CL_CHECK(prof_queue.enqueueWriteImage(
                        cl_src, CL_TRUE, img_origin, img_region,
                        static_cast<size_t>(frame_w) * 4, 0,
                        rgba_in.data()));
                }

                if (hw_tmp) av_frame_free(&hw_tmp);
            }

            auto t_map_end = Clock::now();
            double map_ms  = Ms(t_map_end - t_map_start).count();

            // ── Filter: dispatch kernel, time via cl::Event ─────────────────
            CL_CHECK(kernel.setArg(0, cl_src));
            CL_CHECK(kernel.setArg(1, cl_dst));
            CL_CHECK(kernel.setArg(2, frame_w));
            CL_CHECK(kernel.setArg(3, frame_h));

            cl::Event filter_event;
            CL_CHECK(prof_queue.enqueueNDRangeKernel(
                kernel, cl::NullRange,
                cl::NDRange(static_cast<size_t>(frame_w),
                            static_cast<size_t>(frame_h)),
                cl::NullRange, nullptr, &filter_event));
            CL_CHECK(prof_queue.finish());

            double filter_ms = duration_ms(filter_event);

            // ── Encode ─────────────────────────────────────────────────────────
            auto t_enc_start = Clock::now();

            AVFrame* send_frame = nullptr;
            AVFrame* hw_frame   = nullptr;

            if (hw_interop && enc_using_vaapi) {
                // ── Zero-copy encode path ────────────────────────────────────
                // Get an encoder VAAPI surface from the pool, import it as two
                // write-only CL images, run RGBA→NV12 kernels directly from
                // cl_dst — no CPU readback, no sws_scale, no av_hwframe_transfer_data.
                hw_frame = av_frame_alloc();
                if (av_hwframe_get_buffer(enc_ctx->hw_frames_ctx, hw_frame, 0) < 0)
                    throw std::runtime_error("av_hwframe_get_buffer failed");

                VASurfaceID enc_surf = static_cast<VASurfaceID>(
                    reinterpret_cast<uintptr_t>(hw_frame->data[3]));

                cl_int err = CL_SUCCESS;
                cl_mem enc_y_mem  = clCreateFromVA_surf(
                    ocl.context(), CL_MEM_WRITE_ONLY, &enc_surf, Y_PLANE, &err);
                if (err != CL_SUCCESS)
                    throw std::runtime_error("clCreateFromVA_surf(enc Y) err=" + std::to_string(err));
                cl_mem enc_uv_mem = clCreateFromVA_surf(
                    ocl.context(), CL_MEM_WRITE_ONLY, &enc_surf, UV_PLANE, &err);
                if (err != CL_SUCCESS) {
                    clReleaseMemObject(enc_y_mem);
                    throw std::runtime_error("clCreateFromVA_surf(enc UV) err=" + std::to_string(err));
                }

                cl_mem enc_mems[2] = {enc_y_mem, enc_uv_mem};
                CL_CHECK(clAcquireVA_surfs(prof_queue(), 2, enc_mems, 0, nullptr, nullptr));

                cl::Image2D enc_y_img(enc_y_mem,   true);
                cl::Image2D enc_uv_img(enc_uv_mem, true);

                // Y plane: full W×H resolution
                CL_CHECK(rgba_nv12_y_kernel.setArg(0, cl_dst));
                CL_CHECK(rgba_nv12_y_kernel.setArg(1, enc_y_img));
                CL_CHECK(rgba_nv12_y_kernel.setArg(2, frame_w));
                CL_CHECK(rgba_nv12_y_kernel.setArg(3, frame_h));
                CL_CHECK(prof_queue.enqueueNDRangeKernel(
                    rgba_nv12_y_kernel, cl::NullRange,
                    cl::NDRange(static_cast<size_t>(frame_w),
                                static_cast<size_t>(frame_h)),
                    cl::NullRange));

                // UV plane: half resolution (4:2:0 chroma subsampling)
                CL_CHECK(rgba_nv12_uv_kernel.setArg(0, cl_dst));
                CL_CHECK(rgba_nv12_uv_kernel.setArg(1, enc_uv_img));
                CL_CHECK(rgba_nv12_uv_kernel.setArg(2, frame_w));
                CL_CHECK(rgba_nv12_uv_kernel.setArg(3, frame_h));
                CL_CHECK(prof_queue.enqueueNDRangeKernel(
                    rgba_nv12_uv_kernel, cl::NullRange,
                    cl::NDRange(static_cast<size_t>((frame_w + 1) / 2),
                                static_cast<size_t>((frame_h + 1) / 2)),
                    cl::NullRange));

                CL_CHECK(prof_queue.finish());

                CL_CHECK(clReleaseVA_surfs(prof_queue(), 2, enc_mems, 0, nullptr, nullptr));
                clReleaseMemObject(enc_y_mem);
                clReleaseMemObject(enc_uv_mem);

                hw_frame->pts = static_cast<int64_t>(frame_idx);
                send_frame    = hw_frame;
            } else {
                // ── GPU-assisted encode path ──────────────────────────────────
                // RGBA→NV12 on GPU: avoids 8 MB RGBA readback + CPU sws_scale.
                // cl_nv12_y/cl_nv12_uv are reused from the map path allocation.
                if (av_frame_make_writable(enc_frame) < 0)
                    throw std::runtime_error("av_frame_make_writable failed");

                CL_CHECK(rgba_nv12_y_kernel.setArg(0, cl_dst));
                CL_CHECK(rgba_nv12_y_kernel.setArg(1, cl_nv12_y));
                CL_CHECK(rgba_nv12_y_kernel.setArg(2, frame_w));
                CL_CHECK(rgba_nv12_y_kernel.setArg(3, frame_h));
                CL_CHECK(prof_queue.enqueueNDRangeKernel(
                    rgba_nv12_y_kernel, cl::NullRange,
                    cl::NDRange(static_cast<size_t>(frame_w),
                                static_cast<size_t>(frame_h)),
                    cl::NullRange));

                CL_CHECK(rgba_nv12_uv_kernel.setArg(0, cl_dst));
                CL_CHECK(rgba_nv12_uv_kernel.setArg(1, cl_nv12_uv));
                CL_CHECK(rgba_nv12_uv_kernel.setArg(2, frame_w));
                CL_CHECK(rgba_nv12_uv_kernel.setArg(3, frame_h));
                CL_CHECK(prof_queue.enqueueNDRangeKernel(
                    rgba_nv12_uv_kernel, cl::NullRange,
                    cl::NDRange(static_cast<size_t>((frame_w + 1) / 2),
                                static_cast<size_t>((frame_h + 1) / 2)),
                    cl::NullRange));

                CL_CHECK(prof_queue.finish());

                // WHY read NV12 Y+UV separately: ~3 MB total vs 8 MB RGBA (2.67× less PCIe).
                // enc_frame->linesize[0/1] used as row_pitch so FFmpeg alignment is respected.
                CL_CHECK(prof_queue.enqueueReadImage(
                    cl_nv12_y, CL_TRUE, img_origin, img_region,
                    static_cast<size_t>(enc_frame->linesize[0]), 0,
                    enc_frame->data[0]));
                CL_CHECK(prof_queue.enqueueReadImage(
                    cl_nv12_uv, CL_TRUE, img_origin, uv_region,
                    static_cast<size_t>(enc_frame->linesize[1]), 0,
                    enc_frame->data[1]));

                enc_frame->pts = static_cast<int64_t>(frame_idx);
                send_frame     = enc_frame;

                if (enc_using_vaapi) {
                    // Non-interop VAAPI: upload SW NV12 frame to VAAPI surface.
                    hw_frame = av_frame_alloc();
                    if (av_hwframe_get_buffer(enc_ctx->hw_frames_ctx, hw_frame, 0) < 0)
                        throw std::runtime_error("av_hwframe_get_buffer failed");
                    if (av_hwframe_transfer_data(hw_frame, enc_frame, 0) < 0)
                        throw std::runtime_error("av_hwframe_transfer_data (upload) failed");
                    hw_frame->pts = enc_frame->pts;
                    send_frame    = hw_frame;
                }
            }

            if (avcodec_send_frame(enc_ctx, send_frame) >= 0) {
                AVPacket* out_pkt = av_packet_alloc();
                while (avcodec_receive_packet(enc_ctx, out_pkt) == 0) {
                    av_packet_rescale_ts(out_pkt, enc_ctx->time_base,
                                         out_stream->time_base);
                    out_pkt->stream_index = out_stream->index;
                    av_interleaved_write_frame(out_ctx, out_pkt);
                    av_packet_unref(out_pkt);
                }
                av_packet_free(&out_pkt);
            }

            if (hw_frame) av_frame_free(&hw_frame);

            auto t_enc_end  = Clock::now();
            double encode_ms = Ms(t_enc_end - t_enc_start).count();

            double total_ms = decode_ms + map_ms + filter_ms + encode_ms;

            std::printf("%-6d | %6.2f ms  | %6.2f ms  | %6.2f ms  | %6.2f ms  | %6.2f ms\n",
                        frame_idx, decode_ms, map_ms, filter_ms, encode_ms, total_ms);

            if (total_ms > 0.0) {
                fps_acc += 1000.0 / total_ms;
                fps_count++;
            }

            ++frame_idx;
            av_frame_unref(dec_frame);

            // Reset decode timer for any additional frames from this packet.
            t_dec_start = Clock::now();
        }
    }  // end packet while

    if (frame_idx > 0) break;  // success — do not retry with SW
    }  // end retry for

    // ── Flush encoder ────────────────────────────────────────────────────────
    if (avcodec_send_frame(enc_ctx, nullptr) < 0)
        std::cerr << "[WARN] Encoder flush signal failed\n";
    {
        AVPacket* out_pkt = av_packet_alloc();
        while (avcodec_receive_packet(enc_ctx, out_pkt) == 0) {
            av_packet_rescale_ts(out_pkt, enc_ctx->time_base, out_stream->time_base);
            out_pkt->stream_index = out_stream->index;
            av_interleaved_write_frame(out_ctx, out_pkt);
            av_packet_unref(out_pkt);
        }
        av_packet_free(&out_pkt);
    }

    av_write_trailer(out_ctx);

    // ── Summary ──────────────────────────────────────────────────────────────
    if (fps_count > 0)
        std::printf("\nAverage FPS: %.1f  (over %d frames)\n",
                    fps_acc / fps_count, fps_count);

    std::cout << "Output: " << output_path << "\n";

    // ── Teardown ──────────────────────────────────────────────────────────────
    if (sws_to_rgba) sws_freeContext(sws_to_rgba);
    av_frame_free(&dec_frame);
    av_frame_free(&enc_frame);
    av_packet_free(&pkt);
    avcodec_free_context(&dec_ctx);
    avcodec_free_context(&enc_ctx);
    if (vaapi_dev_ctx) av_buffer_unref(&vaapi_dev_ctx);
    avformat_close_input(&fmt_ctx);
    if (!(out_ctx->oformat->flags & AVFMT_NOFILE))
        avio_closep(&out_ctx->pb);
    avformat_free_context(out_ctx);

    return 0;
}  // run()
