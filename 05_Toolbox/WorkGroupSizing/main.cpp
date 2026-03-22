// occupancy_demo — sweeps local_work_size values for a MAD kernel and reports
// kernel time (ms) alongside a software occupancy estimate.
//
// WHY sweep local_work_size: the GPU hides memory latency by switching between
// wavefronts/warps.  A work-group that is too small leaves hardware execution
// units idle; too large may spill registers.  This tool makes the sweet spot
// visible as concrete numbers without requiring vendor profiling tools.

#include "opencl_utils.hpp"  // CL_CHECK, duration_ms, load_kernel_source; pulls in cl.hpp + macros
#include "ocl_wrapper.hpp"   // create_context(), OclContext

#include <CLI/CLI.hpp>

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>


int main(int argc, char** argv) {
    // ── CLI ─────────────────────────────────────────────────────────────────
    CLI::App app{"occupancy_demo — WorkGroupSizing sweep benchmark"};

    int         width  = 1920;
    int         height = 1080;
    std::string kernel_name = "mad";

    app.add_option("--width",  width,       "Buffer width  (default 1920)");
    app.add_option("--height", height,      "Buffer height (default 1080)");
    app.add_option("--kernel", kernel_name, "Kernel name (default: mad)");

    CLI11_PARSE(app, argc, argv);

    if (width <= 0 || height <= 0) {
        throw std::runtime_error("--width and --height must be positive");
    }

    // ── Compute element count ───────────────────────────────────────────────
    // WHY promote width to size_t first: multiplying two ints would overflow
    // for large resolutions before the result is used as a size.
    const size_t n = static_cast<size_t>(width) * static_cast<size_t>(height);

    // Guard against overflow when passing n as cl_int to the kernel.
    if (n > static_cast<size_t>(std::numeric_limits<cl_int>::max())) {
        throw std::runtime_error("Buffer too large for cl_int kernel arg (n > INT_MAX)");
    }

    // ── OpenCL context ───────────────────────────────────────────────────────
    // WHY create_context(): honours GPU env var, avoids hard-coded device indices.
    OclContext ocl = create_context();

    // create_context() returns a queue without profiling; create a new one that
    // has CL_QUEUE_PROFILING_ENABLE so cl::Event timestamps are accurate.
    cl::CommandQueue prof_queue(ocl.context, ocl.device, CL_QUEUE_PROFILING_ENABLE);

    // ── Device capability query ──────────────────────────────────────────────
    // Read the hardware limit so we never submit a work-group larger than the
    // device can run — that would cause CL_INVALID_WORK_GROUP_SIZE.
    const size_t device_max_wgs = ocl.device.getInfo<CL_DEVICE_MAX_WORK_GROUP_SIZE>();
    std::cout << "Device max WGS   : " << device_max_wgs << "\n";
    std::cout << "Buffer elements  : " << n << " (float, "
              << (n * sizeof(float)) / (1024 * 1024) << " MB)\n\n";

    // ── Float buffer ────────────────────────────────────────────────────────
    // CL_MEM_READ_WRITE: the MAD kernel reads and writes the same buffer in-place.
    cl::Buffer buf(ocl.context, CL_MEM_READ_WRITE, n * sizeof(float));

    // ── Kernel dispatch map ──────────────────────────────────────────────────
    // WHY dispatch map: --kernel arg must select both the source file and entry
    // point; hardcoding them ignores user input and causes silent wrong behavior.
    struct KernelSpec { std::string src_path; std::string entry; };
    const std::map<std::string, KernelSpec> kernel_map = {
        {"mad", {"kernels/mad_kernel.cl", "mad_kernel"}},
    };

    if (kernel_map.find(kernel_name) == kernel_map.end()) {
        throw std::runtime_error(
            kernel_name == "blur_r5"
                ? "kernel 'blur_r5' not yet implemented"
                : "Unknown kernel: '" + kernel_name + "'. Supported: mad");
    }
    const auto& kspec = kernel_map.at(kernel_name);

    // ── Kernel build ────────────────────────────────────────────────────────
    // WHY load at runtime: .cl files stay editable without recompilation.
    const std::string src = load_kernel_source(kspec.src_path);
    cl::Program program(ocl.context, cl::Program::Sources{src});

    try {
        program.build({ocl.device});
    } catch (const cl::Error&) {
        std::cerr << "Kernel build log:\n"
                  << program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(ocl.device) << "\n";
        throw;
    }

    cl::Kernel kernel(program, kspec.entry.c_str());

    // ── Sweep ────────────────────────────────────────────────────────────────
    // Candidate local_work_size values; skip any exceeding device_max_wgs.
    const std::vector<size_t> candidates = {8, 16, 32, 64, 128, 256};

    struct Result {
        size_t lws;
        double ms;
    };
    std::vector<Result> results;

    for (size_t lws : candidates) {
        if (lws > device_max_wgs) {
            std::cout << "  lws=" << lws << " skipped (> device max " << device_max_wgs << ")\n";
            continue;
        }

        // Pad global size so it is an exact multiple of lws.
        const size_t global_size = round_up(n, lws);

        // Pass actual element count (not padded) so the kernel's boundary guard
        // prevents out-of-bounds writes on the tail partial group.
        CL_CHECK(kernel.setArg(0, buf));
        CL_CHECK(kernel.setArg(1, static_cast<cl_int>(n)));

        cl::Event evt;
        CL_CHECK(prof_queue.enqueueNDRangeKernel(
            kernel,
            cl::NullRange,
            cl::NDRange(global_size),
            cl::NDRange(lws),
            nullptr,
            &evt));

        // finish() must complete before querying profiling timestamps.
        CL_CHECK(prof_queue.finish());

        const double ms = duration_ms(evt);

        // Software occupancy estimate: fraction of max WGS used, expressed as %.
        // WHY this formula: it approximates how well the work-group fills a
        // compute unit's wavefront/warp capacity.  It is a simplified upper-bound
        // estimate — real occupancy also depends on register & LDS usage.
        const double occupancy = (static_cast<double>(lws) /
                                  static_cast<double>(device_max_wgs)) * 100.0;

        results.push_back({lws, ms});

        std::cout << "local_work_size=" << std::setw(3) << lws
                  << ": " << std::fixed << std::setprecision(3) << std::setw(9) << ms
                  << " ms  occupancy: " << std::fixed << std::setprecision(1)
                  << occupancy << "%\n";
    }

    // ── Sweet spot ──────────────────────────────────────────────────────────
    if (!results.empty()) {
        const auto best = std::min_element(results.begin(), results.end(),
            [](const Result& a, const Result& b) { return a.ms < b.ms; });

        std::cout << "\nSweet spot: local_work_size=" << best->lws
                  << " at " << std::fixed << std::setprecision(3) << best->ms << " ms\n";
    }

    return 0;
}
