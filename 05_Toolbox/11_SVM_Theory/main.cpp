// svm_theory — benchmarks five OpenCL memory-transfer paths over a 1920×1080
// RGBA image round-trip, each reporting split Transfer + Kernel timing:
//
//   A. buffer_map     — CL_MEM_COPY_HOST_PTR + enqueueMapBuffer/Unmap (OpenCL 1.x baseline)
//   B. copy_host_ptr  — CL_MEM_COPY_HOST_PTR: driver copies host→device at creation
//   C. use_host_ptr   — CL_MEM_USE_HOST_PTR: driver may zero-copy on UMA hardware
//   D. coarse_svm     — SVM coarse-grained: shared pointer with explicit map/unmap (OpenCL 2.0+)
//   E. fine_svm       — SVM fine-grained: coherent shared pointer, no map needed (OpenCL 2.0+)
//
// WHY split timing (Transfer + Kernel separately):
//   Profiling the full round-trip as one number hides the bottleneck. On a
//   discrete GPU, Transfer dominates; on a UMA iGPU, Transfer collapses to ~0.
//   Splitting reveals which phase to optimise.
//
// WHY CL_HPP_TARGET_OPENCL_VERSION 200 before opencl_utils.hpp:
//   opencl_utils.hpp defaults to 120 via #ifndef guard. Defining 200 first
//   unlocks SVM API symbols without touching any other module.
#define CL_HPP_TARGET_OPENCL_VERSION  200
#define CL_HPP_MINIMUM_OPENCL_VERSION 120

// STB must be implemented exactly once per binary, before image_utils.hpp.
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image.h>
#include <stb_image_write.h>

#include "../../common/opencl_utils.hpp"
#include "../../common/image_utils.hpp"
#include <CLI/CLI.hpp>

#include <algorithm>
#include <chrono>
#include <climits>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// OclSetup — bundles all OpenCL context state.
// WHY inline struct: SVM raw C API calls need cl_context and cl_command_queue
// handles directly (via operator()); bundling them avoids passing four args
// through every benchmark helper.
// ---------------------------------------------------------------------------
struct OclSetup {
    cl::Platform     platform;
    cl::Device       device;
    cl::Context      context;
    cl::CommandQueue queue;
};

// ---------------------------------------------------------------------------
// select_device — honours GPU env-var (vendor substring, case-insensitive).
// Matches CL_PLATFORM_VENDOR and CL_DEVICE_VENDOR; falls back to first GPU,
// then first CPU.  Inlined here (not using ocl_wrapper.hpp) because we need
// CL_VERSION_2_0 symbols in scope for the context creation path.
// ---------------------------------------------------------------------------
static OclSetup select_device() {
    const char* gpu_env = std::getenv("GPU");
    std::string vendor_filter = gpu_env ? gpu_env : "";
    std::transform(vendor_filter.begin(), vendor_filter.end(),
                   vendor_filter.begin(), ::tolower);

    std::vector<cl::Platform> platforms;
    cl::Platform::get(&platforms);
    if (platforms.empty()) throw std::runtime_error("No OpenCL platforms found");

    for (auto& p : platforms) {
        std::vector<cl::Device> devices;
        try { p.getDevices(CL_DEVICE_TYPE_ALL, &devices); }
        catch (...) { continue; }
        for (auto& d : devices) {
            std::string pv = p.getInfo<CL_PLATFORM_VENDOR>();
            std::string dv = d.getInfo<CL_DEVICE_VENDOR>();
            std::transform(pv.begin(), pv.end(), pv.begin(), ::tolower);
            std::transform(dv.begin(), dv.end(), dv.begin(), ::tolower);
            if (vendor_filter.empty() ||
                pv.find(vendor_filter) != std::string::npos ||
                dv.find(vendor_filter) != std::string::npos) {
                cl_context_properties props[] = {
                    CL_CONTEXT_PLATFORM,
                    reinterpret_cast<cl_context_properties>(p()),
                    0
                };
                cl::Context ctx({d}, props);
                // WHY CL_QUEUE_PROFILING_ENABLE: cl::Event timestamps require
                // the queue to have profiling enabled at creation time.
                cl::CommandQueue q(ctx, d, CL_QUEUE_PROFILING_ENABLE);
                return {p, d, ctx, q};
            }
        }
    }
    throw std::runtime_error(
        std::string("No matching device for GPU=") + (gpu_env ? gpu_env : "(unset)"));
}

