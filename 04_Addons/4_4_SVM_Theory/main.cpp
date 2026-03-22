// svm_deep_dive — benchmarks four OpenCL memory-transfer paths over a
// 1920×1080 RGBA image round-trip:
//   A. CL_MEM_COPY_HOST_PTR — driver copies host→device at buffer creation.
//   B. CL_MEM_USE_HOST_PTR  — driver may pin/zero-copy on UMA hardware.
//   C. SVM coarse-grained   — shared pointer with explicit map/unmap.
//   D. SVM fine-grained     — coherent shared pointer, no map needed.
//
// WHY CL_HPP_TARGET_OPENCL_VERSION 200 before opencl_utils.hpp:
//   opencl_utils.hpp uses #ifndef guards (defaults to 120). By defining 200
//   first, we unlock SVM API symbols (clSVMAlloc, CL_MEM_SVM_FINE_GRAIN_BUFFER,
//   etc.) without changing any other module's compilation.
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
#include <climits>
#include <cstring>
#include <iostream>
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


int main(int argc, char* argv[]) {
    CLI::App app{"SVM Deep Dive Benchmark"};
    int width      = 1920;
    int height     = 1080;
    int iterations = 10;
    app.add_option("--width",      width,      "Image width")    ->default_val(1920);
    app.add_option("--height",     height,     "Image height")   ->default_val(1080);
    app.add_option("--iterations", iterations, "Benchmark iterations")->default_val(10);
    CLI11_PARSE(app, argc, argv);

    auto ocl = select_device();
    std::cout << "Device: " << ocl.device.getInfo<CL_DEVICE_NAME>() << "\n";

    // §7.1: promote to size_t before multiplying to avoid 32-bit overflow.
    if (static_cast<size_t>(width) * static_cast<size_t>(height) >
        static_cast<size_t>(INT_MAX))
        throw std::runtime_error("Image too large: pixel count exceeds INT_MAX");

    const size_t pixel_count = static_cast<size_t>(width) * height;
    const size_t byte_size   = pixel_count * 4;
    const cl_int pixel_count_int = static_cast<cl_int>(pixel_count);

    // Build kernel — pass path directly to build_program (loads source internally).
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

    // =========================================================================
    // Path A: CL_MEM_COPY_HOST_PTR
    // Driver allocates device memory and copies from src at buffer creation.
    // Most portable path — works on all OpenCL 1.x hardware.
    // =========================================================================
    {
        double total_ms = 0.0;
        std::vector<uint8_t> readback(byte_size);
        for (int i = 0; i < iterations; ++i) {
            // WHY CL_MEM_COPY_HOST_PTR: transfers data to device on construction;
            // src is safe to modify on the host after this call returns.
            cl::Buffer in_buf(ocl.context,
                CL_MEM_COPY_HOST_PTR | CL_MEM_READ_ONLY, byte_size, src.data());
            cl::Buffer out_buf(ocl.context, CL_MEM_WRITE_ONLY, byte_size);
            cl::Kernel k(program, "passthrough");
            CL_CHECK(k.setArg(0, in_buf));
            CL_CHECK(k.setArg(1, out_buf));
            CL_CHECK(k.setArg(2, pixel_count_int));
            cl::Event ev;
            CL_CHECK(ocl.queue.enqueueNDRangeKernel(
                k, cl::NullRange, cl::NDRange(pixel_count), cl::NullRange,
                nullptr, &ev));
            CL_CHECK(ocl.queue.enqueueReadBuffer(
                out_buf, CL_TRUE, 0, byte_size, readback.data()));
            CL_CHECK(ocl.queue.finish());
            total_ms += duration_ms(ev);
        }
        if (std::memcmp(src.data(), readback.data(), byte_size) != 0)
            throw std::runtime_error("COPY_HOST_PTR: correctness check failed");
        save_bmp("output_svm_verify.bmp", readback, width, height, 4);
        printf("[COPY_HOST_PTR ] %dx%d round-trip: %.3f ms  (mean over %d iterations)\n",
               width, height, total_ms / iterations, iterations);
    }

    // =========================================================================
    // Path B: CL_MEM_USE_HOST_PTR
    // Driver may use the host pointer directly (zero-copy / pinned) on UMA
    // hardware; on discrete GPUs it typically still performs a copy via PCIe.
    // =========================================================================
    {
        double total_ms = 0.0;
        std::vector<uint8_t> readback(byte_size);
        for (int i = 0; i < iterations; ++i) {
            // WHY CL_MEM_USE_HOST_PTR: on iGPU/APU hardware where CPU and GPU
            // share physical memory, the driver can map the host buffer directly
            // rather than copying — eliminating PCIe transfer latency entirely.
            cl::Buffer in_buf(ocl.context,
                CL_MEM_USE_HOST_PTR | CL_MEM_READ_ONLY, byte_size, src.data());
            cl::Buffer out_buf(ocl.context, CL_MEM_WRITE_ONLY, byte_size);
            cl::Kernel k(program, "passthrough");
            CL_CHECK(k.setArg(0, in_buf));
            CL_CHECK(k.setArg(1, out_buf));
            CL_CHECK(k.setArg(2, pixel_count_int));
            cl::Event ev;
            CL_CHECK(ocl.queue.enqueueNDRangeKernel(
                k, cl::NullRange, cl::NDRange(pixel_count), cl::NullRange,
                nullptr, &ev));
            CL_CHECK(ocl.queue.enqueueReadBuffer(
                out_buf, CL_TRUE, 0, byte_size, readback.data()));
            CL_CHECK(ocl.queue.finish());
            total_ms += duration_ms(ev);
        }
        if (std::memcmp(src.data(), readback.data(), byte_size) != 0)
            throw std::runtime_error("USE_HOST_PTR: correctness check failed");
        printf("[USE_HOST_PTR  ] %dx%d round-trip: %.3f ms\n",
               width, height, total_ms / iterations);
    }

    // =========================================================================
    // Paths C & D: SVM (OpenCL 2.0+)
    // SVM gives host and device a shared virtual address space, removing the
    // need for explicit buffer copies when both sides can access the same pointer.
    // =========================================================================
#ifdef CL_VERSION_2_0
    {
        // WHY capabilities query (not CL_DEVICE_OPENCL_C_VERSION): the legacy
        // CL_DEVICE_OPENCL_C_VERSION string returns "OpenCL C 1.2" even on
        // OpenCL 3.0 drivers that expose SVM; the bitmask is authoritative.
        cl_device_svm_capabilities svm_caps = 0;
        CL_CHECK(clGetDeviceInfo(ocl.device(), CL_DEVICE_SVM_CAPABILITIES,
                                 sizeof(svm_caps), &svm_caps, nullptr));
        const bool has_coarse   = (svm_caps & CL_DEVICE_SVM_COARSE_GRAIN_BUFFER) != 0;
        const bool has_fine_sys = (svm_caps & CL_DEVICE_SVM_FINE_GRAIN_SYSTEM)   != 0;

        // =====================================================================
        // Path C: SVM coarse-grained
        // Host must map/unmap before reading or writing the SVM region; the
        // driver uses these fences to synchronise cache lines between CPU and GPU.
        // =====================================================================
        if (!has_coarse) {
            printf("[SVM coarse    ] SKIPPED — CL_DEVICE_SVM_COARSE_GRAIN_BUFFER not supported\n");
        } else {
            void* svm_in  = clSVMAlloc(ocl.context(), CL_MEM_READ_ONLY,  byte_size, 0);
            void* svm_out = clSVMAlloc(ocl.context(), CL_MEM_WRITE_ONLY, byte_size, 0);
            if (!svm_in || !svm_out) {
                if (svm_in)  clSVMFree(ocl.context(), svm_in);
                if (svm_out) clSVMFree(ocl.context(), svm_out);
                throw std::runtime_error("clSVMAlloc (coarse) failed");
            }

            // Map → fill → unmap: host write to coarse SVM requires an open map
            // window (§7.7: coarse-grained SVM requires clEnqueueSVMMap before
            // any host read/write).
            CL_CHECK(clEnqueueSVMMap(ocl.queue(), CL_TRUE, CL_MAP_WRITE,
                                     svm_in, byte_size, 0, nullptr, nullptr));
            std::memcpy(svm_in, src.data(), byte_size);
            CL_CHECK(clEnqueueSVMUnmap(ocl.queue(), svm_in, 0, nullptr, nullptr));
            CL_CHECK(clFinish(ocl.queue()));

            double total_ms = 0.0;
            cl::Kernel k(program, "passthrough");
            for (int i = 0; i < iterations; ++i) {
                // WHY clSetKernelArgSVMPointer (not cl::Kernel::setArg):
                // the C++ wrapper has no overload for raw SVM pointers; the
                // raw C API is the only supported path for SVM kernel args.
                CL_CHECK(clSetKernelArgSVMPointer(k(), 0, svm_in));
                CL_CHECK(clSetKernelArgSVMPointer(k(), 1, svm_out));
                CL_CHECK(k.setArg(2, pixel_count_int));
                cl::Event ev;
                CL_CHECK(ocl.queue.enqueueNDRangeKernel(
                    k, cl::NullRange, cl::NDRange(pixel_count), cl::NullRange,
                    nullptr, &ev));
                CL_CHECK(clFinish(ocl.queue()));
                total_ms += duration_ms(ev);
            }

            // Map for read-back — must open a read window before memcpy.
            CL_CHECK(clEnqueueSVMMap(ocl.queue(), CL_TRUE, CL_MAP_READ,
                                     svm_out, byte_size, 0, nullptr, nullptr));
            std::vector<uint8_t> result_coarse(byte_size);
            std::memcpy(result_coarse.data(), svm_out, byte_size);
            CL_CHECK(clEnqueueSVMUnmap(ocl.queue(), svm_out, 0, nullptr, nullptr));
            CL_CHECK(clFinish(ocl.queue()));

            clSVMFree(ocl.context(), svm_in);
            clSVMFree(ocl.context(), svm_out);

            if (std::memcmp(src.data(), result_coarse.data(), byte_size) != 0)
                throw std::runtime_error("SVM coarse: correctness check failed");
            printf("[SVM coarse    ] %dx%d round-trip: %.3f ms\n",
                   width, height, total_ms / iterations);
        }

        // =====================================================================
        // Path D: SVM fine-grained system
        // Host and device share coherent system memory — no map/unmap required.
        // Direct pointer access is safe from both sides at all times.
        // =====================================================================
        if (!has_fine_sys) {
            printf("[SVM fine      ] SKIPPED — CL_DEVICE_SVM_FINE_GRAIN_SYSTEM not supported\n");
        } else {
            // Plain clSVMAlloc (no extra flags) for fine-grained system SVM —
            // system coherency is a device capability, not an allocation flag.
            void* svm_in  = clSVMAlloc(ocl.context(), CL_MEM_READ_ONLY,  byte_size, 0);
            void* svm_out = clSVMAlloc(ocl.context(), CL_MEM_WRITE_ONLY, byte_size, 0);
            if (!svm_in || !svm_out) {
                if (svm_in)  clSVMFree(ocl.context(), svm_in);
                if (svm_out) clSVMFree(ocl.context(), svm_out);
                throw std::runtime_error("clSVMAlloc (fine) failed");
            }

            // Direct host write — no map needed for fine-grained coherent memory.
            std::memcpy(svm_in, src.data(), byte_size);

            double total_ms = 0.0;
            cl::Kernel k(program, "passthrough");
            for (int i = 0; i < iterations; ++i) {
                CL_CHECK(clSetKernelArgSVMPointer(k(), 0, svm_in));
                CL_CHECK(clSetKernelArgSVMPointer(k(), 1, svm_out));
                CL_CHECK(k.setArg(2, pixel_count_int));
                cl::Event ev;
                CL_CHECK(ocl.queue.enqueueNDRangeKernel(
                    k, cl::NullRange, cl::NDRange(pixel_count), cl::NullRange,
                    nullptr, &ev));
                CL_CHECK(clFinish(ocl.queue()));
                total_ms += duration_ms(ev);
            }

            // Direct host read — coherent, no map needed.
            std::vector<uint8_t> result_fine(byte_size);
            std::memcpy(result_fine.data(), svm_out, byte_size);

            clSVMFree(ocl.context(), svm_in);
            clSVMFree(ocl.context(), svm_out);

            if (std::memcmp(src.data(), result_fine.data(), byte_size) != 0)
                throw std::runtime_error("SVM fine: correctness check failed");
            printf("[SVM fine      ] %dx%d round-trip: %.3f ms\n",
                   width, height, total_ms / iterations);
        }
    }
#else
    // Headers pre-date OpenCL 2.0 — SVM symbols unavailable at compile time.
    printf("[SVM coarse    ] SKIPPED — compiled without CL_VERSION_2_0\n");
    printf("[SVM fine      ] SKIPPED — compiled without CL_VERSION_2_0\n");
#endif

    return 0;
}
