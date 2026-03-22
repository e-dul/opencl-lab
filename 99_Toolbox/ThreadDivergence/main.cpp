// divergence_demo — benchmarks branching if-else vs branchless select() blur.
//
// Objective: make thread-divergence cost visible as concrete millisecond
// numbers.  On a discrete GPU with a half-masked image (mask_density=0.5),
// the select() variant should be measurably faster because every thread in
// a warp follows the same instruction stream.
//
// WHY two separate output buffers: we verify byte-identical results from
// both kernels before writing BMPs, ensuring the benchmark is not masking
// a correctness bug.

// stb implementations must be defined exactly once per binary before image_utils.hpp.
// WHY both STB_IMAGE_IMPLEMENTATION and STB_IMAGE_WRITE_IMPLEMENTATION: image_utils.hpp
// declares load_rgb_image() which references stbi_load/stbi_image_free even though this
// tool only calls save_bmp().  The compiler sees the full header so both symbols must exist.
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image.h>
#include <stb_image_write.h>
#include "image_utils.hpp"   // save_bmp()

#include "opencl_utils.hpp"   // CL_CHECK, duration_ms, load_kernel_source
#include "ocl_wrapper.hpp"    // create_context(), OclContext

#include <CLI/CLI.hpp>

#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

// Fixed work-group edge size.  Both dimensions of local NDRange use this value.
static constexpr int LOCAL_SIZE = 16;

// ---------------------------------------------------------------------------
// round_up — smallest multiple of base that is >= n.
// WHY needed: NDRange global size must be a multiple of local size so the
// driver can partition the grid evenly; the kernel guards out-of-bound ids.
// ---------------------------------------------------------------------------
static size_t round_up(size_t n, size_t base) {
    return ((n + base - 1) / base) * base;
}

// ---------------------------------------------------------------------------
// print_row — fixed-width table row for the timing report.
// ---------------------------------------------------------------------------
static void print_row(const std::string& name, double ms, double speedup) {
    std::cout << std::left  << std::setw(24) << name
              << std::right << std::fixed << std::setprecision(3)
              << std::setw(16) << ms
              << std::setw(12) << speedup << "x\n";
}

