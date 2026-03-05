// SVM — benchmarks three OpenCL host-device data-sharing strategies:
//   A. Buffer + Map/Unmap  (baseline, works on OpenCL 1.x)
//   B. Coarse-grained SVM  (OpenCL 2.0+)
//   C. Fine-grained SVM    (OpenCL 2.0+, optional capability)
//
// WHY compare these: SVM lets the host and device share a single virtual
// address space, eliminating explicit copies and map/unmap overhead that
// Buffer+Map requires.  This tool makes the trade-off visible as numbers.
//
// OpenCL version note: we target 2.0 here so that SVM API symbols
// (clSVMAlloc, CL_MEM_SVM_FINE_GRAIN_BUFFER, etc.) are available at compile
// time.  All SVM code paths are guarded by #ifdef CL_VERSION_2_0 so the
// translation unit still compiles cleanly when those macros are absent.

// Must be defined BEFORE including opencl.hpp to unlock the 2.0 symbols.
#define CL_HPP_TARGET_OPENCL_VERSION  200
#define CL_HPP_MINIMUM_OPENCL_VERSION 120
#define CL_HPP_ENABLE_EXCEPTIONS

#include <CL/opencl.hpp>
#include <CLI/CLI.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <fstream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Local CL_CHECK — mirrors common/opencl_utils.hpp but without pulling in the
// version-120 header which would cause CL_HPP_TARGET_OPENCL_VERSION conflicts.
// ---------------------------------------------------------------------------
#define CL_CHECK(err)                                                          \
    do {                                                                       \
        if ((err) != CL_SUCCESS) {                                             \
            throw std::runtime_error(std::string("OpenCL error ") +            \
                                     std::to_string(err) + " at " +            \
                                     __FILE__ + ":" + std::to_string(__LINE__)); \
        }                                                                      \
    } while (0)

// ---------------------------------------------------------------------------
// duration_ms — convert cl::Event profiling timestamps (nanoseconds) to ms.
// Requires CL_QUEUE_PROFILING_ENABLE and queue.finish() before calling.
// ---------------------------------------------------------------------------
static double duration_ms(const cl::Event& e) {
    return (e.getProfilingInfo<CL_PROFILING_COMMAND_END>() -
            e.getProfilingInfo<CL_PROFILING_COMMAND_START>()) / 1e6;
}

// ---------------------------------------------------------------------------
// load_kernel_source — reads an OpenCL .cl file from disk.
// ---------------------------------------------------------------------------
static std::string load_kernel_source(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open())
        throw std::runtime_error("Failed to open kernel file: " + path);
    std::ostringstream buf;
    buf << file.rdbuf();
    return buf.str();
}

// ---------------------------------------------------------------------------
// GPU env-var device selection — inline implementation of the same logic
// from common/ocl_wrapper.hpp, avoiding the version-120 include.
// Substring match on CL_PLATFORM_VENDOR / CL_DEVICE_VENDOR, case-insensitive.
// Falls back to first GPU, then first CPU if no GPU found.
// Issue fix: when GPU hint is set, also search all device types (not just
// CL_DEVICE_TYPE_GPU) so that GPU=INTEL matches on a CPU-only OpenCL stack.
// ---------------------------------------------------------------------------
struct OclSetup {
    cl::Platform     platform;
    cl::Device       device;
    cl::Context      context;
};

