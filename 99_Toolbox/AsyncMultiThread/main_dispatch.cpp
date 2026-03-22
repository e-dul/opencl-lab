// main_dispatch.cpp — top-level entry point; dispatches to the selected mode.
//
// WHY a single binary with --mode: allows apples-to-apples comparison on the
// same device without re-running cmake or rebuilding. Each mode is isolated in
// its own translation unit to keep the code readable for mid-level engineers.

#include "opencl_utils.hpp"
#include "ocl_wrapper.hpp"

#include <CLI/CLI.hpp>

#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>

// Forward declarations — implemented in the sub-directory translation units.
double run_basic_sync  (cl::Context& ctx, cl::Device& device, int frames, int size, int iters);
double run_async_single(cl::Context& ctx, cl::Device& device, int frames, int size, int iters);
double run_multi_thread(cl::Context& ctx, cl::Device& device, int frames, int size, int iters);

int main(int argc, char** argv) {
    CLI::App app{"AsyncMultiThread — pipeline overlap demonstration (OpenCL 1.2)"};

    std::string mode;
    int frames = 10;
    int size   = 1 << 20; // 1 048 576 float elements ≈ 4 MB
    int iters  = 8192;

    app.add_option("--mode", mode,
        "Pipeline variant: basic_sync | async_single | multi_thread | all")
        ->required();
    app.add_option("--frames", frames,
        "Number of frames to process (default: 10)")
        ->default_val(10);
    app.add_option("--size", size,
        "Buffer size in float elements (default: 1048576 = 4 MB)")
        ->default_val(1 << 20);
    app.add_option("--iters", iters,
        "FMA iterations per work item — controls compute load (default: 8192)")
        ->default_val(8192);

    CLI11_PARSE(app, argc, argv);

    if (size <= 0) {
        throw std::runtime_error("--size must be positive");
    }
    if (frames <= 0) {
        throw std::runtime_error("--frames must be positive");
    }

    // create_context() respects the GPU env var; do not hard-code device index.
    auto ocl = create_context();
    std::cout << "\n";

    if (mode == "basic_sync") {
        run_basic_sync(ocl.context, ocl.device, frames, size, iters);
    } else if (mode == "async_single") {
        run_async_single(ocl.context, ocl.device, frames, size, iters);
    } else if (mode == "multi_thread") {
        run_multi_thread(ocl.context, ocl.device, frames, size, iters);
    } else if (mode == "all") {
        // Run all three modes sequentially so results are directly comparable.
        double t_sync   = run_basic_sync  (ocl.context, ocl.device, frames, size, iters);
        double t_async  = run_async_single(ocl.context, ocl.device, frames, size, iters);
        double t_multi  = run_multi_thread(ocl.context, ocl.device, frames, size, iters);

        // WHY guard against division by zero: on pathological hardware either
        // async mode could theoretically report 0 ms (e.g. very small --size).
        std::cout << std::fixed << std::setprecision(3);
        if (t_async > 0.0) {
            std::cout << "Speedup async_single vs basic_sync: "
                      << t_sync / t_async << "x\n";
        }
        if (t_multi > 0.0) {
            std::cout << "Speedup multi_thread vs basic_sync:  "
                      << t_sync / t_multi << "x\n";
        }
    } else {
        throw std::runtime_error(
            "Unknown mode '" + mode + "'. Use: basic_sync | async_single | multi_thread | all");
    }

    return 0;
}