// ---------------------------------------------------------------------------
// run_kernel — dispatches one divergence kernel, waits for completion, reads
// results back into host_out, and returns the GPU kernel time in ms.
// ---------------------------------------------------------------------------
static double run_kernel(cl::CommandQueue&      queue,
                         cl::Kernel&            kernel,
                         cl::Buffer&            buf_in,
                         cl::Buffer&            buf_mask,
                         cl::Buffer&            buf_out,
                         int                    width,
                         int                    height,
                         std::vector<cl_uchar>& host_out)
{
    // Overflow guard: width and height must fit in cl_int for the kernel args.
    if (static_cast<size_t>(width)  > static_cast<size_t>(std::numeric_limits<cl_int>::max()) ||
        static_cast<size_t>(height) > static_cast<size_t>(std::numeric_limits<cl_int>::max())) {
        throw std::runtime_error("Image dimensions exceed cl_int range");
    }

    CL_CHECK(kernel.setArg(0, buf_in));
    CL_CHECK(kernel.setArg(1, buf_mask));
    CL_CHECK(kernel.setArg(2, buf_out));
    CL_CHECK(kernel.setArg(3, static_cast<cl_int>(width)));
    CL_CHECK(kernel.setArg(4, static_cast<cl_int>(height)));

    // Pad global size to the next multiple of LOCAL_SIZE so the work-group
    // partitioning is always complete; the kernel itself discards OOB ids.
    size_t gw = round_up(static_cast<size_t>(width),  LOCAL_SIZE);
    size_t gh = round_up(static_cast<size_t>(height), LOCAL_SIZE);

    cl::Event evt;
    CL_CHECK(queue.enqueueNDRangeKernel(
        kernel,
        cl::NullRange,
        cl::NDRange(gw, gh),
        cl::NDRange(LOCAL_SIZE, LOCAL_SIZE),
        nullptr,
        &evt));

    // finish() required before reading profiling timestamps.
    CL_CHECK(queue.finish());

    const size_t byte_count = static_cast<size_t>(width) * static_cast<size_t>(height);
    host_out.resize(byte_count);
    CL_CHECK(queue.enqueueReadBuffer(buf_out, CL_TRUE, 0, byte_count, host_out.data()));

    return duration_ms(evt);
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main(int argc, char** argv) {
    // ── CLI ─────────────────────────────────────────────────────────────────
    CLI::App app{"ThreadDivergence — branching if-else vs branchless select() blur benchmark"};

    int   width        = 1920;
    int   height       = 1080;
    float mask_density = 0.5f;

    app.add_option("--width",        width,        "Image width  (default 1920)");
    app.add_option("--height",       height,       "Image height (default 1080)");
    app.add_option("--mask-density", mask_density, "Fraction of pixels in blur class 0.0-1.0 (default 0.5)")
       ->check(CLI::Range(0.0f, 1.0f));

    CLI11_PARSE(app, argc, argv);

    if (width <= 0 || height <= 0) {
        throw std::runtime_error("width and height must be > 0");
    }

    // ── OpenCL context ──────────────────────────────────────────────────────
    // WHY create_context(): honours GPU env var, avoids hard-coded device indices.
    OclContext ocl = create_context();

    // create_context() doesn't enable profiling — create a dedicated queue.
    // WHY CL_QUEUE_PROFILING_ENABLE: without this flag, getProfilingInfo()
    // returns CL_PROFILING_INFO_NOT_AVAILABLE.
    cl::CommandQueue prof_queue(ocl.context, ocl.device, CL_QUEUE_PROFILING_ENABLE);

    // ── Synthetic input data ─────────────────────────────────────────────────
    // Deterministic gradient: value = (x*3 + y*7) % 256.
    // WHY static_cast<size_t>(width) * height: avoids int overflow before cast.
    const size_t pixel_count = static_cast<size_t>(width) * static_cast<size_t>(height);
    std::vector<cl_uchar> host_input(pixel_count);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            host_input[static_cast<size_t>(y) * static_cast<size_t>(width) + x] =
                static_cast<cl_uchar>((x * 3 + y * 7) % 256);
        }
    }

    // Binary mask: 0 = blur pixel, 1 = copy pixel.
    // srand(42) ensures reproducible results across runs for correctness checks.
    std::vector<cl_uchar> host_mask(pixel_count);
    std::srand(42);
    for (size_t i = 0; i < pixel_count; ++i) {
        host_mask[i] = (static_cast<float>(std::rand()) / RAND_MAX < mask_density) ? 0 : 1;
    }

    std::cout << "Image size       : " << width << "x" << height
              << " (" << pixel_count << " bytes grayscale)\n";
    std::cout << "Mask density     : " << std::fixed << std::setprecision(2)
              << mask_density << " (" << static_cast<int>(mask_density * 100) << "% blur pixels)\n\n";

    // ── Buffers ─────────────────────────────────────────────────────────────
    // WHY CL_MEM_COPY_HOST_PTR on input and mask: copies host data into
    // device-local memory at construction time so both kernel dispatches
    // share the same uploaded data without extra enqueueWriteBuffer calls.
    cl::Buffer buf_in(ocl.context,
                      CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                      pixel_count,
                      host_input.data());

    cl::Buffer buf_mask(ocl.context,
                        CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                        pixel_count,
                        host_mask.data());

    // Separate output buffers so the correctness check is unambiguous.
    cl::Buffer buf_out_ifelse(ocl.context, CL_MEM_WRITE_ONLY, pixel_count);
    cl::Buffer buf_out_select(ocl.context, CL_MEM_WRITE_ONLY, pixel_count);

    // ── Kernel program ──────────────────────────────────────────────────────
    // WHY load at runtime: keeps .cl files editable without host recompilation.
    const std::string src = load_kernel_source("kernels/divergence_kernel.cl");
    cl::Program program(ocl.context, cl::Program::Sources{src});

    try {
        program.build({ocl.device});
    } catch (const cl::Error&) {
        std::cerr << "Kernel build log:\n"
                  << program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(ocl.device) << "\n";
        throw;
    }

    cl::Kernel kernel_ifelse(program, "blur_ifelse");
    cl::Kernel kernel_select(program, "blur_select");

    // ── Benchmark runs ───────────────────────────────────────────────────────
    std::vector<cl_uchar> out_ifelse, out_select;

    const double ms_ifelse = run_kernel(prof_queue, kernel_ifelse,
                                        buf_in, buf_mask, buf_out_ifelse,
                                        width, height, out_ifelse);

    const double ms_select = run_kernel(prof_queue, kernel_select,
                                        buf_in, buf_mask, buf_out_select,
                                        width, height, out_select);

    // ── Timing table ─────────────────────────────────────────────────────────
    // speedup = how many times faster select() is vs if-else.
    // base is the slower if-else variant (always 1.00x).
    const double speedup = (ms_select > 0.0) ? (ms_ifelse / ms_select) : 0.0;

    std::cout << std::left  << std::setw(24) << "Variant"
              << std::right << std::setw(16) << "Kernel Time (ms)"
              << std::setw(12) << "Speedup" << "\n";
    std::cout << std::string(52, '-') << "\n";
    print_row("IF-ELSE  (divergent)",  ms_ifelse, 1.0);
    print_row("SELECT() (branchless)", ms_select, speedup);
    std::cout << "\n";

    // ── Pixel correctness check ──────────────────────────────────────────────
    // Both kernels implement the same logical operation (mask-conditional blur)
    // so their outputs must be byte-identical.  Any mismatch indicates a kernel
    // bug rather than a real divergence effect.
    for (size_t i = 0; i < pixel_count; ++i) {
        if (out_ifelse[i] != out_select[i]) {
            int px = static_cast<int>(i % static_cast<size_t>(width));
            int py = static_cast<int>(i / static_cast<size_t>(width));
            throw std::runtime_error(
                "[ERROR] pixel mismatch at (" + std::to_string(px) + ", " + std::to_string(py) +
                "): ifelse=" + std::to_string(static_cast<int>(out_ifelse[i])) +
                " select=" + std::to_string(static_cast<int>(out_select[i])));
        }
    }
    std::cout << "Pixel correctness: PASS (both outputs are byte-identical)\n";

    // ── Write output BMPs ────────────────────────────────────────────────────
    // Write grayscale (1 channel) BMP; both files should look visually identical
    // (blurred region + identity region visible).
    // save_bmp wraps stbi_write_bmp and throws on failure (from image_utils.hpp).
    save_bmp("output_ifelse.bmp",
             std::vector<uint8_t>(out_ifelse.begin(), out_ifelse.end()),
             width, height, 1);
    save_bmp("output_select.bmp",
             std::vector<uint8_t>(out_select.begin(), out_select.end()),
             width, height, 1);
    std::cout << "Outputs          : output_ifelse.bmp, output_select.bmp\n";

    return 0;
}
