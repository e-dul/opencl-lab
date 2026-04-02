// CoalescedAccess — benchmarks three OpenCL memory access patterns.
//
// Patterns under test (all on a 2D float array, width x height):
//   row_major   — reads/writes element[y * width + x]: adjacent threads hit
//                 adjacent addresses → hardware merges into one transaction.
//   col_major   — reads/writes element[x * height + y]: adjacent threads hit
//                 addresses `height` apart → every thread misses the cache.
//   transposed  — reads row-major (coalesced), writes column-major (scattered):
//                 a common layout-fix pattern that trades write scatter for
//                 read coalescing on the hot path.
//
// WHY compare these: memory coalescing is the single most impactful GPU memory
// optimisation. This tool makes the 5-7× performance gap visible as concrete
// millisecond numbers from cl::Event profiling.

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

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>


// ---------------------------------------------------------------------------
// print_row — one formatted timing row in the results table.
// ---------------------------------------------------------------------------
static void print_row(const std::string& name, double ms) {
    std::cout << std::left  << std::setw(26) << name
              << std::right << std::fixed << std::setprecision(3) << ms << "\n";
}

// ---------------------------------------------------------------------------
// run_kernel — dispatches a single named kernel with a 2D NDRange.
// Returns the GPU kernel time in ms via cl::Event profiling.
//
// WHY separate function: all three variants share the same dispatch mechanics;
// only the kernel object differs. Factoring out avoids copy-paste mistakes
// in NDRange construction and event readback.
// ---------------------------------------------------------------------------
static double run_kernel(cl::CommandQueue&  queue,
                         cl::Kernel&        kernel,
                         int                width,
                         int                height,
                         cl::Buffer&        buf_in,
                         cl::Buffer&        buf_out)
{
    CL_CHECK(kernel.setArg(0, buf_in));
    CL_CHECK(kernel.setArg(1, buf_out));
    CL_CHECK(kernel.setArg(2, width));
    CL_CHECK(kernel.setArg(3, height));

    // Pad global size to a multiple of local size so the runtime accepts the
    // NDRange. Kernels guard against touching padded work-items internally.
    const cl::NDRange global{round_up(width, 16), round_up(height, 16)};
    const cl::NDRange local{16, 16};

    cl::Event evt;
    CL_CHECK(queue.enqueueNDRangeKernel(
        kernel,
        cl::NullRange,
        global,
        local,
        nullptr,
        &evt));

    // finish() must complete before profiling timestamps are valid.
    CL_CHECK(queue.finish());

    return duration_ms(evt);
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main(int argc, char** argv) {
    // ── CLI ─────────────────────────────────────────────────────────────────
    CLI::App app{"CoalescedAccess — OpenCL memory coalescing benchmark"};

    int width  = 1920;
    int height = 1080;

    app.add_option("--width",  width,  "Array width  (default 1920)");
    app.add_option("--height", height, "Array height (default 1080)");

    CLI11_PARSE(app, argc, argv);

    if (width <= 0 || height <= 0)
        throw std::runtime_error("--width and --height must be positive.");

    // Guard against integer overflow before allocating buffers.
    // WHY check before cast: width * height as int32 overflows at ~46K x 46K.
    if (static_cast<size_t>(width) * static_cast<size_t>(height) >
        static_cast<size_t>(std::numeric_limits<int>::max())) {
        throw std::runtime_error("Array too large for cl_int size parameters.");
    }

    const size_t n_elements = static_cast<size_t>(width) * height;
    const size_t byte_size  = n_elements * sizeof(float);

    // ── Host data ────────────────────────────────────────────────────────────
    // Gradient: value in [0, 1] so float → uchar conversion is meaningful.
    std::vector<float> host_input(n_elements);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            host_input[static_cast<size_t>(y) * width + x] =
                static_cast<float>(x + y) / static_cast<float>(width + height);
        }
    }

    std::cout << "Array size : " << width << "x" << height
              << " (" << n_elements << " floats, "
              << byte_size / 1024 / 1024 << " MiB)\n";

    // ── OpenCL setup ─────────────────────────────────────────────────────────
    // WHY create_context(): honours GPU env var, avoids hard-coded indices.
    OclContext ocl = create_context();

    // create_context() builds a queue without profiling; construct a fresh one.
    // WHY CL_QUEUE_PROFILING_ENABLE: cl::Event timestamps require this flag
    // on the queue — without it getProfilingInfo returns CL_PROFILING_INFO_NOT_AVAILABLE.
    cl::CommandQueue prof_queue(ocl.context, ocl.device,
                                CL_QUEUE_PROFILING_ENABLE);

    // ── Buffers ───────────────────────────────────────────────────────────────
    // Input is uploaded once; each kernel variant writes to its own output so
    // we can read all three back independently for correctness verification.
    // WHY CL_MEM_COPY_HOST_PTR: uploads host_input immediately at buffer creation time;
    // the host vector is safe to modify or destroy after this call returns.
    // WHY CL_MEM_READ_ONLY: restricts device writes to this buffer, allowing the driver
    // to place it in read-only cache paths on hardware that benefits from the hint.
    cl::Buffer buf_in(ocl.context,
                      CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                      byte_size,
                      host_input.data());

    // Three output buffers, one per variant.
    cl::Buffer buf_row  (ocl.context, CL_MEM_WRITE_ONLY, byte_size);
    cl::Buffer buf_col  (ocl.context, CL_MEM_WRITE_ONLY, byte_size);
    cl::Buffer buf_trans(ocl.context, CL_MEM_WRITE_ONLY, byte_size);

    // ── Kernel compilation ───────────────────────────────────────────────────
    // Kernel file is placed next to the binary by the POST_BUILD cmake rule.
    const std::string kernel_src =
        load_kernel_source("kernels/coalesced_kernel.cl");

    cl::Program program(ocl.context, cl::Program::Sources{kernel_src});
    try {
        program.build({ocl.device});
    } catch (const cl::Error&) {
        std::cerr << "Kernel build log:\n"
                  << program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(ocl.device)
                  << "\n";
        throw;
    }

    cl::Kernel k_row  (program, "row_major");
    cl::Kernel k_col  (program, "col_major");
    cl::Kernel k_trans(program, "transposed");

    // ── Run variants ─────────────────────────────────────────────────────────
    const double ms_row   = run_kernel(prof_queue, k_row,   width, height, buf_in, buf_row);
    const double ms_col   = run_kernel(prof_queue, k_col,   width, height, buf_in, buf_col);
    const double ms_trans = run_kernel(prof_queue, k_trans, width, height, buf_in, buf_trans);

    // ── Timing table ─────────────────────────────────────────────────────────
    std::cout << "\n";
    std::cout << std::left << std::setw(26) << "Variant"
              << "Kernel Time (ms)\n";
    std::cout << std::string(42, '-') << "\n";
    print_row("ROW-MAJOR (coalesced)",   ms_row);
    print_row("COL-MAJOR (uncoalesced)", ms_col);
    print_row("TRANSPOSED (fixed)",      ms_trans);
    std::cout << "\n";

    // ── Read back ─────────────────────────────────────────────────────────────
    std::vector<float> out_row  (n_elements);
    std::vector<float> out_col  (n_elements);
    std::vector<float> out_trans(n_elements);

    CL_CHECK(prof_queue.enqueueReadBuffer(buf_row,   CL_TRUE, 0, byte_size, out_row.data()));
    CL_CHECK(prof_queue.enqueueReadBuffer(buf_col,   CL_TRUE, 0, byte_size, out_col.data()));
    CL_CHECK(prof_queue.enqueueReadBuffer(buf_trans, CL_TRUE, 0, byte_size, out_trans.data()));

    // ── Correctness check ─────────────────────────────────────────────────────
    // row_major and col_major both compute element * 2 using their own layout,
    // so after readback each output[i] == host_input[i] * 2.
    // They must be byte-identical across all n_elements.
    //
    // transposed writes a different layout (col-major output), so its raw
    // buffer is NOT directly comparable to row_major without re-transposing.
    // The correctness of transposed is verified separately via re-transposition.
    // WHY flat comparison is valid for col_major: the col_major kernel applies
    // only an element-wise scalar multiply (x2) with no cross-element index
    // remapping — every output[i] equals input[i] * 2 regardless of the
    // column-major address formula used inside the kernel. The layout difference
    // affects only which physical address a work-item reads/writes; the logical
    // mapping i -> i is preserved end-to-end, so out_col[i] == host_input[i] * 2
    // is a correct flat-index check.
    bool mismatch = false;
    for (size_t i = 0; i < n_elements; ++i) {
        const float expected = host_input[i] * 2.0f;
        if (out_row[i] != expected) {
            std::cerr << "[ERROR] ROW-MAJOR output mismatch at index " << i
                      << " (got " << out_row[i] << ", expected " << expected << ")\n";
            mismatch = true;
            break;
        }
        if (out_col[i] != expected) {
            std::cerr << "[ERROR] COL-MAJOR output mismatch at index " << i
                      << " (got " << out_col[i] << ", expected " << expected << ")\n";
            mismatch = true;
            break;
        }
    }

    // Verify transposed: re-transpose the output back to row-major and compare.
    // WHY: transposed kernel stores result[x * height + y] = input[y * width + x] * 2,
    // so to recover row-major layout on host we read out_trans[x * height + y]
    // and expect host_input[y * width + x] * 2.
    for (int y = 0; y < height && !mismatch; ++y) {
        for (int x = 0; x < width && !mismatch; ++x) {
            const float expected  = host_input[static_cast<size_t>(y) * width + x] * 2.0f;
            const float got_trans = out_trans[static_cast<size_t>(x) * height + y];
            if (got_trans != expected) {
                std::cerr << "[ERROR] TRANSPOSED output mismatch at ("
                          << x << "," << y << ")"
                          << " (got " << got_trans << ", expected " << expected << ")\n";
                mismatch = true;
            }
        }
    }

    if (mismatch) throw std::runtime_error("Output correctness check failed.");

    std::cout << "Correctness: PASS (all variants produce expected output)\n";

    // ── Write output.bmp ──────────────────────────────────────────────────────
    // Use row_major result as the reference output image.
    // Convert float [0.0, 1.0] → uchar [0, 255] for BMP grayscale.
    std::vector<unsigned char> pixels(n_elements);
    for (size_t i = 0; i < n_elements; ++i) {
        // clamp before cast to avoid UB on out-of-range floats.
        const float clamped = std::fmin(std::fmax(out_row[i], 0.0f), 1.0f);
        pixels[i] = static_cast<unsigned char>(clamped * 255.0f + 0.5f);
    }

    // save_bmp wraps stbi_write_bmp and throws on failure (from image_utils.hpp).
    save_bmp("output.bmp", pixels, width, height, 1);
    std::cout << "Output      : output.bmp (grayscale, row-major result)\n";

    return 0;
}