// ---------------------------------------------------------------------------
// TimingResult — split timing per mode: Transfer time + Kernel time.
// ---------------------------------------------------------------------------
struct TimingResult {
    std::string mode_name;
    double transfer_ms{0.0};   // host→device path (chrono or cl::Event)
    double kernel_ms{0.0};     // kernel execution (always cl::Event)
    bool   skipped{false};
    std::string skip_reason;
};

static void print_result(const TimingResult& r) {
    if (r.skipped) {
        std::cout << "[" << std::left << std::setw(14) << r.mode_name << "]  "
                  << "SKIPPED — " << r.skip_reason << "\n";
        return;
    }
    std::cout << "[" << std::left << std::setw(14) << r.mode_name << "]  "
              << "Transfer: " << std::fixed << std::setprecision(3)
              << r.transfer_ms << " ms  "
              << "Kernel: "   << std::fixed << std::setprecision(3)
              << r.kernel_ms << " ms\n";
}

// ===========================================================================
// Mode A: buffer_map
// Allocate with CL_MEM_COPY_HOST_PTR (driver copies at creation), then
// exercise map/unmap explicitly to demonstrate the round-trip fence cost.
// WHY CL_MEM_COPY_HOST_PTR + map/unmap: this is the baseline for all OpenCL
// 1.x hardware — portable but carries explicit synchronisation overhead.
// Transfer time = enqueueMapBuffer + enqueueUnmapMemObject latency (chrono).
// Kernel time   = passthrough kernel dispatch (cl::Event).
// ===========================================================================
static TimingResult run_buffer_map(OclSetup& ocl, cl::Program& program,
                                   const std::vector<uint8_t>& src,
                                   size_t byte_size, cl_int pixel_count_int,
                                   int width, int height) {
    TimingResult r;
    r.mode_name = "buffer_map";

    // CL_MEM_COPY_HOST_PTR: driver allocates device memory and pre-populates
    // from src atomically. src is safe to modify on the host after this returns.
    cl::Buffer buf(ocl.context,
                   CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
                   byte_size,
                   const_cast<uint8_t*>(src.data()));
    cl::Buffer out_buf(ocl.context, CL_MEM_WRITE_ONLY, byte_size);

    // Time the map/unmap fence round-trip (CPU-side chrono — map/unmap are
    // host-driver synchronisation points, not GPU work items).
    auto t0 = std::chrono::steady_clock::now();
    void* mapped_ptr = ocl.queue.enqueueMapBuffer(
        buf, CL_TRUE, CL_MAP_READ | CL_MAP_WRITE, 0, byte_size);
    // WHY enqueueUnmapMemObject in CL_CHECK: it returns cl_int and is NOT
    // covered by CL_HPP_ENABLE_EXCEPTIONS.
    CL_CHECK(ocl.queue.enqueueUnmapMemObject(buf, mapped_ptr));
    // WHY finish() before t1: ensures the unmap has fully committed before
    // the chrono timestamp is captured (avoids underestimating map cost).
    CL_CHECK(ocl.queue.finish());
    auto t1 = std::chrono::steady_clock::now();
    r.transfer_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

    // Run passthrough kernel — identity copy from buf → out_buf.
    cl::Kernel k(program, "passthrough");
    CL_CHECK(k.setArg(0, buf));
    CL_CHECK(k.setArg(1, out_buf));
    CL_CHECK(k.setArg(2, pixel_count_int));

    cl::Event ev;
    CL_CHECK(ocl.queue.enqueueNDRangeKernel(
        k, cl::NullRange, cl::NDRange(static_cast<size_t>(pixel_count_int)),
        cl::NullRange, nullptr, &ev));
    CL_CHECK(ocl.queue.finish());
    r.kernel_ms = duration_ms(ev);

    // Read back for correctness check and BMP output.
    std::vector<uint8_t> readback(byte_size);
    CL_CHECK(ocl.queue.enqueueReadBuffer(out_buf, CL_TRUE, 0, byte_size, readback.data()));
    if (std::memcmp(src.data(), readback.data(), byte_size) != 0)
        throw std::runtime_error("buffer_map: correctness check failed");
    save_bmp("output.bmp", readback, width, height, 4);

    return r;
}

