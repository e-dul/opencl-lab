// FastMath — demonstrates throughput gains from half_ and native_ math builtins
// by normalizing N float4 ray vectors with three kernel variants:
//   1. standard  — rsqrt  (IEEE-754 compliant)
//   2. half_     — half_rsqrt  (~11-bit mantissa; skipped if cl_khr_fp16 absent)
//   3. native_   — native_rsqrt (single HW instruction, implementation-defined precision)
//
// Timing is done exclusively via cl::Event profiling — wall-clock results are
// NOT reported to avoid inflating scores with host overhead.

#include "opencl_utils.hpp"   // CL_CHECK, load_kernel_source, duration_ms
#include "ocl_wrapper.hpp"    // create_context(), OclContext

#include <CLI/CLI.hpp>

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

// ──────────────────────────────────────────────────────────────────────────────
// build_program_variant — compile ray_kernel.cl with a variant-specific -D flag.
// Extra options (e.g. -cl-fast-relaxed-math) are appended when requested.
// Surfaces the compiler log on failure so kernel errors are easy to diagnose.
// ──────────────────────────────────────────────────────────────────────────────
static cl::Program build_program_variant(const cl::Context& ctx,
                                         const cl::Device&  dev,
                                         const std::string& source,
                                         const std::string& variant_define,
                                         const std::string& extra_opts) {
    std::string build_opts = variant_define + " " + extra_opts;
    cl::Program prog(ctx, source);
    try {
        prog.build({dev}, build_opts.c_str());
    } catch (const cl::Error&) {
        // WHY getBuildInfo here: the cl::Error message alone does not contain
        // the kernel compiler output; we need it to debug kernel syntax errors.
        std::string log = prog.getBuildInfo<CL_PROGRAM_BUILD_LOG>(dev);
        throw std::runtime_error("Kernel build failed (" + variant_define + "):\n" + log);
    }
    return prog;
}

// ──────────────────────────────────────────────────────────────────────────────
// run_variant — dispatch one kernel variant, return cl::Event kernel time (ms).
// ──────────────────────────────────────────────────────────────────────────────
static double run_variant(cl::CommandQueue& queue,
                          const cl::Program&      prog,
                          const char*             kernel_name,
                          const cl::Buffer&       buf_in,
                          cl::Buffer&             buf_out,
                          int                     n_rays) {
    cl::Kernel kernel(prog, kernel_name);

    CL_CHECK(kernel.setArg(0, buf_in));
    CL_CHECK(kernel.setArg(1, buf_out));
    CL_CHECK(kernel.setArg(2, n_rays));

    cl::Event ev;
    CL_CHECK(queue.enqueueNDRangeKernel(kernel, cl::NullRange,
                                        cl::NDRange(static_cast<size_t>(n_rays)),
                                        cl::NullRange, nullptr, &ev));
    CL_CHECK(queue.finish());
    return duration_ms(ev);
}

// ──────────────────────────────────────────────────────────────────────────────
// check_fp16_support — inspect CL_DEVICE_EXTENSIONS for cl_khr_fp16.
// half_ builtins are only safe to call if this extension is advertised.
// ──────────────────────────────────────────────────────────────────────────────
static bool check_fp16_support(const cl::Device& dev) {
    std::string exts = dev.getInfo<CL_DEVICE_EXTENSIONS>();
    return exts.find("cl_khr_fp16") != std::string::npos;
}

// ──────────────────────────────────────────────────────────────────────────────
// print_table — structured timing output matching task spec format.
// ──────────────────────────────────────────────────────────────────────────────
struct VariantResult {
    std::string name;
    int         n_rays;
    double      kernel_ms;
    bool        skipped{false};
};

static void print_table(const std::vector<VariantResult>& results) {
    // Find standard time for speedup calculation.
    double standard_ms = 0.0;
    for (auto& r : results) {
        if (r.name == "standard" && !r.skipped) {
            standard_ms = r.kernel_ms;
            break;
        }
    }

    std::cout << "\n";
    std::cout << std::left
              << std::setw(18) << "Variant"
              << std::setw(12) << "Rays"
              << std::setw(16) << "Kernel (ms)"
              << "Speedup vs Standard\n";
    std::cout << std::string(62, '-') << "\n";

    for (auto& r : results) {
        if (r.skipped) {
            std::cout << std::left << std::setw(18) << r.name
                      << "(skipped)\n";
            continue;
        }
        double speedup = (standard_ms > 0.0) ? standard_ms / r.kernel_ms : 0.0;
        std::cout << std::left  << std::setw(18) << r.name
                  << std::left  << std::setw(12) << r.n_rays
                  << std::fixed << std::setprecision(3)
                  << std::setw(16) << r.kernel_ms
                  << std::fixed << std::setprecision(2) << speedup << "x\n";
    }
    std::cout << "\n";
}

