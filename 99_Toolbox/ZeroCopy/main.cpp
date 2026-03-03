// ZeroCopy — benchmarks three OpenCL buffer strategies on the same workload.
//
// Strategies under test:
//   CL_MEM_COPY_HOST_PTR  — driver copies host data into a new device-owned buffer.
//   CL_MEM_ALLOC_HOST_PTR — driver allocates a new pinned (page-locked) host buffer;
//                           host data is copied into it (combined with COPY_HOST_PTR).
//                           Pinned memory enables faster DMA transfers than pageable memory,
//                           but a copy still occurs — this is NOT zero-copy.
//   CL_MEM_USE_HOST_PTR   — driver aliases the caller's existing pointer directly.
//                           No allocation, no copy: this is the true zero-copy path.
//
// WHY compare these: understanding which strategy avoids allocations and copies
// is critical for latency-sensitive pipelines on integrated and discrete GPUs.
// This tool makes the trade-off visible as concrete millisecond numbers.

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image.h>
#include <stb_image_write.h>

#include "opencl_utils.hpp"   // CL_CHECK, duration_ms, load_kernel_source
#include "ocl_wrapper.hpp"    // create_context(), OclContext

#include <CLI/CLI.hpp>

#include <cstring>
#include <limits>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Helper: build a formatted table row
// ---------------------------------------------------------------------------
static void print_row(const std::string& name, double ms) {
    std::cout << std::left  << std::setw(22) << name
              << std::right << std::fixed << std::setprecision(3) << ms << "\n";
}

// ---------------------------------------------------------------------------
// BenchmarkResult groups the output pixels and the measured kernel time.
// ---------------------------------------------------------------------------
struct BenchmarkResult {
    std::vector<cl_uchar> output_pixels;
    double                kernel_ms{0.0};
};

// ---------------------------------------------------------------------------
// run_strategy — executes the passthrough kernel with the given buffer flags
// and returns the output pixels + kernel execution time in ms.
//
// WHY separate function: each strategy shares identical kernel code; the only
// variable is the cl::Buffer constructor flag.  Keeping it DRY avoids subtle
// copy-paste mistakes in flag selection.
// ---------------------------------------------------------------------------
static BenchmarkResult run_strategy(cl::Context&          ctx,
                                    cl::CommandQueue&      queue,
                                    cl::Kernel&            kernel,
                                    const std::vector<cl_uchar>& host_pixels,
                                    cl_mem_flags           in_flags)
{
    if (host_pixels.size() > static_cast<size_t>(std::numeric_limits<cl_int>::max()))
        throw std::runtime_error("Image too large for cl_int size parameter");
    cl_int size = static_cast<cl_int>(host_pixels.size());

    // Input buffer: created with caller-supplied flags.
    // WHY const_cast: cl.hpp's Buffer ctor takes host_ptr as non-const void*.
    // It does not provide a const-qualified overload, so we must strip const
    // here. The cast is safe: cl.hpp only reads from host_ptr for COPY/USE flags.
    cl::Buffer buf_in(ctx,
                      in_flags,
                      static_cast<size_t>(size),
                      const_cast<cl_uchar*>(host_pixels.data()));

    // Output buffer: simple device-local read/write allocation.
    cl::Buffer buf_out(ctx,
                       CL_MEM_WRITE_ONLY,
                       static_cast<size_t>(size));

    CL_CHECK(kernel.setArg(0, buf_in));
    CL_CHECK(kernel.setArg(1, buf_out));
    CL_CHECK(kernel.setArg(2, size));

    // Dispatch: one work-item per byte so each pixel channel is processed.
    // WHY NDRange from size: the kernel guards against out-of-bounds with an
    // if(gid < size) check, so rounding to workgroup size is safe.
    cl::Event evt;
    CL_CHECK(queue.enqueueNDRangeKernel(
        kernel,
        cl::NullRange,
        cl::NDRange(static_cast<size_t>(size)),
        cl::NullRange,   // implementation-chosen local size
        nullptr,
        &evt));

    // finish() is required before reading profiling timestamps.
    CL_CHECK(queue.finish());

    // Read back for correctness verification.
    BenchmarkResult result;
    result.output_pixels.resize(static_cast<size_t>(size));
    CL_CHECK(queue.enqueueReadBuffer(buf_out, CL_TRUE, 0,
                                     static_cast<size_t>(size),
                                     result.output_pixels.data()));

    result.kernel_ms = duration_ms(evt);
    return result;
}

