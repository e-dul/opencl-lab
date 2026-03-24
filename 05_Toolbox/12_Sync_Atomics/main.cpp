// SyncAtomics — demonstrates the impact of atomics on histogram throughput.
// Three kernel variants run on 1M uchar elements (256 bins):
//   1. unsafe        — naked read-modify-write (intentionally racy, CORRUPT output)
//   2. global_atomic — OpenCL 1.2 atomic_add on __global int*
//   3. local_reduce  — local-accumulate-then-merge pattern
//
// All timings are from cl::Event profiling — no wall-clock measurements.

#include "opencl_utils.hpp"   // CL_CHECK, load_kernel_source, duration_ms
#include "ocl_wrapper.hpp"    // create_context(), OclContext

#include <CLI/CLI.hpp>

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

// ─────────────────────────────────────────────────────────────────────────────
// HistResult — per-variant outcome from correctness check.
// ─────────────────────────────────────────────────────────────────────────────
struct HistResult {
    int  bin_sum;
    bool correct;   // bin_sum == n_elements
};

// ─────────────────────────────────────────────────────────────────────────────
// zero_hist_buf — write 256 zeros into the histogram buffer before each dispatch.
// WHY: the same buffer is reused across variants; leftover counts from the
// previous run would corrupt subsequent results.
// ─────────────────────────────────────────────────────────────────────────────
static void zero_hist_buf(cl::CommandQueue& queue, cl::Buffer& buf) {
    static const std::vector<int> zeros(256, 0);
    CL_CHECK(queue.enqueueWriteBuffer(buf, CL_TRUE, 0,
                                      256 * sizeof(int),
                                      zeros.data()));
}

// ─────────────────────────────────────────────────────────────────────────────
// run_histogram_variant — dispatch one kernel, return cl::Event kernel time (ms).
// is_local controls the extra __local arg required by histogram_local.
// ─────────────────────────────────────────────────────────────────────────────
static double run_histogram_variant(cl::CommandQueue& queue,
                                    cl::Kernel&       kernel,
                                    const cl::Buffer& buf_data,
                                    cl::Buffer&       buf_hist,
                                    int               n_elements,
                                    int               local_size,
                                    bool              is_local) {
    zero_hist_buf(queue, buf_hist);

    if (is_local) {
        // histogram_local: data, global_hist, __local local_hist, size
        CL_CHECK(kernel.setArg(0, buf_data));
        CL_CHECK(kernel.setArg(1, buf_hist));
        // WHY cl::Local: allocates local memory per work-group without a host pointer.
        // 256 bins × sizeof(int) bytes are allocated in fast on-chip scratchpad.
        CL_CHECK(kernel.setArg(2, cl::Local(256 * sizeof(int))));
        CL_CHECK(kernel.setArg(3, static_cast<cl_int>(n_elements)));
    } else {
        // histogram_unsafe / histogram_global_atomic: data, histogram, size
        CL_CHECK(kernel.setArg(0, buf_data));
        CL_CHECK(kernel.setArg(1, buf_hist));
        CL_CHECK(kernel.setArg(2, static_cast<cl_int>(n_elements)));
    }

    // Round global work size up to next multiple of local_size.
    size_t global_size = static_cast<size_t>(
        (n_elements + local_size - 1) / local_size) * static_cast<size_t>(local_size);

    cl::Event ev;
    CL_CHECK(queue.enqueueNDRangeKernel(
        kernel, cl::NullRange,
        cl::NDRange(global_size),
        is_local ? cl::NDRange(static_cast<size_t>(local_size)) : cl::NullRange,
        nullptr, &ev));
    CL_CHECK(queue.finish());
    return duration_ms(ev);
}

// ─────────────────────────────────────────────────────────────────────────────
// check_histogram — read back the GPU histogram and verify bin sum.
// Returns {bin_sum, correct}. Does NOT throw on mismatch — unsafe is expected to fail.
// ─────────────────────────────────────────────────────────────────────────────
static HistResult check_histogram(cl::CommandQueue&           queue,
                                   cl::Buffer&                 buf_hist,
                                   int                         n_elements) {
    std::vector<int> gpu_hist(256);
    CL_CHECK(queue.enqueueReadBuffer(buf_hist, CL_TRUE, 0,
                                     256 * sizeof(int), gpu_hist.data()));
    int bin_sum = std::accumulate(gpu_hist.begin(), gpu_hist.end(), 0);
    return {bin_sum, bin_sum == n_elements};
}

// ─────────────────────────────────────────────────────────────────────────────
// print_table — structured output matching task spec format.
// ─────────────────────────────────────────────────────────────────────────────
struct VariantRow {
    std::string name;
    int         n_elements;
    double      kernel_ms;
    HistResult  result;
};

