// main.cpp — FFmpeg OpenCL Transcoder
//
// Pipeline per frame:
//   Decode (VAAPI/NVDEC or SW) → Map to OpenCL → Filter kernel → Re-encode (H.264)
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
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/hwcontext.h>
#include <libavutil/avutil.h>
#include <libswscale/swscale.h>
}

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

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// load_kernel_source() is provided by opencl_utils.hpp (included via ocl_wrapper.hpp).

// Extract elapsed GPU time in milliseconds from a profiled cl::Event.
static double event_ms(const cl::Event& ev)
{
    cl_ulong start = ev.getProfilingInfo<CL_PROFILING_COMMAND_START>();
    cl_ulong end   = ev.getProfilingInfo<CL_PROFILING_COMMAND_END>();
    return static_cast<double>(end - start) * 1.0e-6;
}

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

    // ── OpenCL context ───────────────────────────────────────────────────────
    OclContext ocl = create_context();

    // WHY separate profiling queue: create_context() returns a standard queue
    // without CL_QUEUE_PROFILING_ENABLE; CL_PROFILING_COMMAND_* queries require
    // that flag to be set at queue creation time.
    cl::CommandQueue prof_queue(ocl.context, ocl.device, CL_QUEUE_PROFILING_ENABLE);

    // ── Hardware interop detection ───────────────────────────────────────────
    // cl_intel_va_api_media_sharing → Intel/AMD VAAPI zero-copy
    // cl_khr_egl_image              → Nvidia EGL zero-copy
    bool hw_interop = device_has_ext(ocl.device, "cl_intel_va_api_media_sharing")
                   || device_has_ext(ocl.device, "cl_khr_egl_image");
    if (!hw_interop)
        std::cout << "[INFO] Hardware interop unavailable; using software copy path.\n";

    // ── VAAPI device (shared for HW decoder + encoder) ───────────────────────
    // WHY shared: both h264_vaapi decoder and encoder need the same VAAPI device;
    // a single av_hwdevice_ctx avoids double driver init and enables surface reuse.
    AVBufferRef* vaapi_dev_ctx = nullptr;
    if (hw_interop) {
        if (av_hwdevice_ctx_create(&vaapi_dev_ctx, AV_HWDEVICE_TYPE_VAAPI,
                                   nullptr, nullptr, 0) < 0) {
            std::cout << "[INFO] VAAPI device init failed; falling back to SW paths.\n";
            vaapi_dev_ctx = nullptr;
        }
    }

    // ── Build filter kernel ───────────────────────────────────────────────────
    std::string kernel_src = load_kernel_source("kernels/filter.cl");
    cl::Program program(ocl.context, kernel_src);

    std::string build_opts = (effect == "sepia") ? "-D EFFECT_SEPIA" : "-D EFFECT_BLUR";
    try {
        program.build({ocl.device}, build_opts.c_str());
    } catch (const cl::Error&) {
        std::string log = program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(ocl.device);
        throw std::runtime_error("Kernel build failed:\n" + log);
    }

    cl::Kernel kernel(program, "apply_filter");

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

    if (vaapi_dev_ctx)
        encoder = avcodec_find_encoder_by_name("h264_vaapi");
    if (encoder)
        enc_using_vaapi = true;
    else {
        encoder = avcodec_find_encoder_by_name("libx264");
        if (!encoder) encoder = avcodec_find_encoder(AV_CODEC_ID_H264);
    }
    if (!encoder) throw std::runtime_error("No H.264 encoder found");

    AVStream* out_stream = avformat_new_stream(out_ctx, nullptr);
    if (!out_stream) throw std::runtime_error("avformat_new_stream failed");

    AVCodecContext* enc_ctx = avcodec_alloc_context3(encoder);
    if (!enc_ctx) throw std::runtime_error("avcodec_alloc_context3 failed (encoder)");

    enc_ctx->width        = frame_w;
    enc_ctx->height       = frame_h;
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
            enc_ctx->pix_fmt      = AV_PIX_FMT_YUV420P;
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
              << (enc_using_vaapi ? " (hardware/vaapi)" : " (software)") << "\n";

    avcodec_parameters_from_context(out_stream->codecpar, enc_ctx);
    out_stream->time_base = enc_ctx->time_base;

    if (!(out_ctx->oformat->flags & AVFMT_NOFILE)) {
        if (avio_open(&out_ctx->pb, output_path.c_str(), AVIO_FLAG_WRITE) < 0)
            throw std::runtime_error("Failed to open output file: " + output_path);
    }

    if (avformat_write_header(out_ctx, nullptr) < 0)
        throw std::runtime_error("avformat_write_header failed");

    // ── SwsContext: RGBA → NV12 (VAAPI path) or YUV420P (SW path) ────────────
    // WHY NV12 for VAAPI: h264_vaapi hw surfaces use NV12 as their sw_format;
    // uploading YUV420P would require an extra conversion inside the driver.
    AVPixelFormat enc_sw_fmt = enc_using_vaapi ? AV_PIX_FMT_NV12 : AV_PIX_FMT_YUV420P;
    SwsContext* sws_to_yuv = sws_getContext(
        frame_w, frame_h, AV_PIX_FMT_RGBA,
        frame_w, frame_h, enc_sw_fmt,
        SWS_BILINEAR, nullptr, nullptr, nullptr);
    if (!sws_to_yuv) throw std::runtime_error("sws_getContext (→YUV) failed");

    // WHY lazy sws_to_rgba: the decoded frame's pixel format is unknown until the
    // first frame arrives — HW decoders may output NV12/CUDA surfaces while SW
    // outputs YUV420P. Creating the context on the first frame avoids a mismatch
    // that would silently produce corrupt colour output.
    SwsContext* sws_to_rgba = nullptr;

    // ── OpenCL image objects (RGBA UNORM_INT8) ────────────────────────────────
    cl::ImageFormat img_fmt(CL_RGBA, CL_UNORM_INT8);
    cl::Image2D cl_src(ocl.context, CL_MEM_READ_ONLY,  img_fmt, frame_w, frame_h);
    cl::Image2D cl_dst(ocl.context, CL_MEM_WRITE_ONLY, img_fmt, frame_w, frame_h);

    // Integer safety: promote to size_t before multiply.
    const size_t rgba_bytes = static_cast<size_t>(frame_w) * frame_h * 4;
    std::vector<uint8_t> rgba_in(rgba_bytes);
    std::vector<uint8_t> rgba_out(rgba_bytes);

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

            // ── Map: YUV → RGBA → cl::Image2D ──────────────────────────────
            auto t_map_start = Clock::now();

            // Transfer HW surface (VAAPI or CUDA) to CPU memory for sws_scale.
            // WHY: sws_scale cannot operate on HW pixel formats directly.
            AVFrame* sw_frame = dec_frame;
            AVFrame* hw_tmp   = nullptr;
            if (dec_frame->format == AV_PIX_FMT_VAAPI ||
                dec_frame->format == AV_PIX_FMT_CUDA) {
                hw_tmp = av_frame_alloc();
                if (av_hwframe_transfer_data(hw_tmp, dec_frame, 0) < 0)
                    throw std::runtime_error("av_hwframe_transfer_data failed");
                sw_frame = hw_tmp;
            }

            // Lazy sws_to_rgba: created on the first frame using the actual
            // decoded pixel format, which may differ between HW and SW decoders.
            if (!sws_to_rgba) {
                sws_to_rgba = sws_getContext(
                    frame_w, frame_h,
                    static_cast<AVPixelFormat>(sw_frame->format),
                    frame_w, frame_h, AV_PIX_FMT_RGBA,
                    SWS_BILINEAR, nullptr, nullptr, nullptr);
                if (!sws_to_rgba)
                    throw std::runtime_error("sws_getContext (->RGBA) failed");
            }

            uint8_t*       dst_data[4]   = {rgba_in.data(), nullptr, nullptr, nullptr};
            int            dst_stride[4] = {frame_w * 4, 0, 0, 0};
            sws_scale(sws_to_rgba,
                      sw_frame->data, sw_frame->linesize, 0, frame_h,
                      dst_data, dst_stride);

            if (hw_tmp) av_frame_free(&hw_tmp);

            CL_CHECK(prof_queue.enqueueWriteImage(
                cl_src, CL_TRUE,
                img_origin, img_region,
                static_cast<size_t>(frame_w) * 4, 0,
                rgba_in.data()));

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

            double filter_ms = event_ms(filter_event);

            // Read back filtered RGBA.
            CL_CHECK(prof_queue.enqueueReadImage(
                cl_dst, CL_TRUE,
                img_origin, img_region,
                static_cast<size_t>(frame_w) * 4, 0,
                rgba_out.data()));

            // ── Encode: RGBA → NV12/YUV420P → (upload to VAAPI?) → avcodec ──
            auto t_enc_start = Clock::now();

            if (av_frame_make_writable(enc_frame) < 0)
                throw std::runtime_error("av_frame_make_writable failed");

            const uint8_t* src_data[4]   = {rgba_out.data(), nullptr, nullptr, nullptr};
            int            src_stride[4] = {frame_w * 4, 0, 0, 0};
            sws_scale(sws_to_yuv,
                      src_data, src_stride, 0, frame_h,
                      enc_frame->data, enc_frame->linesize);

            enc_frame->pts = static_cast<int64_t>(frame_idx);

            // For VAAPI encoder: upload the sw NV12 frame into a VAAPI surface.
            AVFrame* send_frame = enc_frame;
            AVFrame* hw_frame   = nullptr;
            if (enc_using_vaapi) {
                hw_frame = av_frame_alloc();
                if (av_hwframe_get_buffer(enc_ctx->hw_frames_ctx, hw_frame, 0) < 0)
                    throw std::runtime_error("av_hwframe_get_buffer failed");
                if (av_hwframe_transfer_data(hw_frame, enc_frame, 0) < 0)
                    throw std::runtime_error("av_hwframe_transfer_data (upload) failed");
                hw_frame->pts = enc_frame->pts;
                send_frame = hw_frame;
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
    sws_freeContext(sws_to_yuv);
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