// ===========================================================================
// Mode B: copy_host_ptr
// Driver allocates device memory and copies from src at buffer creation.
// Most portable path — works on all OpenCL 1.x hardware.
// Transfer time = enqueueWriteBuffer (explicit re-upload, separate event).
// Kernel time   = passthrough kernel dispatch (cl::Event).
// ===========================================================================
static TimingResult run_copy_host_ptr(OclSetup& ocl, cl::Program& program,
                                      const std::vector<uint8_t>& src,
                                      size_t byte_size, cl_int pixel_count_int,
                                      int width, int height) {
    TimingResult r;
    r.mode_name = "copy_host_ptr";

    // WHY CL_MEM_COPY_HOST_PTR: transfers data to device on construction;
    // src is safe to modify on the host after this call returns.
    cl::Buffer in_buf(ocl.context,
        CL_MEM_COPY_HOST_PTR | CL_MEM_READ_ONLY, byte_size,
        const_cast<uint8_t*>(src.data()));
    cl::Buffer out_buf(ocl.context, CL_MEM_WRITE_ONLY, byte_size);

    // Time a re-upload via enqueueWriteBuffer to measure the transfer cost
    // in isolation (the construction-time copy is not separately timeable).
    cl::Event write_ev;
    CL_CHECK(ocl.queue.enqueueWriteBuffer(
        in_buf, CL_FALSE, 0, byte_size,
        const_cast<uint8_t*>(src.data()), nullptr, &write_ev));
    CL_CHECK(ocl.queue.finish());
    r.transfer_ms = duration_ms(write_ev);

    cl::Kernel k(program, "passthrough");
    CL_CHECK(k.setArg(0, in_buf));
    CL_CHECK(k.setArg(1, out_buf));
    CL_CHECK(k.setArg(2, pixel_count_int));

    cl::Event ev;
    CL_CHECK(ocl.queue.enqueueNDRangeKernel(
        k, cl::NullRange, cl::NDRange(static_cast<size_t>(pixel_count_int)),
        cl::NullRange, nullptr, &ev));
    CL_CHECK(ocl.queue.finish());
    r.kernel_ms = duration_ms(ev);

    std::vector<uint8_t> readback(byte_size);
    CL_CHECK(ocl.queue.enqueueReadBuffer(out_buf, CL_TRUE, 0, byte_size, readback.data()));
    if (std::memcmp(src.data(), readback.data(), byte_size) != 0)
        throw std::runtime_error("copy_host_ptr: correctness check failed");
    save_bmp("output.bmp", readback, width, height, 4);

    return r;
}

// ===========================================================================
// Mode C: use_host_ptr
// Driver may use the host pointer directly (zero-copy / pinned) on UMA
// hardware; on discrete GPUs it typically still performs a copy via PCIe.
// Transfer time = enqueueWriteBuffer (measures what the driver actually pays).
// Kernel time   = passthrough kernel dispatch (cl::Event).
// ===========================================================================
static TimingResult run_use_host_ptr(OclSetup& ocl, cl::Program& program,
                                     const std::vector<uint8_t>& src,
                                     size_t byte_size, cl_int pixel_count_int,
                                     int width, int height) {
    TimingResult r;
    r.mode_name = "use_host_ptr";

    // WHY CL_MEM_USE_HOST_PTR: on iGPU/APU hardware where CPU and GPU
    // share physical memory, the driver can map the host buffer directly
    // rather than copying — eliminating PCIe transfer latency entirely.
    cl::Buffer in_buf(ocl.context,
        CL_MEM_USE_HOST_PTR | CL_MEM_READ_ONLY, byte_size,
        const_cast<uint8_t*>(src.data()));
    cl::Buffer out_buf(ocl.context, CL_MEM_WRITE_ONLY, byte_size);

    // Time the first write to reveal whether the driver actually zero-copies.
    cl::Event write_ev;
    CL_CHECK(ocl.queue.enqueueWriteBuffer(
        in_buf, CL_FALSE, 0, byte_size,
        const_cast<uint8_t*>(src.data()), nullptr, &write_ev));
    CL_CHECK(ocl.queue.finish());
    r.transfer_ms = duration_ms(write_ev);

    cl::Kernel k(program, "passthrough");
    CL_CHECK(k.setArg(0, in_buf));
    CL_CHECK(k.setArg(1, out_buf));
    CL_CHECK(k.setArg(2, pixel_count_int));

    cl::Event ev;
    CL_CHECK(ocl.queue.enqueueNDRangeKernel(
        k, cl::NullRange, cl::NDRange(static_cast<size_t>(pixel_count_int)),
        cl::NullRange, nullptr, &ev));
    CL_CHECK(ocl.queue.finish());
    r.kernel_ms = duration_ms(ev);

    std::vector<uint8_t> readback(byte_size);
    CL_CHECK(ocl.queue.enqueueReadBuffer(out_buf, CL_TRUE, 0, byte_size, readback.data()));
    if (std::memcmp(src.data(), readback.data(), byte_size) != 0)
        throw std::runtime_error("use_host_ptr: correctness check failed");
    save_bmp("output.bmp", readback, width, height, 4);

    return r;
}