static OclSetup select_device() {
    std::vector<cl::Platform> platforms;
    cl::Platform::get(&platforms);
    if (platforms.empty())
        throw std::runtime_error("No OpenCL platforms found. Check your drivers.");

    const char* gpu_hint_raw = std::getenv("GPU");
    std::string gpu_hint     = gpu_hint_raw ? gpu_hint_raw : "";
    std::transform(gpu_hint.begin(), gpu_hint.end(), gpu_hint.begin(), ::toupper);

    auto contains_hint = [&](const std::string& s) {
        std::string upper = s;
        std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);
        return upper.find(gpu_hint) != std::string::npos;
    };

    cl::Platform selected_platform;
    cl::Device   selected_device;
    bool         found = false;

    if (!gpu_hint.empty()) {
        // First pass: search GPU-type devices only.
        // WHY iterate all GPU devices: the hint vendor string may only appear
        // on a non-first GPU (e.g. multi-GPU system). Checking only gpus.front()
        // would miss all but the first device on a platform.
        for (auto& p : platforms) {
            std::vector<cl::Device> gpus;
            p.getDevices(CL_DEVICE_TYPE_GPU, &gpus);
            if (gpus.empty()) continue;
            bool plat_match = contains_hint(p.getInfo<CL_PLATFORM_VENDOR>());
            for (auto& g : gpus) {
                if (plat_match || contains_hint(g.getInfo<CL_DEVICE_VENDOR>())) {
                    selected_platform = p;
                    selected_device   = g;
                    found             = true;
                    break;
                }
            }
            if (found) break;
        }
        // Second pass: if not found via GPU type, search all device types.
        // WHY: vendor strings like "INTEL" may appear on CPU-only OpenCL stacks
        // (e.g. Intel OpenCL runtime exposed as CPU device).
        if (!found) {
            for (auto& p : platforms) {
                std::vector<cl::Device> all_devs;
                p.getDevices(CL_DEVICE_TYPE_ALL, &all_devs);
                for (auto& d : all_devs) {
                    if (contains_hint(p.getInfo<CL_PLATFORM_VENDOR>()) ||
                        contains_hint(d.getInfo<CL_DEVICE_VENDOR>())) {
                        selected_platform = p;
                        selected_device   = d;
                        found             = true;
                        break;
                    }
                }
                if (found) break;
            }
        }
        if (!found) {
            // WHY exit(0) instead of throw: spec requires that a missing GPU
            // vendor is a graceful informational exit, not a crash.  The binary
            // is not in an error state — the user simply asked for hardware that
            // isn't present on this machine.
            std::cout << "[INFO] No device matching GPU=" << gpu_hint_raw
                      << " found. Available platforms:\n";
            for (auto& p : platforms) {
                std::cout << "  - " << p.getInfo<CL_PLATFORM_VENDOR>();
                std::vector<cl::Device> gpus;
                p.getDevices(CL_DEVICE_TYPE_GPU, &gpus);
                if (!gpus.empty())
                    std::cout << " / device: " << gpus.front().getInfo<CL_DEVICE_VENDOR>();
                std::cout << "\n";
            }
            std::cout << "[INFO] Exiting gracefully.\n";
            std::exit(0);
        }
    } else {
        // Default: first platform with a GPU; fall back to CPU.
        for (auto& p : platforms) {
            std::vector<cl::Device> gpus;
            p.getDevices(CL_DEVICE_TYPE_GPU, &gpus);
            if (!gpus.empty()) {
                selected_platform = p;
                selected_device   = gpus.front();
                found             = true;
                break;
            }
        }
        if (!found) {
            for (auto& p : platforms) {
                std::vector<cl::Device> cpus;
                p.getDevices(CL_DEVICE_TYPE_CPU, &cpus);
                if (!cpus.empty()) {
                    selected_platform = p;
                    selected_device   = cpus.front();
                    found             = true;
                    break;
                }
            }
        }
        if (!found)
            throw std::runtime_error("No usable OpenCL device found.");
    }

    cl::Context ctx(selected_device);
    return OclSetup{selected_platform, selected_device, ctx};
}

