// B1 — CLBlast MatMul Benchmark
// Compares a naive OpenCL GEMM kernel against CLBlast Gemm<float> on the same
// square matrix, then prints a structured timing table.
// WHY structured table: master_specs §3 designates the console timing table
// as the sole artifact for purely numeric benchmarks (no BMP required).

#include "opencl_utils.hpp"   // CL_CHECK, load_kernel_source, duration_ms
#include "ocl_wrapper.hpp"    // create_context(), OclContext

#include <clblast.h>

#include <CLI/CLI.hpp>

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Extract elapsed nanoseconds from a raw cl_event (CLBlast returns cl_event,
// not cl::Event, so we query the C API directly).
// ---------------------------------------------------------------------------
static double event_duration_ms(cl_event ev) {
    cl_ulong t_start = 0, t_end = 0;
    CL_CHECK(clGetEventProfilingInfo(ev, CL_PROFILING_COMMAND_START, sizeof(t_start), &t_start, nullptr));
    CL_CHECK(clGetEventProfilingInfo(ev, CL_PROFILING_COMMAND_END,   sizeof(t_end),   &t_end,   nullptr));
    // WHY / 1e6: profiling timestamps are nanoseconds; convert to milliseconds.
    return static_cast<double>(t_end - t_start) / 1e6;
}

// ---------------------------------------------------------------------------
// GFLOPS for N×N GEMM: 2*N^3 multiply-add operations.
// WHY 2*N^3: each of N*N output elements needs N multiplications + N additions.
// ---------------------------------------------------------------------------
static double gflops(int N, double time_ms) {
    return 2.0 * static_cast<double>(N) * N * N / (time_ms * 1e-3) / 1e9;
}

// ---------------------------------------------------------------------------
// Print the timing table to stdout.
// ---------------------------------------------------------------------------
static void print_table(int N, double naive_ms, double clblast_ms) {
    const double naive_gf   = gflops(N, naive_ms);
    const double clblast_gf = gflops(N, clblast_ms);
    const double speedup    = naive_ms / clblast_ms;

    std::cout << "\nMatrix size : " << N << " x " << N << "\n";
    std::cout << u8"\u250c\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u252c\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u252c\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2510\n";
    std::cout << u8"\u2502 Implementation   \u2502 Time (ms)  \u2502 GFLOPS       \u2502\n";
    std::cout << u8"\u251c\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u253c\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u253c\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2524\n";

    std::cout << std::fixed;
    std::cout << u8"\u2502 Naive GEMM       \u2502 "
              << std::setw(10) << std::setprecision(3) << naive_ms
              << u8" \u2502 "
              << std::setw(12) << std::setprecision(3) << naive_gf
              << u8" \u2502\n";
    std::cout << u8"\u2502 CLBlast SGEMM    \u2502 "
              << std::setw(10) << std::setprecision(3) << clblast_ms
              << u8" \u2502 "
              << std::setw(12) << std::setprecision(3) << clblast_gf
              << u8" \u2502\n";
    std::cout << u8"\u251c\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u253c\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u253c\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2524\n";
    std::cout << u8"\u2502 Speedup          \u2502            \u2502 "
              << std::setw(8) << std::setprecision(2) << speedup
              << u8"x     \u2502\n";
    std::cout << u8"\u2514\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2534\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2534\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2518\n";
}

int main(int argc, char* argv[]) {
    // ── CLI ─────────────────────────────────────────────────────────────────
    CLI::App app{"CLBlast vs Naive GEMM Benchmark"};
    int size = 1024;
    app.add_option("--size", size, "Matrix dimension N for N x N GEMM")->default_val(1024);
    CLI11_PARSE(app, argc, argv);

    if (size <= 0) {
        throw std::runtime_error("--size must be a positive integer");
    }
    const int N = size;

    // ── Device setup ────────────────────────────────────────────────────────
    OclContext ocl = create_context();

    // WHY CL_QUEUE_PROFILING_ENABLE: cl::Event timestamps require profiling
    // to be enabled at queue creation; enabling it later is not possible.
    cl_int queue_err = CL_SUCCESS;
    cl::CommandQueue queue(ocl.context, ocl.device,
                           CL_QUEUE_PROFILING_ENABLE, &queue_err);
    CL_CHECK(queue_err);

    // ── Host data ───────────────────────────────────────────────────────────
    // WHY size_t for elem_count: N*N can exceed INT_MAX for large N; promote
    // before multiplying to avoid signed overflow (master_specs §7.1).
    const size_t elem_count = static_cast<size_t>(N) * N;
    std::vector<float> h_A(elem_count);
    std::vector<float> h_B(elem_count);
    std::vector<float> h_C(elem_count, 0.0f);

    std::mt19937 rng(42);
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
    std::generate(h_A.begin(), h_A.end(), [&]{ return dist(rng); });
    std::generate(h_B.begin(), h_B.end(), [&]{ return dist(rng); });

    const size_t byte_size = elem_count * sizeof(float);

    // ── Device buffers ──────────────────────────────────────────────────────
    // WHY CL_MEM_COPY_HOST_PTR | CL_MEM_READ_ONLY: single allocation + upload
    // in one call, and kernels never write to A or B.
    cl::Buffer d_A(ocl.context, CL_MEM_READ_ONLY  | CL_MEM_COPY_HOST_PTR, byte_size, h_A.data());
    cl::Buffer d_B(ocl.context, CL_MEM_READ_ONLY  | CL_MEM_COPY_HOST_PTR, byte_size, h_B.data());
    cl::Buffer d_C(ocl.context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, byte_size, h_C.data());

    // ── Naive kernel ────────────────────────────────────────────────────────
    const std::string kernel_src = load_kernel_source("kernels/naive_gemm.cl");
    cl::Program::Sources sources{kernel_src};
    cl::Program program(ocl.context, sources);

    try {
        program.build({ocl.device});
    } catch (const cl::Error&) {
        const std::string log = program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(ocl.device);
        throw std::runtime_error("Kernel build failed:\n" + log);
    }

    cl::Kernel naive_kernel(program, "naive_gemm");
    CL_CHECK(naive_kernel.setArg(0, d_A));
    CL_CHECK(naive_kernel.setArg(1, d_B));
    CL_CHECK(naive_kernel.setArg(2, d_C));
    CL_CHECK(naive_kernel.setArg(3, N));

    // WHY 16×16 work-group: common balanced baseline for 2-D GEMM kernels;
    // fits within the minimum required max work-group size (256) on all
    // OpenCL 1.2 devices.
    const size_t LOCAL  = 16;
    const size_t GLOBAL = ((static_cast<size_t>(N) + LOCAL - 1) / LOCAL) * LOCAL;

    cl::Event naive_event;
    CL_CHECK(queue.enqueueNDRangeKernel(
        naive_kernel,
        cl::NullRange,
        cl::NDRange(GLOBAL, GLOBAL),
        cl::NDRange(LOCAL, LOCAL),
        nullptr,
        &naive_event
    ));
    CL_CHECK(queue.finish());

    const double naive_ms = duration_ms(naive_event);

    // ── CLBlast Gemm<float> ──────────────────────────────────────────────────
    // Reset C to zeros before the CLBlast run so both paths start equivalently.
    CL_CHECK(queue.enqueueWriteBuffer(d_C, CL_TRUE, 0, byte_size, h_C.data()));

    cl_event clblast_raw_event = nullptr;

    // WHY alpha=1, beta=0: standard C = A*B with no scaling or accumulation.
    // WHY Gemm<float>: CLBlast C++ API is templated; Sgemm exists only in the
    // C binding (clblast_c.h), not the C++ header (clblast.h).
    cl_command_queue raw_queue = queue();  // unwrap once to avoid dangling ref
    clblast::StatusCode status = clblast::Gemm<float>(
        clblast::Layout::kRowMajor,
        clblast::Transpose::kNo,
        clblast::Transpose::kNo,
        static_cast<size_t>(N),   // M
        static_cast<size_t>(N),   // N
        static_cast<size_t>(N),   // K
        1.0f,                     // alpha
        d_A(),                    // cl_mem: raw handle from cl::Buffer
        0,                        // a_offset
        static_cast<size_t>(N),   // a_ld
        d_B(),
        0,                        // b_offset
        static_cast<size_t>(N),   // b_ld
        0.0f,                     // beta
        d_C(),
        0,                        // c_offset
        static_cast<size_t>(N),   // c_ld
        &raw_queue,
        &clblast_raw_event
    );

    if (status != clblast::StatusCode::kSuccess) {
        throw std::runtime_error(
            "CLBlast Gemm<float> failed with status " +
            std::to_string(static_cast<int>(status)));
    }

    // WHY clWaitForEvents: CLBlast enqueues asynchronously; profiling
    // timestamps are only valid after the event reaches CL_COMPLETE.
    CL_CHECK(clWaitForEvents(1, &clblast_raw_event));

    const double clblast_ms = event_duration_ms(clblast_raw_event);
    // WHY manual release: clblast_raw_event is a raw cl_event (C API),
    // not managed by a cl::Event RAII wrapper.
    CL_CHECK(clReleaseEvent(clblast_raw_event));

    // ── Print results ────────────────────────────────────────────────────────
    print_table(N, naive_ms, clblast_ms);

    return 0;
}