#ifdef CL_VERSION_2_0
// ===========================================================================
// Mode D: coarse_svm
// SVM coarse-grained: shared pointer with explicit map/unmap fences.
// Transfer time = clEnqueueSVMMap + clEnqueueSVMUnmap round-trip (chrono).
// Kernel time   = passthrough kernel dispatch (cl::Event).
// ===========================================================================
static TimingResult run_coarse_svm(OclSetup& ocl, cl::Program& program,
                                   const std::vector<uint8_t>& src,
                                   size_t byte_size, cl_int pixel_count_int,
                                   int width, int height,
                                   bool has_coarse) {
    TimingResult r;
    r.mode_name = "coarse_svm";

    if (!has_coarse) {
        r.skipped    = true;
        r.skip_reason = "CL_DEVICE_SVM_COARSE_GRAIN_BUFFER not supported";
        return r;
    }

    void* svm_in  = clSVMAlloc(ocl.context(), CL_MEM_READ_ONLY,  byte_size, 0);
    void* svm_out = clSVMAlloc(ocl.context(), CL_MEM_WRITE_ONLY, byte_size, 0);
    if (!svm_in || !svm_out) {
        if (svm_in)  clSVMFree(ocl.context(), svm_in);
        if (svm_out) clSVMFree(ocl.context(), svm_out);
        throw std::runtime_error("clSVMAlloc (coarse) failed");
    }

    // Data init: coarse-grained SVM requires an open map window before
    // the host may legally write the SVM region (§7.8 — SVM spec).
    CL_CHECK(clEnqueueSVMMap(ocl.queue(), CL_TRUE, CL_MAP_WRITE,
                             svm_in, byte_size, 0, nullptr, nullptr));
    std::memcpy(svm_in, src.data(), byte_size);
    CL_CHECK(clEnqueueSVMUnmap(ocl.queue(), svm_in, 0, nullptr, nullptr));
    CL_CHECK(clFinish(ocl.queue()));

    // Time the map/unmap synchronisation fence — this is the transfer cost.
    auto t0 = std::chrono::steady_clock::now();
    CL_CHECK(clEnqueueSVMMap(ocl.queue(), CL_TRUE, CL_MAP_READ | CL_MAP_WRITE,
                             svm_in, byte_size, 0, nullptr, nullptr));
    CL_CHECK(clEnqueueSVMUnmap(ocl.queue(), svm_in, 0, nullptr, nullptr));
    // WHY finish() before t1: both map and unmap are asynchronous commands;
    // finish() guarantees they have completed before the timestamp.
    CL_CHECK(clFinish(ocl.queue()));
    auto t1 = std::chrono::steady_clock::now();
    r.transfer_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

    // WHY clSetKernelArgSVMPointer (not cl::Kernel::setArg):
    // the C++ wrapper has no overload for raw SVM pointers; the raw C API
    // is the only supported path for SVM pointer arguments.
    cl::Kernel k(program, "passthrough");
    CL_CHECK(clSetKernelArgSVMPointer(k(), 0, svm_in));
    CL_CHECK(clSetKernelArgSVMPointer(k(), 1, svm_out));
    CL_CHECK(k.setArg(2, pixel_count_int));

    cl::Event ev;
    CL_CHECK(ocl.queue.enqueueNDRangeKernel(
        k, cl::NullRange, cl::NDRange(static_cast<size_t>(pixel_count_int)),
        cl::NullRange, nullptr, &ev));
    CL_CHECK(clFinish(ocl.queue()));
    r.kernel_ms = duration_ms(ev);

    // Map for read-back — must open a read window before memcpy.
    CL_CHECK(clEnqueueSVMMap(ocl.queue(), CL_TRUE, CL_MAP_READ,
                             svm_out, byte_size, 0, nullptr, nullptr));
    std::vector<uint8_t> result(byte_size);
    std::memcpy(result.data(), svm_out, byte_size);
    CL_CHECK(clEnqueueSVMUnmap(ocl.queue(), svm_out, 0, nullptr, nullptr));
    CL_CHECK(clFinish(ocl.queue()));

    clSVMFree(ocl.context(), svm_in);
    clSVMFree(ocl.context(), svm_out);

    if (std::memcmp(src.data(), result.data(), byte_size) != 0)
        throw std::runtime_error("coarse_svm: correctness check failed");
    save_bmp("output.bmp", result, width, height, 4);

    return r;
}

