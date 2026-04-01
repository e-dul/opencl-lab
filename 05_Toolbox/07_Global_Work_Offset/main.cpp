// global_work_offset_demo — benchmarks tiled ROI dispatch vs. full-frame dispatch.
//
// Objective: make the thread-count reduction from global_work_offset *visible*
// as concrete millisecond numbers.  A 256×256 tile over a 4K frame dispatches
// ~430× fewer threads than the full frame; the timing table shows this directly.
//
// WHY no BMP output: the artifact is a structured console timing table from cl::Event
// profiling — the numeric-benchmark exception applies (purely synthetic workload,
// no image content to inspect visually).

#include "opencl_utils.hpp"   // CL_CHECK, duration_ms, load_kernel_source, get_binary_dir, round_up
#include "ocl_wrapper.hpp"    // create_context(), OclContext

#include <CLI/CLI.hpp>

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// bench_run — dispatch roi_invert once, return kernel time in ms via cl::Event.
// global_work_offset and global_work_size control the dispatch window.
// ---------------------------------------------------------------------------
static double bench_run(cl::CommandQueue& queue,
                         cl::Kernel&       kernel,
                         cl::Buffer&       buf_in,
                         cl::Buffer&       buf_out,
                         int               width,
                         int               height,
                         int               offset_x,
                         int               offset_y,
                         int               tile_x,
                         int               tile_y)
{
    CL_CHECK(kernel.setArg(0, buf_in));
    CL_CHECK(kernel.setArg(1, buf_out));
    CL_CHECK(kernel.setArg(2, static_cast<cl_int>(width)));
    CL_CHECK(kernel.setArg(3, static_cast<cl_int>(height)));
    // WHY no offset_x/offset_y setArg: the kernel reads these via get_global_offset(),
    // which is always in sync with the cl::NDRange offset passed to enqueueNDRangeKernel.
    CL_CHECK(kernel.setArg(4, static_cast<cl_int>(tile_x)));
    CL_CHECK(kernel.setArg(5, static_cast<cl_int>(tile_y)));

    // WHY local_size 16: a 16×16 work-group (256 threads) is the most common
    // well-occupying tile on discrete GPUs; it divides evenly into common image
    // dimensions and fits comfortably within the 256–1024 thread limit.
    // ALTERNATIVE: pass cl::NullRange here — the runtime picks the work-group size,
    // no rounding is needed, and tile_w/tile_h kernel params become unnecessary.
    const size_t LOCAL_SIZE = 16;

    // Round up to LOCAL_SIZE boundary so global_work_size is always a multiple
    // of local_work_size — required by OpenCL 1.2 when local size is explicit.
    size_t gw = round_up(static_cast<size_t>(tile_x), LOCAL_SIZE);
    size_t gh = round_up(static_cast<size_t>(tile_y), LOCAL_SIZE);

    // WHY cl::NDRange for offset: global_work_offset shifts the dispatch window
    // so that work-item IDs seen inside the kernel are ABSOLUTE image coordinates
    // (per OpenCL 1.2 spec §6.11.1).  With offset={offset_x, offset_y}, the first
    // work-item receives gx=offset_x and gy=offset_y — ready to use as buffer indices
    // without any further addition.  This is the canonical way to process an ROI
    // without launching dead threads for pixels outside the window.
    cl::NDRange offset(static_cast<size_t>(offset_x), static_cast<size_t>(offset_y));
    cl::NDRange global(gw, gh);
    cl::NDRange local(LOCAL_SIZE, LOCAL_SIZE);

    cl::Event evt;
    CL_CHECK(queue.enqueueNDRangeKernel(kernel, offset, global, local, nullptr, &evt));
    // finish() must precede getProfilingInfo — timestamps are undefined until
    // the command completes.
    CL_CHECK(queue.finish());

    return duration_ms(evt);
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main(int argc, char** argv) {
    // ── CLI ─────────────────────────────────────────────────────────────────
    CLI::App app{"GlobalWorkOffset — ROI tile vs full-frame dispatch benchmark"};

    int width      = 3840;
    int height     = 2160;
    int tile_x     = 256;
    int tile_y     = 256;
    int offset_x   = 0;
    int offset_y   = 0;
    int iterations = 10;

    app.add_option("--width",      width,      "Full image width  (default 3840)");
    app.add_option("--height",     height,     "Full image height (default 2160)");
    app.add_option("--tile-x",     tile_x,     "ROI tile width    (default 256)");
    app.add_option("--tile-y",     tile_y,     "ROI tile height   (default 256)");
    app.add_option("--offset-x",   offset_x,   "ROI start column  (default 0)");
    app.add_option("--offset-y",   offset_y,   "ROI start row     (default 0)");
    app.add_option("--iterations", iterations, "Benchmark iterations (default 10)");

    CLI11_PARSE(app, argc, argv);

    if (width <= 0 || height <= 0 || tile_x <= 0 || tile_y <= 0 || iterations <= 0) {
        std::cerr << "[ERROR] width, height, tile-x, tile-y, and iterations must all be > 0\n";
        return 1;
    }
    if (offset_x < 0 || offset_y < 0 ||
        offset_x + tile_x > width || offset_y + tile_y > height) {
        std::cerr << "[ERROR] ROI (offset + tile) extends beyond image boundaries\n";
        return 1;
    }
    if (static_cast<size_t>(width) * static_cast<size_t>(height) >
        static_cast<size_t>(std::numeric_limits<cl_int>::max())) {
        throw std::runtime_error("Image dimensions exceed cl_int range");
    }

    // ── OpenCL context ──────────────────────────────────────────────────────
    // WHY create_context(): honours GPU env var, avoids hard-coded device indices.
    OclContext ocl = create_context();

    // create_context() returns a non-profiling queue; create a profiling queue
    // for benchmark accuracy.  CL_QUEUE_PROFILING_ENABLE is required for
    // cl::Event timestamp queries.
    cl::CommandQueue prof_queue(ocl.context, ocl.device, CL_QUEUE_PROFILING_ENABLE);

    // ── Synthetic input ─────────────────────────────────────────────────────
    // Deterministic ramp: value = (x + y) % 256.  Chosen for visual clarity
    // if the output were ever inspected, not for randomness.
    const size_t pixel_count = static_cast<size_t>(width) * static_cast<size_t>(height);
    std::vector<cl_uchar> host_input(pixel_count);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            host_input[static_cast<size_t>(y) * static_cast<size_t>(width) + x] =
                static_cast<cl_uchar>((x + y) % 256);
        }
    }

    // ── Buffers ─────────────────────────────────────────────────────────────
    // WHY CL_MEM_COPY_HOST_PTR on input: driver uploads host data into a
    // device-local buffer once; both benchmark runs share the same device data.
    cl::Buffer buf_in(ocl.context,
                      CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                      pixel_count * sizeof(cl_uchar),
                      host_input.data());

    // Output buffer: full-frame size so both tile and full-frame runs can write
    // without reallocating.
    cl::Buffer buf_out(ocl.context, CL_MEM_WRITE_ONLY, pixel_count * sizeof(cl_uchar));

    // ── Kernel ──────────────────────────────────────────────────────────────
    // WHY load at runtime: keeps .cl files editable without recompilation.
    const auto bin_dir = get_binary_dir();
    const std::string kernel_path = (bin_dir / "kernels" / "global_work_offset.cl").string();
    cl::Program program = build_program(ocl.context, ocl.device, kernel_path);
    cl::Kernel kernel(program, "roi_invert");

    // ── Benchmark loop A — Tile dispatch ────────────────────────────────────
    std::vector<double> tile_times(static_cast<size_t>(iterations));
    for (int i = 0; i < iterations; ++i) {
        tile_times[static_cast<size_t>(i)] = bench_run(
            prof_queue, kernel, buf_in, buf_out,
            width, height,
            offset_x, offset_y,
            tile_x, tile_y);
    }

    // ── Benchmark loop B — Full-frame dispatch ───────────────────────────────
    // global_work_offset = {0,0}, global_work_size = {width, height}.
    std::vector<double> full_times(static_cast<size_t>(iterations));
    for (int i = 0; i < iterations; ++i) {
        full_times[static_cast<size_t>(i)] = bench_run(
            prof_queue, kernel, buf_in, buf_out,
            width, height,
            0, 0,
            width, height);
    }

    // ── Statistics ───────────────────────────────────────────────────────────
    auto stats = [](const std::vector<double>& v) -> std::tuple<double, double, double> {
        double sum = 0.0, mn = v[0], mx = v[0];
        for (double t : v) {
            sum += t;
            mn = std::min(mn, t);
            mx = std::max(mx, t);
        }
        return {sum / static_cast<double>(v.size()), mn, mx};
    };

    auto [tile_avg, tile_min, tile_max] = stats(tile_times);
    auto [full_avg, full_min, full_max] = stats(full_times);

    const double speedup = (tile_avg > 0.0) ? (full_avg / tile_avg) : 0.0;

    // ── Timing table ─────────────────────────────────────────────────────────
    std::cout << "\n===== global_work_offset Benchmark =====\n";
    std::cout << "Image:        " << width << " x " << height << "\n";
    std::cout << "Tile:         " << tile_x << " x " << tile_y
              << "  @ (" << offset_x << ", " << offset_y << ")\n";
    std::cout << "Iterations:   " << iterations << "\n\n";

    std::cout << std::left  << std::setw(18) << "Mode"
              << std::right << std::setw(12) << "Avg (ms)"
              << std::setw(12) << "Min (ms)"
              << std::setw(12) << "Max (ms)" << "\n";
    std::cout << std::string(54, '-') << "\n";

    auto print_row = [](const std::string& name,
                        double avg, double mn, double mx) {
        std::cout << std::left  << std::setw(18) << name
                  << std::right << std::fixed << std::setprecision(3)
                  << std::setw(12) << avg
                  << std::setw(12) << mn
                  << std::setw(12) << mx << "\n";
    };

    const std::string tile_label = "Tile (" +
        std::to_string(tile_x) + "x" + std::to_string(tile_y) + ")";
    print_row(tile_label,     tile_avg, tile_min, tile_max);
    print_row("Full frame 4K", full_avg, full_min, full_max);

    std::cout << "Speedup:         "
              << std::fixed << std::setprecision(2) << speedup << "x"
              << "  (tile vs full frame)\n";

    return 0;
}