// ──────────────────────────────────────────────────────────────────────────────
// run_benchmark — compile all variants, dispatch, collect times.
// ──────────────────────────────────────────────────────────────────────────────
static std::vector<VariantResult> run_benchmark(OclContext&               ocl,
                                                const std::string&        kernel_source,
                                                const std::vector<float>& host_rays,
                                                int                       n_rays,
                                                bool                      has_fp16,
                                                const std::string&        extra_opts) {
    const size_t buf_bytes = static_cast<size_t>(n_rays) * 4 * sizeof(float);

    // Upload input once; share across all three variant dispatches.
    cl::Buffer buf_in(ocl.context,
                      CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                      buf_bytes,
                      // WHY const_cast: COPY_HOST_PTR copies immediately; the
                      // cl::Buffer constructor takes void* but does not mutate.
                      const_cast<float*>(host_rays.data()));

    // Output buffer reused per variant — no need to preserve between runs.
    // WHY reuse buf_out across all three variants: each kernel unconditionally
    // overwrites all n_rays elements, so no inter-variant state carry-over occurs.
    // Reusing avoids three separate allocations for data we never read mid-run.
    cl::Buffer buf_out(ocl.context, CL_MEM_WRITE_ONLY, buf_bytes);

    // ── Build standard variant ────────────────────────────────────────────
    cl::Program prog_standard = build_program_variant(
        ocl.context, ocl.device, kernel_source, "-D VARIANT=STANDARD", extra_opts);

    // ── Build half variant (only if fp16 extension is present) ───────────
    cl::Program prog_half;
    bool half_built = false;
    if (has_fp16) {
        prog_half = build_program_variant(
            ocl.context, ocl.device, kernel_source, "-D USE_HALF", extra_opts);
        half_built = true;
    }

    // ── Build native variant ──────────────────────────────────────────────
    cl::Program prog_native = build_program_variant(
        ocl.context, ocl.device, kernel_source, "-D VARIANT=NATIVE", extra_opts);

    // ── Dispatch & time ───────────────────────────────────────────────────
    std::vector<VariantResult> results;

    {
        double ms = run_variant(ocl.queue, prog_standard,
                                "ray_normalize_standard", buf_in, buf_out, n_rays);
        results.push_back({"standard", n_rays, ms});
    }

    if (half_built) {
        double ms = run_variant(ocl.queue, prog_half,
                                "ray_normalize_half", buf_in, buf_out, n_rays);
        results.push_back({"half_rsqrt", n_rays, ms});
    } else {
        results.push_back({"half_rsqrt", n_rays, 0.0, true});
    }

    {
        double ms = run_variant(ocl.queue, prog_native,
                                "ray_normalize_native", buf_in, buf_out, n_rays);
        results.push_back({"native_rsqrt", n_rays, ms});
    }

    return results;
}

