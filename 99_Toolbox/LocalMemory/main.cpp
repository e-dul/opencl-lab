// local_mem_demo — benchmarks global-memory vs local-memory (tile+halo) box blur.
//
// Objective: make the cost of redundant global reads *visible* as concrete
// millisecond numbers.  A good discrete GPU will show ≥3× speedup at radius 5.
//
// WHY two separate output buffers: we need byte-identical results from both
// kernels to confirm correctness before the output.bmp write.  Using separate
// buffers avoids any ordering ambiguity between the two dispatches.

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

#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

// Fixed work-group edge size.  Must match the LOCAL_SIZE define injected into
// the kernel.  If you change this, update the build option string too.
static constexpr int LOCAL_SIZE = 16;

// ---------------------------------------------------------------------------
// round_up — smallest multiple of base that is >= n.
// WHY needed: NDRange dimensions must be multiples of local_work_size so the
// driver can partition the grid evenly.  The kernel guards out-of-bound ids.
// ---------------------------------------------------------------------------
static size_t round_up(size_t n, size_t base) {
    return ((n + base - 1) / base) * base;
}

// ---------------------------------------------------------------------------
// print_table_row — fixed-width row for the timing table.
// ---------------------------------------------------------------------------
static void print_row(const std::string& name, double ms, double speedup) {
    std::cout << std::left  << std::setw(24) << name
              << std::right << std::fixed << std::setprecision(3)
              << std::setw(16) << ms
              << std::setw(12) << speedup << "x\n";
}

