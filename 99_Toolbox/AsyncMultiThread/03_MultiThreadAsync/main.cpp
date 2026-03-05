// 03_MultiThreadAsync/main.cpp — per-thread cl::CommandQueue from shared cl::Context.
//
// cl::CommandQueue is NOT thread-safe (OpenCL spec §4.3). The correct approach
// is to give each thread its own queue constructed from a shared cl::Context.
// The context itself IS thread-safe for object creation and reference counting.

#include "opencl_utils.hpp"
#include "ocl_wrapper.hpp"

#include <chrono>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <vector>

struct FrameResult {
    double upload_ms   = 0.0;
    double kernel_ms   = 0.0;
    double download_ms = 0.0;
};

// WHY shared program, per-thread queue: cl::Program and cl::Kernel are
// reference-counted objects. Building once and sharing the program is safe;
// each thread creates its own cl::Kernel from the shared program to avoid
// data races on kernel argument state.
double run_multi_thread(cl::Context& ctx, cl::Device& device, int frames, int size, int iters) {
    std::string src = load_kernel_source("kernels/pipeline_kernel.cl");
    cl::Program program(ctx, src);
    try {
        program.build({device});
    } catch (const cl::Error&) {
        std::string log = program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(device);
        throw std::runtime_error("Kernel build failed:\n" + log);
    }

    const size_t buf_bytes = static_cast<size_t>(size) * sizeof(float);

    // Clamp thread count to hardware parallelism to avoid over-subscription.
    // Each thread processes one frame; more threads than frames is wasteful.
    int num_threads = std::min(
        static_cast<int>(std::thread::hardware_concurrency()),
        frames
    );
    if (num_threads < 1) num_threads = 1;

    std::vector<FrameResult> results(frames);

    // Thread function: owns its queue + kernel + buffers for the assigned frame.
    // iters is captured by [&] — safe because run_multi_thread outlives all threads.
    auto thread_fn = [&](int frame_idx) {
        // Each thread constructs its own queue — no shared queue, no mutex needed.
        cl_int err;
        cl::CommandQueue queue(ctx, device, CL_QUEUE_PROFILING_ENABLE, &err);
        CL_CHECK(err);

        // Per-thread kernel so setArg() calls don't race across threads.
        cl::Kernel kernel(program, "process");

        std::vector<float> host_in(size, 1.0f);
        std::vector<float> host_out(size, 0.0f);

        cl::Buffer buf_in(ctx,  CL_MEM_READ_ONLY,  buf_bytes);
        cl::Buffer buf_out(ctx, CL_MEM_WRITE_ONLY, buf_bytes);

        CL_CHECK(kernel.setArg(0, buf_in));
        CL_CHECK(kernel.setArg(1, buf_out));
        CL_CHECK(kernel.setArg(2, size));
        CL_CHECK(kernel.setArg(3, iters));
        CL_CHECK(kernel.setArg(4, 2.0f));

        cl::NDRange global(static_cast<size_t>(size));

        cl::Event ev_upload, ev_kernel, ev_download;

        CL_CHECK(queue.enqueueWriteBuffer(buf_in, CL_TRUE, 0, buf_bytes,
                                           host_in.data(), nullptr, &ev_upload));

        CL_CHECK(queue.enqueueNDRangeKernel(kernel, cl::NullRange, global,
                                             cl::NullRange, nullptr, &ev_kernel));
        CL_CHECK(queue.finish());

        CL_CHECK(queue.enqueueReadBuffer(buf_out, CL_TRUE, 0, buf_bytes,
                                          host_out.data(), nullptr, &ev_download));

        results[frame_idx].upload_ms   = duration_ms(ev_upload);
        results[frame_idx].kernel_ms   = duration_ms(ev_kernel);
        results[frame_idx].download_ms = duration_ms(ev_download);
    };

    auto wall_start = std::chrono::steady_clock::now();

    // Dispatch frames across threads in round-robin batches.
    // We process all frames, launching at most num_threads concurrently.
    int f = 0;
    while (f < frames) {
        int batch_end = std::min(f + num_threads, frames);
        int batch_size = batch_end - f;

        std::vector<std::thread> workers;
        workers.reserve(static_cast<size_t>(batch_size));
        for (int i = f; i < batch_end; ++i) {
            workers.emplace_back(thread_fn, i);
        }
        // WHY join before launching next batch: ensures device load stays
        // bounded and simplifies result aggregation (no atomic needed).
        for (auto& t : workers) t.join();
        f = batch_end;
    }

    auto wall_end = std::chrono::steady_clock::now();
    double wall_ms = std::chrono::duration<double, std::milli>(wall_end - wall_start).count();

    double total_upload_ms = 0.0, total_kernel_ms = 0.0, total_download_ms = 0.0;
    for (const auto& r : results) {
        total_upload_ms   += r.upload_ms;
        total_kernel_ms   += r.kernel_ms;
        total_download_ms += r.download_ms;
    }

    double n = static_cast<double>(frames);
    std::cout << std::fixed << std::setprecision(3);
    std::cout << "[multi_thread ] "
              << "Upload: "   << total_upload_ms   / n << " ms | "
              << "Kernel: "   << total_kernel_ms   / n << " ms | "
              << "Download: " << total_download_ms / n << " ms | "
              << "Total: "    << wall_ms           << " ms\n";
    return wall_ms;
}
