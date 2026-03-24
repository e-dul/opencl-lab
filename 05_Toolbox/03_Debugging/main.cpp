// debug_demo — Intentionally buggy OpenCL binary for Oclgrind demonstrations.
//
// Modes:
//   --test out_of_bounds   : kernel writes buf[64] (one element past end of
//                            a 64-element allocation).  Oclgrind flags "Invalid write".
//   --test race_condition  : all 64 work-items write 1.0f to buf[0] without
//                            synchronisation.  Oclgrind --check-api flags the race.
//
// WHY this tool exists:
//   Out-of-bounds global memory writes and work-item data races are SILENT on
//   real GPU hardware — the GPU has no fault-isolation per allocation.  Oclgrind
//   runs the kernel on the CPU with full memory instrumentation and catches both
//   classes of bug before they cause non-deterministic failures in production.
//
// WHY exit 0 always:
//   The host never observes the memory error — it occurs inside the kernel on
//   GPU (or inside Oclgrind's simulator).  The host-side CL API returns SUCCESS
//   in both cases; only Oclgrind's instrumentation layer reports the violation.

#include "opencl_utils.hpp"  // CL_CHECK, load_kernel_source; pulls in cl.hpp + macros
#include "ocl_wrapper.hpp"   // create_context(), OclContext

#include <CLI/CLI.hpp>

#include <iostream>
#include <stdexcept>
#include <string>

// Fixed buffer length: small enough for fast Oclgrind instrumentation, large
// enough that a single work-group of 64 fills it completely.
static constexpr int   N               = 64;
static constexpr int   KERNEL_MODE_OOB = 0;   // out-of-bounds
static constexpr int   KERNEL_MODE_RACE = 1;  // race condition

int main(int argc, char** argv)
{
    // ── CLI ──────────────────────────────────────────────────────────────────
    CLI::App app{"debug_demo — Oclgrind out-of-bounds and race-condition demo"};

    std::string test_mode;
    app.add_option("--test", test_mode,
                   "Bug to inject: out_of_bounds | race_condition")
       ->required();

    CLI11_PARSE(app, argc, argv);

    // Validate --test value explicitly so the error message is clear.
    if (test_mode != "out_of_bounds" && test_mode != "race_condition") {
        std::cerr << "Error: --test must be 'out_of_bounds' or 'race_condition',"
                  << " got: '" << test_mode << "'\n";
        return 1;
    }

    const int kernel_mode = (test_mode == "out_of_bounds")
                                ? KERNEL_MODE_OOB
                                : KERNEL_MODE_RACE;

    // ── OpenCL setup ─────────────────────────────────────────────────────────
    // WHY create_context(): honours GPU env var; hard-coded device indices are
    // forbidden by master specs §5 — they break multi-vendor environments.
    OclContext ocl = create_context();

    // WHY no CL_QUEUE_PROFILING_ENABLE: this tool has no timing requirement.
    // Profiling adds overhead that could obscure the Oclgrind output.

    // ── Buffer ───────────────────────────────────────────────────────────────
    // Allocate exactly N floats.  The out-of-bounds mode writes to index N,
    // which is one element past this allocation — the bug Oclgrind must catch.
    cl::Buffer buf(ocl.context, CL_MEM_READ_WRITE, static_cast<size_t>(N) * sizeof(float));

    // ── Kernel build ─────────────────────────────────────────────────────────
    // WHY load at runtime: .cl files stay editable without recompiling the host.
    const std::string src = load_kernel_source("kernels/debug_kernel.cl");
    cl::Program program(ocl.context, cl::Program::Sources{src});

    try {
        program.build({ocl.device});
    } catch (const cl::Error&) {
        std::cerr << "Kernel build log:\n"
                  << program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(ocl.device) << "\n";
        throw;
    }

    cl::Kernel kernel(program, "debug_kernel");

    // ── Kernel arguments ─────────────────────────────────────────────────────
    CL_CHECK(kernel.setArg(0, buf));
    CL_CHECK(kernel.setArg(1, static_cast<cl_int>(N)));
    CL_CHECK(kernel.setArg(2, static_cast<cl_int>(kernel_mode)));

    // ── Dispatch ─────────────────────────────────────────────────────────────
    // WHY global == local == 64: a single work-group is required so that all
    // 64 work-items share the same execution context — necessary for the race
    // condition to be detectable within one group.
    CL_CHECK(ocl.queue.enqueueNDRangeKernel(
        kernel,
        cl::NullRange,
        cl::NDRange(N),          // global_work_size = 64
        cl::NDRange(N),          // local_work_size  = 64 (single work-group)
        nullptr,
        nullptr));

    // WHY finish() before printing: ensures the kernel has completed (and any
    // Oclgrind instrumentation has fired) before we report success to the user.
    CL_CHECK(ocl.queue.finish());

    // ── Confirmation output ───────────────────────────────────────────────────
    if (test_mode == "out_of_bounds") {
        std::cout << "Bug injected: out-of-bounds write at buf[64] "
                     "(1 element past end). "
                     "Run under oclgrind to detect.\n";
    } else {
        std::cout << "Bug injected: race condition — all 64 work-items write "
                     "to buf[0]. "
                     "Run under oclgrind --check-api to detect.\n";
    }

    return 0;
}