// ===========================================================================
// Mode E: fine_svm
// SVM fine-grained system: coherent shared memory — no map/unmap required.
// Transfer time = 0 ms (direct host write, no fence needed).
// Kernel time   = passthrough kernel dispatch (cl::Event).
// ===========================================================================
static TimingResult run_fine_svm(OclSetup& ocl, cl::Program& program,
                                 const std::vector<uint8_t>& src,
                                 size_t byte_size, cl_int pixel_count_int,
                                 int width, int height,
                                 bool has_fine_sys) {
    TimingResult r;
    r.mode_name = "fine_svm";

    if (!has_fine_sys) {
        r.skipped    = true;
        r.skip_reason = "CL_DEVICE_SVM_FINE_GRAIN_SYSTEM not supported";
        return r;
    }

    void* svm_in  = clSVMAlloc(ocl.context(), CL_MEM_READ_ONLY,  byte_size, 0);
    void* svm_out = clSVMAlloc(ocl.context(), CL_MEM_WRITE_ONLY, byte_size, 0);
    if (!svm_in || !svm_out) {
        if (svm_in)  clSVMFree(ocl.context(), svm_in);
        if (svm_out) clSVMFree(ocl.context(), svm_out);
        throw std::runtime_error("clSVMAlloc (fine) failed");
    }

    // Direct host write — no map needed for fine-grained coherent memory.
    // Transfer time is effectively 0: CPU writes are immediately visible to GPU.
    std::memcpy(svm_in, src.data(), byte_size);
    r.transfer_ms = 0.0;

    cl::Kernel k(program, "passthrough");
    CL_CHECK(clSetKernelArgSVMPointer(k(), 0, svm_in));
    CL_CHECK(clSetKernelArgSVMPointer(k(), 1, svm_out));
    CL_CHECK(k.setArg(2, pixel_count_int));

    cl::Event ev;
    CL_CHECK(ocl.queue.enqueueNDRangeKernel(
        k, cl::NullRange, cl::NDRange(static_cast<size_t>(pixel_count_int)),
        cl::NullRange, nullptr, &ev));
    CL_CHECK(clFinish(ocl.queue()));
    r.kernel_ms = duration_ms(ev);

    // Direct host read — coherent, no map needed.
    std::vector<uint8_t> result(byte_size);
    std::memcpy(result.data(), svm_out, byte_size);

    clSVMFree(ocl.context(), svm_in);
    clSVMFree(ocl.context(), svm_out);

    if (std::memcmp(src.data(), result.data(), byte_size) != 0)
        throw std::runtime_error("fine_svm: correctness check failed");
    save_bmp("output.bmp", result, width, height, 4);

    return r;
}
#endif // CL_VERSION_2_0