// ──────────────────────────────────────────────────────────────────────────────
// correctness_check — read back standard output and compare to host reference.
// Only applied to the standard variant; native/half may diverge by design.
// ──────────────────────────────────────────────────────────────────────────────
static void correctness_check(OclContext&               ocl,
                               const std::string&        kernel_source,
                               const std::vector<float>& host_rays,
                               int                       n_rays) {
    const size_t buf_bytes = static_cast<size_t>(n_rays) * 4 * sizeof(float);

    cl::Buffer buf_in(ocl.context,
                      CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                      buf_bytes,
                      const_cast<float*>(host_rays.data()));
    cl::Buffer buf_out(ocl.context, CL_MEM_WRITE_ONLY, buf_bytes);

    cl::Program prog = build_program_variant(ocl.context, ocl.device,
                                             kernel_source, "-D VARIANT=STANDARD", "");
    cl::Kernel  kernel(prog, "ray_normalize_standard");

    CL_CHECK(kernel.setArg(0, buf_in));
    CL_CHECK(kernel.setArg(1, buf_out));
    CL_CHECK(kernel.setArg(2, n_rays));

    cl::Event ev;
    CL_CHECK(ocl.queue.enqueueNDRangeKernel(kernel, cl::NullRange,
                                            cl::NDRange(static_cast<size_t>(n_rays)),
                                            cl::NullRange, nullptr, &ev));
    CL_CHECK(ocl.queue.finish());

    std::vector<float> gpu_out(static_cast<size_t>(n_rays) * 4);
    CL_CHECK(ocl.queue.enqueueReadBuffer(buf_out, CL_TRUE, 0, buf_bytes, gpu_out.data()));

    // Host reference: normalize with sqrtf.
    float max_rel_err = 0.0f;
    for (int i = 0; i < n_rays; ++i) {
        size_t base  = static_cast<size_t>(i) * 4;
        float  rx    = host_rays[base + 0];
        float  ry    = host_rays[base + 1];
        float  rz    = host_rays[base + 2];
        float  len   = sqrtf(rx * rx + ry * ry + rz * rz);
        if (len < 1e-10f) continue;

        float ref_x = rx / len;
        float ref_y = ry / len;
        float ref_z = rz / len;

        float ex = fabsf(gpu_out[base + 0] - ref_x) / (fabsf(ref_x) + 1e-10f);
        float ey = fabsf(gpu_out[base + 1] - ref_y) / (fabsf(ref_y) + 1e-10f);
        float ez = fabsf(gpu_out[base + 2] - ref_z) / (fabsf(ref_z) + 1e-10f);
        float e  = std::max({ex, ey, ez});
        if (e > max_rel_err) max_rel_err = e;
    }

    std::cout << "Correctness (standard variant, max relative error vs host sqrtf): "
              << std::scientific << std::setprecision(3) << max_rel_err << "\n";
}

// ──────────────────────────────────────────────────────────────────────────────
// main
// ──────────────────────────────────────────────────────────────────────────────
int main(int argc, char* argv[]) {
    CLI::App app{"FastMath — ray normalization throughput: standard vs half_ vs native_"};

    int  n_rays  = 1'000'000;
    bool relaxed = false;

    app.add_option("--rays",    n_rays,  "Number of float4 ray vectors (default 1000000)");
    app.add_flag  ("--relaxed", relaxed, "Also run with -cl-fast-relaxed-math appended");

    CLI11_PARSE(app, argc, argv);

    try {
        // ── Context setup (respects GPU env var) ─────────────────────────
        OclContext ocl = create_context();

        // WHY CL_QUEUE_PROFILING_ENABLE: required for cl::Event timestamp
        // queries (CL_PROFILING_COMMAND_START/END). Without this flag the
        // values are undefined.
        ocl.queue = cl::CommandQueue(ocl.context, ocl.device,
                                     CL_QUEUE_PROFILING_ENABLE);

        // ── fp16 extension check ──────────────────────────────────────────
        bool has_fp16 = check_fp16_support(ocl.device);
        if (!has_fp16) {
            std::cout << "cl_khr_fp16 not available — skipping half_ variant\n";
        }

        // ── Generate random ray vectors ───────────────────────────────────
        // Unit-ish: components in [-1, 1]; intentionally not pre-normalized
        // so the kernel actually performs work.
        std::cout << "Generating " << n_rays << " random float4 rays...\n\n";
        std::mt19937                          rng(42);
        std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
        std::vector<float> host_rays(static_cast<size_t>(n_rays) * 4);
        for (size_t i = 0; i < host_rays.size(); i += 4) {
            host_rays[i + 0] = dist(rng);
            host_rays[i + 1] = dist(rng);
            host_rays[i + 2] = dist(rng);
            host_rays[i + 3] = 0.0f;  // w is unused
        }

        // ── Load kernel source ────────────────────────────────────────────
        std::string kernel_source = load_kernel_source("kernels/ray_kernel.cl");

        // ── Correctness check (standard variant only) ─────────────────────
        correctness_check(ocl, kernel_source, host_rays, n_rays);

        // ── Benchmark: no extra flags ─────────────────────────────────────
        std::cout << "\n=== Benchmark (no extra compiler flags) ===\n";
        auto results = run_benchmark(ocl, kernel_source, host_rays, n_rays,
                                     has_fp16, "");
        print_table(results);

        // ── Optional: benchmark with -cl-fast-relaxed-math ───────────────
        if (relaxed) {
            std::cout << "=== Benchmark (with -cl-fast-relaxed-math) ===\n";
            auto results_r = run_benchmark(ocl, kernel_source, host_rays, n_rays,
                                           has_fp16, "-cl-fast-relaxed-math");
            print_table(results_r);
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
