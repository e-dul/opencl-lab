// 01_BasicSync/main.cpp — blocking pipeline baseline.
//
// Each frame is processed sequentially: upload blocks, then kernel runs,
// then download blocks. This is the simplest correct implementation but
// leaves GPU idle during every host<->device transfer.

#include "opencl_utils.hpp"
#include "ocl_wrapper.hpp"

#include <chrono>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <stdexcept>
#include <vector>

double run_basic_sync(cl::Context& ctx, cl::Device& device, int frames, int size, int iters) {
    // WHY CL_QUEUE_PROFILING_ENABLE: required for cl::Event timestamp queries.
    // Without this flag getProfilingInfo() returns CL_PROFILING_INFO_NOT_AVAILABLE.
    cl_int err;
    cl::CommandQueue queue(ctx, device, CL_QUEUE_PROFILING_ENABLE, &err);
    CL_CHECK(err);

    // Load and build kernel from the binary-adjacent kernels/ directory.
    std::string src = load_kernel_source("kernels/pipeline_kernel.cl");
    cl::Program program(ctx, src);
    try {
        program.build({device});
    } catch (const cl::Error&) {
        std::string log = program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(device);
        throw std::runtime_error("Kernel build failed:\n" + log);
    }
    cl::Kernel kernel(program, "process");

    // WHY static_cast<size_t>: size is int; multiplying two ints before casting
    // would overflow for large buffers (>536M elements). Promote first.
    const size_t buf_bytes = static_cast<size_t>(size) * sizeof(float);
    std::vector<float> host_in(size, 1.0f);
    std::vector<float> host_out(size, 0.0f);

    cl::Buffer buf_in(ctx, CL_MEM_READ_ONLY,  buf_bytes);
    cl::Buffer buf_out(ctx, CL_MEM_WRITE_ONLY, buf_bytes);

    CL_CHECK(kernel.setArg(0, buf_in));
    CL_CHECK(kernel.setArg(1, buf_out));
    CL_CHECK(kernel.setArg(2, size));
    CL_CHECK(kernel.setArg(3, iters));
    CL_CHECK(kernel.setArg(4, 2.0f));

    cl::NDRange global(static_cast<size_t>(size));

    double total_upload_ms   = 0.0;
    double total_kernel_ms   = 0.0;
    double total_download_ms = 0.0;

    auto wall_start = std::chrono::steady_clock::now();

    for (int f = 0; f < frames; ++f) {
        cl::Event ev_upload, ev_kernel, ev_download;

        // CL_TRUE = blocking: host stalls until transfer completes.
        // This ensures strict sequencing and is the simplest correct approach,
        // but prevents any overlap with compute.
        // WHY blocking writes here: used intentionally for simplicity; fully
        // non-blocking dispatches would require intra-thread event chains
        // (see 02_AsyncSingle for that pattern).
        CL_CHECK(queue.enqueueWriteBuffer(buf_in, CL_TRUE, 0, buf_bytes,
                                          host_in.data(), nullptr, &ev_upload));

        CL_CHECK(queue.enqueueNDRangeKernel(kernel, cl::NullRange, global,
                                             cl::NullRange, nullptr, &ev_kernel));
        CL_CHECK(queue.finish());

        CL_CHECK(queue.enqueueReadBuffer(buf_out, CL_TRUE, 0, buf_bytes,
                                          host_out.data(), nullptr, &ev_download));

        total_upload_ms   += duration_ms(ev_upload);
        total_kernel_ms   += duration_ms(ev_kernel);
        total_download_ms += duration_ms(ev_download);
    }

    auto wall_end = std::chrono::steady_clock::now();
    double wall_ms = std::chrono::duration<double, std::milli>(wall_end - wall_start).count();

    double n = static_cast<double>(frames);
    std::cout << std::fixed << std::setprecision(3);
    std::cout << "[basic_sync   ] "
              << "Upload: "   << total_upload_ms   / n << " ms | "
              << "Kernel: "   << total_kernel_ms   / n << " ms | "
              << "Download: " << total_download_ms / n << " ms | "
              << "Total: "    << wall_ms           << " ms\n";
    return wall_ms;
}