int main(int argc, char* argv[]) {
    CLI::App app{"SVM Theory — OpenCL memory-transfer path benchmark (5 modes)"};
    int width  = 1920;
    int height = 1080;
    std::string mode = "all";

    app.add_option("--width",  width,  "Image width in pixels")->default_val(1920);
    app.add_option("--height", height, "Image height in pixels")->default_val(1080);
    app.add_option("--mode",   mode,   "Transfer mode to benchmark")
       ->default_val("all")
       ->check(CLI::IsMember({"buffer_map", "use_host_ptr", "copy_host_ptr",
                              "coarse_svm", "fine_svm", "all"}));

    CLI11_PARSE(app, argc, argv);

    auto ocl = select_device();
    std::cout << "Device: " << ocl.device.getInfo<CL_DEVICE_NAME>() << "\n";

    // §7.1: promote to size_t before multiplying to avoid 32-bit overflow.
    if (static_cast<size_t>(width) * static_cast<size_t>(height) >
        static_cast<size_t>(INT_MAX))
        throw std::runtime_error("Image too large: pixel count exceeds INT_MAX");

    const size_t pixel_count    = static_cast<size_t>(width) * height;
    const size_t byte_size      = pixel_count * 4;
    const cl_int pixel_count_int = static_cast<cl_int>(pixel_count);

    // Build kernel — passthrough.cl is the single kernel used by all modes.
    const std::string kernel_path =
        (get_binary_dir() / "kernels" / "passthrough.cl").string();
    cl::Program program = build_program(ocl.context, ocl.device, kernel_path);

    // Synthetic checkerboard RGBA — 8×8 pixel tiles, alpha always 255.
    std::vector<uint8_t> src(byte_size);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const size_t idx = (static_cast<size_t>(y) * width + x) * 4;
            const uint8_t val = ((x / 8 + y / 8) % 2) ? 255u : 0u;
            src[idx+0] = val;
            src[idx+1] = val;
            src[idx+2] = val;
            src[idx+3] = 255u;
        }
    }

    // ── OpenCL 2.0 graceful fallback for SVM-only modes ─────────────────────
    // WHY check here: if a SVM mode is explicitly requested on a 1.2 device,
    // we print a descriptive message and exit cleanly instead of crashing when
    // SVM symbols are not available at runtime.
#ifdef CL_VERSION_2_0
    const std::string ocl_ver = ocl.device.getInfo<CL_DEVICE_VERSION>();
    cl_device_svm_capabilities svm_caps = 0;
    CL_CHECK(clGetDeviceInfo(ocl.device(), CL_DEVICE_SVM_CAPABILITIES,
                             sizeof(svm_caps), &svm_caps, nullptr));
    const bool has_coarse   = (svm_caps & CL_DEVICE_SVM_COARSE_GRAIN_BUFFER) != 0;
    const bool has_fine_sys = (svm_caps & CL_DEVICE_SVM_FINE_GRAIN_SYSTEM)   != 0;

    if ((mode == "coarse_svm" || mode == "fine_svm") && svm_caps == 0) {
        std::cout << "[INFO] Device (" << ocl_ver << ") reports no SVM capability.\n"
                  << "[INFO] Use --mode buffer_map, copy_host_ptr, or use_host_ptr"
                     " for OpenCL 1.x-compatible paths.\n";
        return 0;
    }
#else
    const bool has_coarse   = false;
    const bool has_fine_sys = false;
    if (mode == "coarse_svm" || mode == "fine_svm") {
        std::cout << "[INFO] Compiled without CL_VERSION_2_0. SVM modes unavailable.\n"
                  << "[INFO] Use --mode buffer_map, copy_host_ptr, or use_host_ptr.\n";
        return 0;
    }
#endif

    // ── Mode dispatch ────────────────────────────────────────────────────────
    std::vector<TimingResult> results;

    auto run_if = [&](const std::string& name, auto fn) {
        if (mode == "all" || mode == name)
            results.push_back(fn());
    };

    run_if("buffer_map", [&]() {
        return run_buffer_map(ocl, program, src, byte_size,
                              pixel_count_int, width, height);
    });
    run_if("copy_host_ptr", [&]() {
        return run_copy_host_ptr(ocl, program, src, byte_size,
                                 pixel_count_int, width, height);
    });
    run_if("use_host_ptr", [&]() {
        return run_use_host_ptr(ocl, program, src, byte_size,
                                pixel_count_int, width, height);
    });

#ifdef CL_VERSION_2_0
    run_if("coarse_svm", [&]() {
        return run_coarse_svm(ocl, program, src, byte_size,
                              pixel_count_int, width, height, has_coarse);
    });
    run_if("fine_svm", [&]() {
        return run_fine_svm(ocl, program, src, byte_size,
                            pixel_count_int, width, height, has_fine_sys);
    });
#endif

    // ── Print timing summary ─────────────────────────────────────────────────
    std::cout << "\n";
    for (const auto& r : results)
        print_result(r);
    std::cout << "\n";

    return 0;
}