static void print_table(const std::vector<VariantRow>& rows, double global_ms) {
    std::cout << "\nVariant              Elements     Kernel (ms)     Speedup vs Global\n";
    std::cout << std::string(65, '-') << "\n";

    for (auto& r : rows) {
        double speedup = (global_ms > 0.0) ? global_ms / r.kernel_ms : 0.0;

        std::string tag;
        if (r.result.correct) {
            tag = "[PASS — bins sum to " + std::to_string(r.result.bin_sum) + "]";
        } else {
            tag = "[CORRUPT — bins sum to " + std::to_string(r.result.bin_sum)
                + " \u2260 " + std::to_string(r.n_elements) + "]";
        }

        std::cout << std::left  << std::setw(21) << r.name
                  << std::left  << std::setw(13) << r.n_elements
                  << std::fixed << std::setprecision(3)
                  << std::setw(16) << r.kernel_ms
                  << std::fixed << std::setprecision(2) << speedup << "x  "
                  << tag << "\n";
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// main
// ─────────────────────────────────────────────────────────────────────────────
int main(int argc, char* argv[]) {
    CLI::App app{"SyncAtomics — histogram throughput: unsafe vs global atomic vs local reduce"};

    int n_elements = 1'048'576;
    int local_size = 256;

    app.add_option("--size",       n_elements, "Number of uchar elements (default 1048576)");
    app.add_option("--local-size", local_size, "Local work-group size for local_reduce variant (default 256)");

    CLI11_PARSE(app, argc, argv);

    if (n_elements <= 0) {
        throw std::runtime_error("--size must be positive");
    }
    if (local_size <= 0) {
        throw std::runtime_error("--local-size must be positive");
    }

    try {
        // ── Context setup ─────────────────────────────────────────────────
        OclContext ocl = create_context();

        // WHY CL_QUEUE_PROFILING_ENABLE: required for cl::Event timestamp
        // queries (CL_PROFILING_COMMAND_START/END). Without this flag the
        // profiling values are undefined.
        ocl.queue = cl::CommandQueue(ocl.context, ocl.device,
                                     CL_QUEUE_PROFILING_ENABLE);

        // ── Generate random uchar data ────────────────────────────────────
        std::vector<cl_uchar> host_data(static_cast<size_t>(n_elements));
        std::mt19937 rng(42);
        std::uniform_int_distribution<int> dist(0, 255);
        for (auto& v : host_data) {
            v = static_cast<cl_uchar>(dist(rng));
        }

        // ── Buffers ───────────────────────────────────────────────────────
        cl::Buffer buf_data(ocl.context,
                            CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                            static_cast<size_t>(n_elements) * sizeof(cl_uchar),
                            host_data.data());

        cl::Buffer buf_hist(ocl.context,
                            CL_MEM_READ_WRITE,
                            256 * sizeof(int));

        // ── Kernel build ──────────────────────────────────────────────────
        std::string source = load_kernel_source("kernels/histogram_kernel.cl");
        cl::Program program(ocl.context, source);
        try {
            program.build({ocl.device});
        } catch (const cl::Error&) {
            std::string log = program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(ocl.device);
            throw std::runtime_error("Kernel build failed:\n" + log);
        }

        cl::Kernel k_unsafe(program,        "histogram_unsafe");
        cl::Kernel k_global(program,        "histogram_global_atomic");
        cl::Kernel k_local(program,         "histogram_local");

        // ── Run variants ──────────────────────────────────────────────────
        std::cout << "\n=== SyncAtomics: " << n_elements << "-element histogram (256 bins) ===\n";

        double ms_unsafe = run_histogram_variant(
            ocl.queue, k_unsafe, buf_data, buf_hist, n_elements, local_size, false);
        HistResult res_unsafe = check_histogram(ocl.queue, buf_hist, n_elements);

        double ms_global = run_histogram_variant(
            ocl.queue, k_global, buf_data, buf_hist, n_elements, local_size, false);
        HistResult res_global = check_histogram(ocl.queue, buf_hist, n_elements);

        double ms_local = run_histogram_variant(
            ocl.queue, k_local, buf_data, buf_hist, n_elements, local_size, true);
        HistResult res_local = check_histogram(ocl.queue, buf_hist, n_elements);

        // ── Print table ───────────────────────────────────────────────────
        std::vector<VariantRow> rows = {
            {"unsafe",        n_elements, ms_unsafe, res_unsafe},
            {"global_atomic", n_elements, ms_global, res_global},
            {"local_reduce",  n_elements, ms_local,  res_local},
        };
        print_table(rows, ms_global);

        double speedup_local = (ms_global > 0.0) ? ms_global / ms_local : 0.0;
        std::cout << "\nSpeedup (local_reduce vs global_atomic): "
                  << std::fixed << std::setprecision(2) << speedup_local << "x\n";

        // ── Hard assertion: atomic variants must be correct ───────────────
        // Unsafe failure is expected and must NOT trigger this assertion.
        if (!res_global.correct || !res_local.correct) {
            throw std::runtime_error(
                "Atomic variant produced incorrect histogram — driver bug or kernel error");
        }

    } catch (const cl::Error& e) {
        std::cerr << "[CL ERROR] " << e.what() << " (code " << e.err() << ")\n";
        throw;
    } catch (const std::exception& e) {
        std::cerr << "[ERROR] " << e.what() << "\n";
        throw;
    }

    return 0;
}
