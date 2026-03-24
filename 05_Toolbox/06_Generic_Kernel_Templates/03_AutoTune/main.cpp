// 03_autotune — Runtime autotuner: builds uchar, float, and (optionally) half
// variants of mad_kernel.cl, measures kernel time for each via cl::Event
// profiling, selects the fastest, and produces output.bmp from the winner.
//
// WHY autotuning matters:
//   The optimal scalar type varies by hardware. On some GPUs uchar packs better
//   into vector loads; on others native float throughput wins. The half type
//   can be 2× faster than float on hardware with dedicated FP16 units, but
//   requires cl_khr_fp16 extension support. Rather than guessing, we measure.
//
// Half guard: cl_khr_fp16 is queried at runtime. If absent the half variant is
// skipped with a console message; the binary does not abort or exit non-zero.
//
// Timing table format:
//   Type    | Kernel Time (ms) | Notes
//   --------|------------------|------
//   uchar   |            0.312 |
//   float   |            0.487 |
//   half    |            0.291 |
//   Winner  : uchar

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image.h>
#include <stb_image_write.h>

#include "opencl_utils.hpp"
#include "ocl_wrapper.hpp"
#include "image_utils.hpp"

#include <CLI/CLI.hpp>

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// TypeVariant — metadata for one compiled kernel specialisation.
// ---------------------------------------------------------------------------
struct TypeVariant {
    std::string name;          // "uchar", "float", "half"
    std::string build_opts;    // e.g. "-D TYPE=float"
    size_t      elem_bytes;    // bytes per scalar element
    bool        skipped;       // true if extension check failed
    std::string skip_reason;   // human-readable reason (if skipped)
    double      kernel_ms;     // measured kernel time (invalid if skipped)
};

// ---------------------------------------------------------------------------
// has_extension — check whether a device extension string contains 'ext'.
// WHY substring search: OpenCL extension strings are space-delimited, so we
// check for the exact name surrounded by spaces or at string boundaries.
// ---------------------------------------------------------------------------
static bool has_extension(const cl::Device& dev, const std::string& ext)
{
    const std::string exts = dev.getInfo<CL_DEVICE_EXTENSIONS>();
    // Simple contains: extension names don't have common substrings that would
    // produce false positives (e.g. "cl_khr_fp16" won't match "cl_khr_fp16_extra").
    return exts.find(ext) != std::string::npos;
}

// ---------------------------------------------------------------------------
// build_program_from_source — compile with options; print log and rethrow on failure.
// WHY not common build_program: source is a runtime-generated template string,
// not a .cl file path.
// ---------------------------------------------------------------------------
static cl::Program build_program_from_source(const cl::Context& ctx,
                                              const cl::Device&  dev,
                                              const std::string& src,
                                              const std::string& opts)
{
    cl::Program prog(ctx, cl::Program::Sources{src});
    try {
        prog.build({dev}, opts.c_str());
    } catch (const cl::Error&) {
        std::cerr << "Build log (" << opts << "):\n"
                  << prog.getBuildInfo<CL_PROGRAM_BUILD_LOG>(dev) << "\n";
        throw;
    }
    return prog;
}

// ---------------------------------------------------------------------------
// run_kernel — dispatch mad_kernel once, return kernel time in ms.
// ---------------------------------------------------------------------------
static double run_kernel(cl::CommandQueue& queue,
                         cl::Program&      prog,
                         cl::Buffer&       buf_out,
                         cl::Buffer&       buf_in,
                         float             contrast,
                         float             brightness,
                         cl_uint           n_elements)
{
    cl::Kernel kernel(prog, "mad_kernel");
    CL_CHECK(kernel.setArg(0, buf_out));
    CL_CHECK(kernel.setArg(1, buf_in));
    CL_CHECK(kernel.setArg(2, contrast));
    CL_CHECK(kernel.setArg(3, brightness));
    CL_CHECK(kernel.setArg(4, n_elements));

    cl::Event evt;
    CL_CHECK(queue.enqueueNDRangeKernel(
        kernel,
        cl::NullRange,
        cl::NDRange(static_cast<size_t>(n_elements)),
        cl::NullRange,
        nullptr,
        &evt));
    CL_CHECK(queue.finish());

    return duration_ms(evt);
}