// ---------------------------------------------------------------------------
// verify — compare first 64 bytes of output against input.
// Exits with code 1 on mismatch so the CI pipeline catches regressions.
// ---------------------------------------------------------------------------
static void verify(const std::string&           strategy_name,
                   const std::vector<cl_uchar>& input,
                   const std::vector<cl_uchar>& output)
{
    const size_t check_bytes = std::min<size_t>(64, input.size());
    if (std::memcmp(input.data(), output.data(), check_bytes) != 0) {
        std::cerr << "[ERROR] Buffer strategy " << strategy_name
                  << " produced incorrect output\n";
        std::exit(1);
    }
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main(int argc, char** argv) {
    // ── CLI ─────────────────────────────────────────────────────────────────
    CLI::App app{"ZeroCopy — OpenCL buffer strategy benchmark"};

    int         width  = 1920;
    int         height = 1080;
    std::string image_path;

    app.add_option("--width",  width,  "Synthetic image width  (default 1920)");
    app.add_option("--height", height, "Synthetic image height (default 1080)");
    app.add_option("--image",  image_path, "Optional input BMP path (omit to use generated gradient)");

    CLI11_PARSE(app, argc, argv);

    // ── Pixel data ──────────────────────────────────────────────────────────
    std::vector<cl_uchar> host_pixels;
    int channels = 4;   // RGBA throughout

    if (!image_path.empty()) {
        // Load from disk, forcing 4 channels so we always work in RGBA.
        int w = 0, h = 0, ch_loaded = 0;
        uint8_t* raw = stbi_load(image_path.c_str(), &w, &h, &ch_loaded, 4);
        if (!raw) {
            throw std::runtime_error("stbi_load failed: " +
                                     std::string(stbi_failure_reason()));
        }
        width  = w;
        height = h;
        const size_t n = static_cast<size_t>(w) * h * 4;
        host_pixels.assign(raw, raw + n);
        stbi_image_free(raw);
    } else {
        // Synthetic RGBA gradient: R=x%256, G=y%256, B=128, A=255.
        // WHY modulo 256: width/height may exceed byte range; wrapping keeps
        // the visual gradient meaningful without clamping logic.
        host_pixels.resize(static_cast<size_t>(width) * height * channels);
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                const size_t idx = (static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)) * channels;
                host_pixels[idx + 0] = static_cast<cl_uchar>(x % 256);
                host_pixels[idx + 1] = static_cast<cl_uchar>(y % 256);
                host_pixels[idx + 2] = 128;
                host_pixels[idx + 3] = 255;
            }
        }
    }

    std::cout << "Image size : " << width << "x" << height
              << " (" << host_pixels.size() << " bytes RGBA)\n";

    // ── OpenCL setup ────────────────────────────────────────────────────────
    // WHY create_context(): honours GPU env var, avoids hard-coded indices.
    OclContext ocl = create_context();

    cl_bool unified_mem = ocl.device.getInfo<CL_DEVICE_HOST_UNIFIED_MEMORY>();
    std::cout << "Host Unified Memory: " << (unified_mem ? "YES (USE_HOST_PTR may be zero-copy)" : "NO") << "\n\n";

    // We need a fresh queue with profiling enabled — create_context() builds
    // one without CL_QUEUE_PROFILING_ENABLE, so we construct our own here.
    cl::CommandQueue prof_queue(ocl.context, ocl.device,
                                CL_QUEUE_PROFILING_ENABLE);

    // ── Kernel compilation ──────────────────────────────────────────────────
    // Kernel file is placed next to the binary by the POST_BUILD cmake rule.
    const std::string kernel_src =
        load_kernel_source("kernels/zero_copy_kernel.cl");

    cl::Program program(ocl.context,
                        cl::Program::Sources{kernel_src});
    try {
        program.build({ocl.device});
    } catch (const cl::Error&) {
        std::cerr << "Build log:\n"
                  << program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(ocl.device)
                  << "\n";
        throw;
    }

    cl::Kernel kernel(program, "passthrough");

    // ── Run three strategies ─────────────────────────────────────────────────
    auto result_copy  = run_strategy(ocl.context, prof_queue, kernel,
                                     host_pixels, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR);
    verify("COPY_HOST_PTR",  host_pixels, result_copy.output_pixels);

    // ALLOC_HOST_PTR allocates pinned (page-locked) host memory for DMA-friendly transfers.
    // COPY_HOST_PTR is added to pre-populate it from the host vector — without it,
    // the allocated buffer would be uninitialized and require a separate enqueueWriteBuffer.
    auto result_alloc = run_strategy(ocl.context, prof_queue, kernel,
                                     host_pixels, CL_MEM_READ_ONLY | CL_MEM_ALLOC_HOST_PTR | CL_MEM_COPY_HOST_PTR);
    verify("ALLOC_HOST_PTR", host_pixels, result_alloc.output_pixels);

    // WHY CL_MEM_USE_HOST_PTR requires CL_MEM_READ_ONLY to avoid UB on
    // concurrent host writes: we only read in the kernel; adding WRITE to the
    // device while the host also owns the pointer is unsafe.
    auto result_use   = run_strategy(ocl.context, prof_queue, kernel,
                                     host_pixels, CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR);
    verify("USE_HOST_PTR",   host_pixels, result_use.output_pixels);

    // ── Timing table ────────────────────────────────────────────────────────
    std::cout << "\n";
    std::cout << std::left << std::setw(22) << "Strategy"
              << "Kernel Time (ms)\n";
    std::cout << std::string(38, '-') << "\n";
    print_row("COPY_HOST_PTR",  result_copy.kernel_ms);
    print_row("ALLOC_HOST_PTR", result_alloc.kernel_ms);
    print_row("USE_HOST_PTR",   result_use.kernel_ms);

    // ── Write output BMP ─────────────────────────────────────────────────────
    // Use ALLOC_HOST_PTR result as the canonical output image.
    // WHY ALLOC_HOST_PTR: it is the most portable pinned-memory path; the
    // output data has already been read back to host via enqueueReadBuffer.
    if (!stbi_write_bmp("output.bmp",
                        width, height, channels,
                        result_alloc.output_pixels.data())) {
        throw std::runtime_error("stbi_write_bmp failed for output.bmp");
    }
    std::cout << "\nOutput written to output.bmp\n";

    return 0;
}