// ---------------------------------------------------------------------------
// parse_opencl_c_major — extract the major version number from the string
// returned by CL_DEVICE_VERSION, e.g. "OpenCL C 3.0 Mesa..." -> 3.
// Returns 0 on any parse failure (safe fallback for unknown format).
// ---------------------------------------------------------------------------
static int parse_opencl_c_major(const std::string& ver_str) {
    // Format: "OpenCL C <major>.<minor> ..."
    std::istringstream iss(ver_str);
    std::string token;

    // Read the string word by word, splitting by spaces
    while (iss >> token) {
        try {
            // Attempt to convert the token to a double
            // std::stod naturally stops parsing when it hits non-numeric characters,
            // meaning "3.0" parses cleanly even if the token had trailing characters.
            return std::stod(token);
        } catch (const std::invalid_argument&) {
            // Token is not a number (e.g., "OpenCL", "C", "NEO"), skip to the next
            continue;
        } catch (const std::out_of_range&) {
            // Token represents a number too large for a double, skip
            continue;
        }
    }
    
    return 0;
}

// ---------------------------------------------------------------------------
// Table row helpers
// ---------------------------------------------------------------------------
static void print_table_header() {
    std::cout << "\u250c\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u252c\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u252c\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2510\n";
    std::cout << "\u2502 " << std::left << std::setw(23) << "Strategy"
              << "\u2502 " << std::setw(12) << "Map (ms)"
              << "\u2502 " << std::setw(12) << "Kernel (ms)"
              << "\u2502\n";
    std::cout << "\u251c\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u253c\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u253c\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2524\n";
}

static void print_table_row(const std::string& name,
                            const std::string& map_ms_str,
                            double             kernel_ms) {
    std::ostringstream kernel_ss;
    kernel_ss << std::fixed << std::setprecision(3) << kernel_ms;
    std::cout << "\u2502 " << std::left  << std::setw(23) << name
              << "\u2502 " << std::right << std::setw(12) << map_ms_str
              << "\u2502 " << std::right << std::setw(12) << kernel_ss.str()
              << "\u2502\n";
}

static void print_table_footer() {
    std::cout << "\u2514\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2534\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2534\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2518\n";
}

// ---------------------------------------------------------------------------
// verify_result — correctness gate for scale_add(scale=2.0, offset=1.0).
// Expected: result[0] = 0.0 * 2.0 + 1.0 = 1.0  (initial value 0 * 0.001 = 0.0)
// ---------------------------------------------------------------------------
static bool verify_result(const std::string& mode_name, float result0) {
    const float expected = 1.0f;   // 0 * 0.001 * 2.0 + 1.0
    if (std::fabs(result0 - expected) >= 1e-4f) {
        std::cout << "[FAIL] Mode " << mode_name
                  << ": result[0] = " << std::fixed << std::setprecision(6) << result0
                  << ", expected "   << expected << "\n";
        return false;
    }
    return true;
}