// ---------------------------------------------------------------------------
// run_blur — dispatches one blur kernel and returns the kernel time in ms.
// Output pixels are written into the caller-supplied host vector.
// ---------------------------------------------------------------------------
static double run_blur(cl::CommandQueue& queue,
                       cl::Kernel&       kernel,
                       cl::Buffer&       buf_in,
                       cl::Buffer&       buf_out,
                       int               width,
                       int               height,
                       int               radius,
                       std::vector<cl_uchar>& host_out)
{
    if (static_cast<size_t>(width) > static_cast<size_t>(std::numeric_limits<cl_int>::max()) ||
        static_cast<size_t>(height) > static_cast<size_t>(std::numeric_limits<cl_int>::max())) {
        throw std::runtime_error("Image dimensions exceed cl_int range");
    }

    CL_CHECK(kernel.setArg(0, buf_in));
    CL_CHECK(kernel.setArg(1, buf_out));
    CL_CHECK(kernel.setArg(2, (cl_int)width));
    CL_CHECK(kernel.setArg(3, (cl_int)height));
    CL_CHECK(kernel.setArg(4, (cl_int)radius));

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

    // finish() is required before querying profiling timestamps.
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
    CLI::App app{"LocalMemory — global vs local (tile+halo) box blur benchmark"};

    int width  = 1920;
    int height = 1080;
    int radius = 5;

    app.add_option("--width",  width,  "Image width  (default 1920)");
    app.add_option("--height", height, "Image height (default 1080)");
    app.add_option("--radius", radius, "Box blur radius (default 5)");

    CLI11_PARSE(app, argc, argv);

    if (width <= 0 || height <= 0 || radius <= 0) {
        std::cerr << "[ERROR] width, height, and radius must all be > 0\n";
        return 1;
    }

    // ── OpenCL context ──────────────────────────────────────────────────────
    // WHY create_context(): honours GPU env var, avoids hard-coded device indices.
    OclContext ocl = create_context();

    // create_context() does not enable profiling — create a fresh queue here.
    cl::CommandQueue prof_queue(ocl.context, ocl.device, CL_QUEUE_PROFILING_ENABLE);

    // ── Local memory guard ──────────────────────────────────────────────────
    // Query device limit and ensure the tile fits within 80% of it.
    // WHY 80%: leave headroom for the driver's own __local usage (stack, barriers).
    cl_ulong local_mem_bytes = ocl.device.getInfo<CL_DEVICE_LOCAL_MEM_SIZE>();
    std::cout << "Device local mem : " << local_mem_bytes / 1024 << " KB\n";

    // Tile footprint in bytes: (LOCAL_SIZE + 2*radius)^2 × sizeof(uchar)
    auto tile_bytes = [](int r) -> size_t {
        size_t dim = static_cast<size_t>(LOCAL_SIZE) + 2 * static_cast<size_t>(r);
        return dim * dim;   // uchar, 1 byte each
    };

    const double LIMIT_FRACTION = 0.80;
    while (radius > 0 && tile_bytes(radius) > static_cast<size_t>(local_mem_bytes * LIMIT_FRACTION)) {
        int old = radius;
        --radius;
        std::cout << "[WARN] Tile (" << (LOCAL_SIZE + 2*old) << "x" << (LOCAL_SIZE + 2*old)
                  << "=" << tile_bytes(old) << " B) exceeds 80% of local mem ("
                  << static_cast<size_t>(local_mem_bytes * LIMIT_FRACTION) << " B)."
                  << " Reducing radius " << old << " → " << radius << "\n";
    }
    if (radius <= 0) {
        std::cerr << "[ERROR] radius reduced to 0 — local memory too small for any tile.\n";
        return 1;
    }
    std::cout << "Blur radius      : " << radius << "\n";

    // ── Synthetic input ─────────────────────────────────────────────────────
    // Deterministic gradient: value = (x*3 + y*7) % 256.
    const size_t pixel_count = static_cast<size_t>(width) * static_cast<size_t>(height);
    std::vector<cl_uchar> host_input(pixel_count);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            host_input[static_cast<size_t>(y) * static_cast<size_t>(width) + x] =
                static_cast<cl_uchar>((x * 3 + y * 7) % 256);
        }
    }
    std::cout << "Image size       : " << width << "x" << height
              << " (" << pixel_count << " bytes grayscale)\n\n";

    // ── Buffers ─────────────────────────────────────────────────────────────
    // WHY CL_MEM_COPY_HOST_PTR on input: driver copies host data into a
    // device-local buffer up front, so both kernels share the same uploaded data.
    cl::Buffer buf_in(ocl.context,
                      CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                      pixel_count,
                      host_input.data());

    // Separate output buffers so correctness check is unambiguous.
    cl::Buffer buf_out_global(ocl.context, CL_MEM_WRITE_ONLY, pixel_count);
    cl::Buffer buf_out_local (ocl.context, CL_MEM_WRITE_ONLY, pixel_count);

    // ── Kernel program ──────────────────────────────────────────────────────
    // WHY load at runtime: keeps .cl files editable without recompilation.
    const std::string src = load_kernel_source("kernels/blur_kernel.cl");
    cl::Program program(ocl.context, cl::Program::Sources{src});

    // Inject LOCAL_SIZE and MAX_RADIUS as compile-time constants so that the
    // __local tile array size resolves at kernel compile time (required by
    // OpenCL 1.2 — variable-length __local arrays are not standard).
    const std::string build_opts =
        "-D LOCAL_SIZE=" + std::to_string(LOCAL_SIZE) +
        " -D MAX_RADIUS=" + std::to_string(radius);

    try {
        program.build({ocl.device}, build_opts.c_str());
    } catch (const cl::Error&) {
        std::cerr << "Kernel build log:\n"
                  << program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(ocl.device) << "\n";
        throw;
    }

    cl::Kernel kernel_global(program, "blur_global");
    cl::Kernel kernel_local (program, "blur_local");

    // ── Benchmark runs ───────────────────────────────────────────────────────
    std::vector<cl_uchar> out_global, out_local;

    const double ms_global = run_blur(prof_queue, kernel_global,
                                      buf_in, buf_out_global,
                                      width, height, radius, out_global);

    const double ms_local  = run_blur(prof_queue, kernel_local,
                                      buf_in, buf_out_local,
                                      width, height, radius, out_local);

    // ── Timing table ────────────────────────────────────────────────────────
    const double speedup = (ms_local > 0.0) ? (ms_global / ms_local) : 0.0;

    std::cout << std::left  << std::setw(24) << "Variant"
              << std::right << std::setw(16) << "Kernel Time (ms)"
              << std::setw(12) << "Speedup" << "\n";
    // Use ASCII dashes — portable across all terminals
    std::cout << std::string(52, '-') << "\n";
    print_row("Global memory blur", ms_global, 1.0);
    print_row("Local memory blur",  ms_local,  speedup);
    std::cout << "\n";

    // ── Pixel correctness check ──────────────────────────────────────────────
    // Both kernels must produce byte-identical output for the same (width, height,
    // radius) so the benchmark is meaningful and not masking a kernel bug.
    int mismatch_count = 0;
    for (size_t i = 0; i < pixel_count; ++i) {
        if (out_global[i] != out_local[i]) {
            int px = static_cast<int>(i % static_cast<size_t>(width));
            int py = static_cast<int>(i / static_cast<size_t>(width));
            std::cerr << "[ERROR] output mismatch at pixel (" << px << ", " << py << ")"
                      << "  global=" << (int)out_global[i]
                      << "  local="  << (int)out_local[i]  << "\n";
            ++mismatch_count;
            if (mismatch_count >= 10) {
                std::cerr << "[ERROR] (further mismatches suppressed)\n";
                throw std::runtime_error("Output mismatch: first 10 shown above");
            }
        }
    }
    if (mismatch_count > 0) {
        throw std::runtime_error("Output mismatch: " + std::to_string(mismatch_count) + " pixel(s)");
    }

    // ── Write output BMP ─────────────────────────────────────────────────────
    // Use local-memory result as the canonical output.
    // save_bmp wraps stbi_write_bmp and throws on failure (from image_utils.hpp).
    save_bmp("output.bmp",
             std::vector<uint8_t>(out_local.begin(), out_local.end()),
             width, height, 1);
    std::cout << "Output written to output.bmp\n";

    return 0;
}