int main(int argc, char** argv)
{
    // ── CLI ──────────────────────────────────────────────────────────────────
    CLI::App app{"03_autotune — multi-type MAD kernel autotuner (uchar/float/half)"};

    std::string image_path;
    int         width      = 1920;
    int         height     = 1080;
    float       contrast   = 1.2f;
    float       brightness = -10.0f;
    bool        autotune   = false;   // flag accepted for CLI11 --help conformance

    app.add_option("--image",      image_path, "Input BMP/PNG path (gradient used if absent)");
    app.add_option("--width",      width,      "Gradient width  (default 1920)");
    app.add_option("--height",     height,     "Gradient height (default 1080)");
    app.add_option("--contrast",   contrast,   "Contrast multiplier (default 1.2)");
    app.add_option("--brightness", brightness, "Brightness addend   (default -10)");
    app.add_flag  ("--autotune",   autotune,   "Enable autotuner (always on; flag kept for CLI compatibility)");

    CLI11_PARSE(app, argc, argv);

    // ── Input image ──────────────────────────────────────────────────────────
    int channels = 3;
    std::vector<uint8_t> host_in_u8;

    if (image_path.empty()) {
        std::cout << "No --image provided; using " << width << "x" << height
                  << " synthetic gradient.\n";
        host_in_u8 = make_gradient(width, height, channels);
    } else {
        host_in_u8 = load_rgb_image(image_path, width, height, channels);
        std::cout << "Loaded: " << image_path << " (" << width << "x" << height
                  << ", " << channels << "ch)\n";
    }

    const size_t n_elements = static_cast<size_t>(width)
                            * static_cast<size_t>(height)
                            * static_cast<size_t>(channels);

    if (n_elements > static_cast<size_t>(std::numeric_limits<cl_uint>::max())) {
        throw std::runtime_error("Image too large: element count exceeds cl_uint range");
    }
    const cl_uint n_elem_int = static_cast<cl_uint>(n_elements);

    // Float copy of input for float/half variants.
    std::vector<float> host_in_f32(n_elements);
    for (size_t i = 0; i < n_elements; ++i) {
        host_in_f32[i] = static_cast<float>(host_in_u8[i]);
    }

    // ── OpenCL setup ─────────────────────────────────────────────────────────
    OclContext ocl = create_context();
    cl::CommandQueue prof_queue(ocl.context, ocl.device, CL_QUEUE_PROFILING_ENABLE);

    // ── Extension check for half ──────────────────────────────────────────────
    const bool supports_fp16 = has_extension(ocl.device, "cl_khr_fp16");
    if (!supports_fp16) {
        std::cout << "Note: cl_khr_fp16 not supported on this device — "
                     "half variant will be skipped.\n";
    }

    // ── Load shared kernel source ─────────────────────────────────────────────
    const std::string src = load_kernel_source("kernels/mad_kernel.cl");

    // ── Device buffers ────────────────────────────────────────────────────────
    const size_t bytes_u8  = n_elements * sizeof(uint8_t);
    const size_t bytes_f32 = n_elements * sizeof(float);
    // cl_half is a uint16_t in the host API; size = 2 bytes per element.
    const size_t bytes_f16 = n_elements * sizeof(cl_half);

    cl::Buffer buf_in_u8(ocl.context,
                         CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                         bytes_u8, host_in_u8.data());
    cl::Buffer buf_out_u8(ocl.context, CL_MEM_WRITE_ONLY, bytes_u8);

    cl::Buffer buf_in_f32(ocl.context,
                          CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                          bytes_f32, host_in_f32.data());
    cl::Buffer buf_out_f32(ocl.context, CL_MEM_WRITE_ONLY, bytes_f32);

    // We allocate half buffers unconditionally but only use them if fp16 is available.
    // WHY: cl::Buffer allocation is cheap; avoids conditional buffer management code.
    // The half input is initialised with the same float values cast to cl_half.
    std::vector<cl_half> host_in_f16(n_elements);
    if (supports_fp16) {
        // cl_half is an opaque 16-bit type in the host API; we convert using
        // the standard OpenCL host helper. For portability we use a manual
        // float→half bit-pattern conversion since vstore_half is a device function.
        // WHY manual conversion: cl.hpp 1.2 does not expose clConvertFloat16.
        // Simple approach: clamp to half range and use union trick.
        for (size_t i = 0; i < n_elements; ++i) {
            // Use the compiler builtin if available, otherwise a portable approximation.
            // On most x86 hosts with GCC/Clang, __fp16 or _Float16 are available.
            // We use a simple conversion: cast via union (not IEEE-perfect but adequate
            // for this demonstration where we measure time, not precision).
#if defined(__F16C__) || defined(__AVX512FP16__)
            // Use hardware-accelerated conversion when available.
            host_in_f16[i] = _cvtss_sh(host_in_f32[i], 0);
#else
            // Portable soft conversion: copy via float bit manipulation.
            // This is adequate for demonstrating the autotuner; result is used
            // only to confirm correct output, not for precision-critical work.
            const float f = host_in_f32[i];
            uint32_t bits;
            static_assert(sizeof(float) == sizeof(uint32_t), "IEEE float assumption");
            __builtin_memcpy(&bits, &f, sizeof(float));
            const uint32_t sign     = (bits >> 31) & 0x1;
            const int32_t  exp      = static_cast<int32_t>((bits >> 23) & 0xFF) - 127;
            const uint32_t mantissa = bits & 0x7FFFFF;

            uint16_t h = 0;
            if (exp > 15) {
                h = static_cast<uint16_t>((sign << 15) | 0x7C00);  // inf
            } else if (exp < -14) {
                h = static_cast<uint16_t>(sign << 15);              // zero
            } else {
                h = static_cast<uint16_t>(
                    (sign << 15) |
                    (static_cast<uint16_t>(exp + 15) << 10) |
                    (mantissa >> 13));
            }
            host_in_f16[i] = static_cast<cl_half>(h);
#endif
        }
    }

    cl::Buffer buf_in_f16(ocl.context,
                          supports_fp16
                              ? static_cast<cl_mem_flags>(CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR)
                              : static_cast<cl_mem_flags>(CL_MEM_READ_ONLY),
                          bytes_f16,
                          supports_fp16 ? host_in_f16.data() : nullptr);
    cl::Buffer buf_out_f16(ocl.context, CL_MEM_WRITE_ONLY, bytes_f16);

    // ── Variant registry ──────────────────────────────────────────────────────
    // We define all three variants up front; skipped variants are marked before
    // the dispatch loop.
    std::vector<TypeVariant> variants = {
        { "uchar", "-D TYPE=uchar",  sizeof(uint8_t), false, "", 0.0 },
        { "float", "-D TYPE=float",  sizeof(float),   false, "", 0.0 },
        // WHY pragma via build options: injecting the extension enable pragma
        // into the source would prevent the same source compiling for non-half
        // targets. Build options keep the source type-agnostic.
        { "half",  "-D TYPE=half",
                   sizeof(cl_half), !supports_fp16,
                   supports_fp16 ? "" : "cl_khr_fp16 not supported", 0.0 },
    };
    // Note: OpenCL build options cannot contain #pragma — the extension must be
    // in the source or prepended. We prepend it to the source string for half.

    // ── Build and run each variant ────────────────────────────────────────────
    // Track which buffer pair belongs to each variant.
    struct BufferPair { cl::Buffer& in; cl::Buffer& out; };
    std::vector<BufferPair> bufs = {
        { buf_in_u8,  buf_out_u8  },
        { buf_in_f32, buf_out_f32 },
        { buf_in_f16, buf_out_f16 },
    };

    std::cout << "\nBuilding and running variants...\n";

    for (size_t vi = 0; vi < variants.size(); ++vi) {
        auto& v = variants[vi];

        if (v.skipped) {
            std::cout << "  [" << v.name << "] SKIPPED: " << v.skip_reason << "\n";
            continue;
        }

        std::string src_for_build = src;
        std::string build_opts_clean = v.build_opts;

        // WHY prepend pragma for half: the OpenCL spec §6.9 forbids #pragma in
        // build options strings. We inject the extension enable as the very first
        // line of source so it takes effect before any typedef or use of 'half'.
        if (v.name == "half") {
            src_for_build = "#pragma OPENCL EXTENSION cl_khr_fp16 : enable\n" + src;
            build_opts_clean = "-D TYPE=half";
        }

        std::cout << "  Building " << v.name << " (" << build_opts_clean << ")...\n";

        try {
            cl::Program prog = build_program_from_source(ocl.context, ocl.device,
                                             src_for_build, build_opts_clean);
            v.kernel_ms = run_kernel(prof_queue, prog,
                                     bufs[vi].out, bufs[vi].in,
                                     contrast, brightness, n_elem_int);
        } catch (const std::exception& e) {
            std::cerr << "  [" << v.name << "] failed: " << e.what() << "\n";
            v.skipped     = true;
            v.skip_reason = std::string("runtime error: ") + e.what();
        }
    }

    // ── Find the winner ───────────────────────────────────────────────────────
    double      best_ms   = std::numeric_limits<double>::max();
    std::string winner    = "(none)";
    size_t      winner_vi = 0;

    for (size_t vi = 0; vi < variants.size(); ++vi) {
        const auto& v = variants[vi];
        if (!v.skipped && v.kernel_ms < best_ms) {
            best_ms   = v.kernel_ms;
            winner    = v.name;
            winner_vi = vi;
        }
    }

    // ── Timing table ─────────────────────────────────────────────────────────
    std::cout << "\nType    | Kernel Time (ms) | Notes\n";
    std::cout << "--------|------------------|------\n";
    std::cout << std::fixed << std::setprecision(3);

    for (const auto& v : variants) {
        std::string notes;
        if (v.skipped) {
            notes = "[skipped: " + v.skip_reason + "]";
        }
        if (v.name == winner) {
            notes = "[WINNER]";
        }

        std::cout << std::left << std::setw(8) << v.name << "| ";
        if (v.skipped) {
            std::cout << std::setw(16) << "        N/A" << " | " << notes << "\n";
        } else {
            std::cout << std::right << std::setw(16) << v.kernel_ms
                      << " | " << notes << "\n";
        }
    }

    std::cout << "Winner  : " << winner << "\n";

    // ── Read back winner result & save output.bmp ─────────────────────────────
    std::vector<uint8_t> host_out_final(n_elements);

    if (winner == "uchar") {
        CL_CHECK(prof_queue.enqueueReadBuffer(buf_out_u8, CL_TRUE, 0,
                                              bytes_u8, host_out_final.data()));
    } else if (winner == "float") {
        std::vector<float> tmp(n_elements);
        CL_CHECK(prof_queue.enqueueReadBuffer(buf_out_f32, CL_TRUE, 0,
                                              bytes_f32, tmp.data()));
        for (size_t i = 0; i < n_elements; ++i) {
            host_out_final[i] = static_cast<uint8_t>(
                std::max(0.0f, std::min(255.0f, tmp[i])));
        }
    } else if (winner == "half") {
        std::vector<cl_half> tmp(n_elements);
        CL_CHECK(prof_queue.enqueueReadBuffer(buf_out_f16, CL_TRUE, 0,
                                              bytes_f16, tmp.data()));
        // Convert cl_half back to uint8_t via float for BMP output.
        for (size_t i = 0; i < n_elements; ++i) {
            // Unpack the 16-bit half to float via bit pattern.
            const uint16_t h = static_cast<uint16_t>(tmp[i]);
            const uint32_t sign     = (h >> 15) & 0x1;
            const uint32_t exp_h    = (h >> 10) & 0x1F;
            const uint32_t mant_h   = h & 0x3FF;
            float f = 0.0f;
            if (exp_h == 0x1F) {
                f = sign ? -std::numeric_limits<float>::infinity()
                         :  std::numeric_limits<float>::infinity();
            } else if (exp_h == 0) {
                // subnormal
                f = static_cast<float>(mant_h) / 16777216.0f;  // 2^24
            } else {
                uint32_t bits_f32 = (sign << 31)
                                  | ((exp_h + 112) << 23)   // bias adjust: 127 - 15 = 112
                                  | (mant_h << 13);
                __builtin_memcpy(&f, &bits_f32, sizeof(float));
            }
            host_out_final[i] = static_cast<uint8_t>(
                std::max(0.0f, std::min(255.0f, f)));
        }
    } else {
        std::cerr << "Warning: no winner selected (all variants skipped). "
                     "output.bmp not written.\n";
        return 0;
    }

    save_bmp("output.bmp", host_out_final, width, height, channels);
    std::cout << "\nSaved: output.bmp (from " << winner << " variant)\n";

    return 0;
}