// ===========================================================================
// main
// ===========================================================================
int main(int argc, char** argv) {

    // ── CLI ─────────────────────────────────────────────────────────────────
    CLI::App app{"SVM — OpenCL host-device sharing strategy benchmark"};

    int         size_param = 1048576;   // default 1 M floats = 4 MB
    std::string mode       = "all";

    app.add_option("--size", size_param,
                   "Number of float elements (default 1048576)")
       ->check(CLI::PositiveNumber);
    app.add_option("--mode", mode,
                   "Benchmark mode: all | buffer_map | coarse_svm | fine_svm (default all)")
       ->check(CLI::IsMember({"all", "buffer_map", "coarse_svm", "fine_svm"}));

    CLI11_PARSE(app, argc, argv);

    const size_t elem_count = static_cast<size_t>(size_param);

    // ── Device selection ────────────────────────────────────────────────────
    OclSetup ocl = select_device();

    const std::string dev_name  = ocl.device.getInfo<CL_DEVICE_NAME>();
    const std::string ocl_c_ver = ocl.device.getInfo<CL_DEVICE_VERSION>();

    std::cout << "Device: " << dev_name << " (" << ocl_c_ver << ")\n";
    std::cout << "Buffer size: " << (elem_count * sizeof(float))
              << " bytes (" << elem_count << " floats)\n\n";

    // ── OpenCL 1.x graceful exit ─────────────────────────────────────────────
    if (parse_opencl_c_major(ocl_c_ver) < 2) {
        std::cout << "[INFO] Device reports OpenCL C 1.x. SVM is an OpenCL 2.0 feature.\n";
        std::cout << "[INFO] Use the ZeroCopy tool (99_Toolbox/ZeroCopy/) for host-device "
                     "transfer optimization on OpenCL 1.x.\n";
        return 0;
    }

    // ── Command queue with profiling ─────────────────────────────────────────
    // WHY explicit queue here: we need CL_QUEUE_PROFILING_ENABLE for cl::Event
    // timestamps; the default OclContext queue doesn't carry this flag.
    cl::CommandQueue queue(ocl.context, ocl.device, CL_QUEUE_PROFILING_ENABLE);

    // ── Kernel compilation ───────────────────────────────────────────────────
    // Derive kernel path relative to argv[0] so it works regardless of the
    // working directory from which the binary is invoked.
    const std::string kernel_path =
        (std::filesystem::path(argv[0]).parent_path() / "kernels" / "svm_kernel.cl")
        .string();
    const std::string kernel_src = load_kernel_source(kernel_path);
    cl::Program program(ocl.context, cl::Program::Sources{kernel_src});
    try {
        program.build({ocl.device});
    } catch (const cl::Error&) {
        std::cerr << "Build log:\n"
                  << program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(ocl.device) << "\n";
        throw;
    }
    cl::Kernel kernel(program, "scale_add");

    // WHY cl_ulong: kernel parameter is size_t; cl_ulong (64-bit) matches
    // the widest possible size_t across all OpenCL platforms.
    const cl_ulong n_cl = static_cast<cl_ulong>(elem_count);
    const float  scale  = 2.0f;
    const float  offset = 1.0f;

    // Table rows collected for printing after all runs.
    struct Row {
        std::string name;
        std::string map_str;
        double      kernel_ms{0.0};
    };
    std::vector<Row> rows;
    bool all_pass = true;

    // =========================================================================
    // Mode A: Buffer + Map/Unmap (baseline — no SVM, works on OpenCL 1.x)
    // WHY outside #ifdef CL_VERSION_2_0: this mode uses only OpenCL 1.x APIs
    // (cl::Buffer, enqueueMapBuffer) and must run on all supported devices.
    // =========================================================================
    if (mode == "all" || mode == "buffer_map") {
        // Fill host vector with i * 0.001f so element 0 starts at 0.0.
        std::vector<float> host_data(elem_count);
        for (size_t i = 0; i < elem_count; ++i)
            host_data[i] = static_cast<float>(i) * 0.001f;

        // CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR: driver allocates device
        // memory and pre-populates it from host_data in one step.
        cl::Buffer buf(ocl.context,
                       CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
                       elem_count * sizeof(float),
                       host_data.data());

        // Time the map/unmap round trip with std::chrono (no GPU event for host ops).
        auto t0 = std::chrono::steady_clock::now();
        void* mapped_ptr = queue.enqueueMapBuffer(
            buf, CL_TRUE,
            CL_MAP_READ | CL_MAP_WRITE,
            0, elem_count * sizeof(float));
        CL_CHECK(queue.enqueueUnmapMemObject(buf, mapped_ptr));
        // WHY finish() before recording t1: ensures map + unmap commands have
        // fully completed on the device before the chrono timestamp is taken.
        CL_CHECK(queue.finish());
        auto t1 = std::chrono::steady_clock::now();
        double map_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

        // Run the kernel.
        CL_CHECK(kernel.setArg(0, buf));
        CL_CHECK(kernel.setArg(1, n_cl));
        CL_CHECK(kernel.setArg(2, scale));
        CL_CHECK(kernel.setArg(3, offset));

        cl::Event evt;
        CL_CHECK(queue.enqueueNDRangeKernel(
            kernel, cl::NullRange, cl::NDRange(elem_count), cl::NullRange,
            nullptr, &evt));
        CL_CHECK(queue.finish());

        // Read back element 0 for correctness check.
        float result0 = 0.0f;
        CL_CHECK(queue.enqueueReadBuffer(buf, CL_TRUE, 0, sizeof(float), &result0));

        double kernel_ms = duration_ms(evt);

        std::ostringstream map_ss;
        map_ss << std::fixed << std::setprecision(3) << map_ms;
        rows.push_back({"Buffer + Map/Unmap", map_ss.str(), kernel_ms});
        if (!verify_result("buffer_map", result0)) all_pass = false;
    }

#ifdef CL_VERSION_2_0
    // ── SVM capability query ─────────────────────────────────────────────────
    cl_device_svm_capabilities svm_caps = 0;
    CL_CHECK(ocl.device.getInfo(CL_DEVICE_SVM_CAPABILITIES, &svm_caps));

    const bool has_coarse = (svm_caps & CL_DEVICE_SVM_COARSE_GRAIN_BUFFER) != 0;
    const bool has_fine   = (svm_caps & CL_DEVICE_SVM_FINE_GRAIN_BUFFER)   != 0;

    std::cout << "SVM Capability: Coarse=" << (has_coarse ? "YES" : "NO")
              << "  Fine-grain=" << (has_fine ? "YES" : "NO") << "\n\n";

    // =========================================================================
    // Mode B: Coarse-grained SVM
    // =========================================================================
    if (mode == "all" || mode == "coarse_svm") {
        if (!has_coarse) {
            std::cout << "[SKIP] Coarse SVM not supported\n";
        } else {
            // clSVMAlloc returns a pointer shared between host and device.
            // The host must explicitly map/unmap to synchronise (coarse grain).
            float* svm_ptr = static_cast<float*>(
                clSVMAlloc(ocl.context(), CL_MEM_READ_WRITE,
                           elem_count * sizeof(float), 0));
            if (!svm_ptr)
                throw std::runtime_error("clSVMAlloc failed for coarse SVM");

            // Timing map: measures map/unmap latency only — no fill inside.
            // WHY separate timing pass: writing to svm_ptr outside a map window
            // is UB for coarse-grained SVM; the fill must happen inside its own
            // map window after the timing window is closed.
            auto t0 = std::chrono::steady_clock::now();
            CL_CHECK(clEnqueueSVMMap(queue(), CL_TRUE,
                                     CL_MAP_WRITE,
                                     svm_ptr, elem_count * sizeof(float),
                                     0, nullptr, nullptr));
            CL_CHECK(queue.finish());
            CL_CHECK(clEnqueueSVMUnmap(queue(), svm_ptr, 0, nullptr, nullptr));
            // WHY finish() before t1: ensures both commands have completed on the
            // device before the chrono timestamp is captured.
            CL_CHECK(queue.finish());
            auto t1 = std::chrono::steady_clock::now();
            double map_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

            // Data init map: fill must occur inside a map window per OpenCL spec.
            // WHY second map: coarse-grained SVM requires explicit host-visible
            // mapping before the host may legally read or write the SVM region.
            CL_CHECK(clEnqueueSVMMap(queue(), CL_TRUE,
                                     CL_MAP_WRITE,
                                     svm_ptr, elem_count * sizeof(float),
                                     0, nullptr, nullptr));
            for (size_t i = 0; i < elem_count; ++i)
                svm_ptr[i] = static_cast<float>(i) * 0.001f;
            CL_CHECK(clEnqueueSVMUnmap(queue(), svm_ptr, 0, nullptr, nullptr));
            CL_CHECK(queue.finish());

            // WHY clSetKernelArgSVMPointer: the C++ wrapper cl::Kernel::setArg
            // does not have an overload for raw SVM pointers; the raw C API is
            // required here specifically for SVM pointer arguments.
            cl_int svm_arg_err = clSetKernelArgSVMPointer(kernel(), 0, svm_ptr);
            CL_CHECK(svm_arg_err);
            CL_CHECK(kernel.setArg(1, n_cl));
            CL_CHECK(kernel.setArg(2, scale));
            CL_CHECK(kernel.setArg(3, offset));

            cl::Event evt;
            CL_CHECK(queue.enqueueNDRangeKernel(
                kernel, cl::NullRange, cl::NDRange(elem_count), cl::NullRange,
                nullptr, &evt));
            CL_CHECK(queue.finish());

            // Map for read-back and correctness check.
            CL_CHECK(clEnqueueSVMMap(queue(), CL_TRUE, CL_MAP_READ,
                                     svm_ptr, sizeof(float),
                                     0, nullptr, nullptr));
            float result0 = svm_ptr[0];
            CL_CHECK(clEnqueueSVMUnmap(queue(), svm_ptr, 0, nullptr, nullptr));
            CL_CHECK(queue.finish());

            clSVMFree(ocl.context(), svm_ptr);

            double kernel_ms = duration_ms(evt);
            std::ostringstream map_ss;
            map_ss << std::fixed << std::setprecision(3) << map_ms;
            rows.push_back({"Coarse-grained SVM", map_ss.str(), kernel_ms});
            if (!verify_result("coarse_svm", result0)) all_pass = false;
        }
    }

    // =========================================================================
    // Mode C: Fine-grained SVM
    // =========================================================================
    if (mode == "all" || mode == "fine_svm") {
        if (!has_fine) {
            std::cout << "[SKIP] Fine-grained SVM not supported on this device\n";
        } else {
            // CL_MEM_SVM_FINE_GRAIN_BUFFER: host and device share coherent memory —
            // no explicit map/unmap is required; writes are immediately visible.
            float* svm_ptr = static_cast<float*>(
                clSVMAlloc(ocl.context(),
                           CL_MEM_READ_WRITE | CL_MEM_SVM_FINE_GRAIN_BUFFER,
                           elem_count * sizeof(float), 0));
            if (!svm_ptr)
                throw std::runtime_error("clSVMAlloc failed for fine-grained SVM");

            // No map needed — write directly to shared memory.
            for (size_t i = 0; i < elem_count; ++i)
                svm_ptr[i] = static_cast<float>(i) * 0.001f;

            CL_CHECK(clSetKernelArgSVMPointer(kernel(), 0, svm_ptr));
            CL_CHECK(kernel.setArg(1, n_cl));
            CL_CHECK(kernel.setArg(2, scale));
            CL_CHECK(kernel.setArg(3, offset));

            cl::Event evt;
            CL_CHECK(queue.enqueueNDRangeKernel(
                kernel, cl::NullRange, cl::NDRange(elem_count), cl::NullRange,
                nullptr, &evt));
            CL_CHECK(queue.finish());

            float result0 = svm_ptr[0];
            clSVMFree(ocl.context(), svm_ptr);

            double kernel_ms = duration_ms(evt);
            rows.push_back({"Fine-grained SVM", "0.000 *", kernel_ms});
            if (!verify_result("fine_svm", result0)) all_pass = false;
        }
    }
#else
    // This branch is only reached when CL_VERSION_2_0 is not defined at all
    // (e.g., system OpenCL headers older than 2.0).
    std::cout << "[INFO] Compiled without CL_VERSION_2_0 support. SVM unavailable.\n";
    if (mode == "coarse_svm" || mode == "fine_svm") {
        std::cout << "[SKIP] Requested mode requires OpenCL 2.0 headers.\n";
    }
#endif

    // ── Print results table ──────────────────────────────────────────────────
    print_table_header();
    for (const auto& r : rows)
        print_table_row(r.name, r.map_str, r.kernel_ms);
    print_table_footer();

#ifdef CL_VERSION_2_0
    bool fine_active = (mode == "all" || mode == "fine_svm");
    // Only print footnote if fine SVM is both requested and available.
    if (fine_active && has_fine)
        std::cout << "* Fine-grained SVM requires no explicit synchronization.\n";
#endif
    std::cout << "\n";

    if (all_pass)
        std::cout << "[PASS] All active variants produced correct output.\n";
    else
        return 1;

    return 0;
}
