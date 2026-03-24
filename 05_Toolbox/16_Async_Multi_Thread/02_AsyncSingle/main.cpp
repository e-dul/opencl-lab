// 02_AsyncSingle/main.cpp — dual-queue pipeline for transfer/compute overlap.
//
// Key insight: a single OOO queue routes all commands through one command
// processor on most drivers, so uploads and kernels still serialize internally.
// Two separate in-order queues (dma_queue + compute_queue) let the driver
// schedule DMA transfers and compute dispatches on independent hardware units
// simultaneously — the standard cross-platform approach for pipeline overlap.

#include "opencl_utils.hpp"
#include "ocl_wrapper.hpp"

#include <chrono>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <vector>

double run_async_single(cl::Context& ctx, cl::Device& device, int frames, int size, int iters) {
    cl_int err;

    // WHY two in-order queues instead of one OOO queue:
    // A single OOO queue is still processed by one command processor on NVIDIA
    // and AMD rusticl; uploads and kernels get serialized on the same hardware
    // path. Two independent queues allow the DMA engine and the compute engine
    // to pick work from their respective queues concurrently.
    cl::CommandQueue dma_queue    (ctx, device, CL_QUEUE_PROFILING_ENABLE, &err);
    CL_CHECK(err);
    cl::CommandQueue compute_queue(ctx, device, CL_QUEUE_PROFILING_ENABLE, &err);
    CL_CHECK(err);

    std::cout << "[async_single ] Dual-queue mode: dma_queue + compute_queue.\n";

    std::string src = load_kernel_source("kernels/pipeline_kernel.cl");
    cl::Program program(ctx, src);
    try {
        program.build({device});
    } catch (const cl::Error&) {
        std::string log = program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(device);
        throw std::runtime_error("Kernel build failed:\n" + log);
    }
    cl::Kernel kernel(program, "process");

    const size_t buf_bytes = static_cast<size_t>(size) * sizeof(float);
    std::vector<float> host_in(size, 1.0f);
    std::vector<float> host_out(size, 0.0f);

    // Double-buffering: slot 0 and slot 1 alternate so that while compute_queue
    // processes slot 0's kernel, dma_queue uploads frame N+1 into slot 1.
    const int SLOTS = 2;
    cl::Buffer buf_in[SLOTS], buf_out[SLOTS];
    for (int s = 0; s < SLOTS; ++s) {
        buf_in[s]  = cl::Buffer(ctx, CL_MEM_READ_ONLY,  buf_bytes);
        buf_out[s] = cl::Buffer(ctx, CL_MEM_WRITE_ONLY, buf_bytes);
    }

    cl::NDRange global(static_cast<size_t>(size));

    std::vector<cl::Event> ev_upload(frames), ev_kernel(frames), ev_download(frames);

    auto wall_start = std::chrono::steady_clock::now();

    for (int f = 0; f < frames; ++f) {
        int slot = f % SLOTS;

        // Upload must wait for the kernel that last used this slot to finish
        // before overwriting the input buffer. For frames 0..SLOTS-1 there is
        // no prior user of this slot, so no dependency.
        std::vector<cl::Event> upload_deps;
        if (f >= SLOTS) {
            upload_deps.push_back(ev_kernel[f - SLOTS]);
        }

        CL_CHECK(kernel.setArg(0, buf_in[slot]));
        CL_CHECK(kernel.setArg(1, buf_out[slot]));
        CL_CHECK(kernel.setArg(2, size));
        CL_CHECK(kernel.setArg(3, iters));
        CL_CHECK(kernel.setArg(4, 2.0f));

        // Upload on dma_queue — runs on the DMA engine, concurrent with
        // the compute_queue kernel from the previous slot.
        CL_CHECK(dma_queue.enqueueWriteBuffer(
            buf_in[slot], CL_FALSE, 0, buf_bytes, host_in.data(),
            upload_deps.empty() ? nullptr : &upload_deps,
            &ev_upload[f]));

        // Kernel on compute_queue — waits for this frame's upload via event,
        // then runs on the compute engine while the next upload proceeds.
        std::vector<cl::Event> kernel_deps = {ev_upload[f]};
        CL_CHECK(compute_queue.enqueueNDRangeKernel(
            kernel, cl::NullRange, global, cl::NullRange,
            &kernel_deps, &ev_kernel[f]));

        // Download on dma_queue — waits for this frame's kernel to complete.
        std::vector<cl::Event> dl_deps = {ev_kernel[f]};
        CL_CHECK(dma_queue.enqueueReadBuffer(
            buf_out[slot], CL_FALSE, 0, buf_bytes, host_out.data(),
            &dl_deps, &ev_download[f]));
    }

    // Flush both queues to submit all pending commands to the device,
    // then finish() blocks until all commands on both queues complete.
    CL_CHECK(dma_queue.flush());
    CL_CHECK(compute_queue.flush());
    CL_CHECK(dma_queue.finish());
    CL_CHECK(compute_queue.finish());

    auto wall_end = std::chrono::steady_clock::now();
    double wall_ms = std::chrono::duration<double, std::milli>(wall_end - wall_start).count();

    double total_upload_ms = 0.0, total_kernel_ms = 0.0, total_download_ms = 0.0;
    for (int f = 0; f < frames; ++f) {
        total_upload_ms   += duration_ms(ev_upload[f]);
        total_kernel_ms   += duration_ms(ev_kernel[f]);
        total_download_ms += duration_ms(ev_download[f]);
    }

    double n = static_cast<double>(frames);
    std::cout << std::fixed << std::setprecision(3);
    std::cout << "[async_single ] "
              << "Upload: "   << total_upload_ms   / n << " ms | "
              << "Kernel: "   << total_kernel_ms   / n << " ms | "
              << "Download: " << total_download_ms / n << " ms | "
              << "Total: "    << wall_ms           << " ms\n";

    // Overlap evidence: check whether frame-1 upload started before
    // frame-0 kernel ended (ns-precision from CL profiling timestamps).
    if (frames >= 2) {
        cl_ulong k0_end   = ev_kernel[0].getProfilingInfo<CL_PROFILING_COMMAND_END>();
        cl_ulong u1_start = ev_upload[1].getProfilingInfo<CL_PROFILING_COMMAND_START>();
        if (u1_start < k0_end) {
            std::cout << "[async_single ] Overlap confirmed: frame-1 upload started "
                      << (k0_end - u1_start) / 1000.0
                      << " us before frame-0 kernel ended.\n";
        } else {
            std::cout << "[async_single ] No overlap detected (device may serialize internally).\n";
        }
    }

    return wall_ms;
}
