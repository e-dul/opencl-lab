// vector_add_host.cpp — OpenCL C host code (OpenCL 1.2, host + kernel load)
//
// WHY this file exists: a structural mirror of vector_add.cu so the two APIs
// can be compared line-by-line.  See report.md §Side-by-Side Code Reference.
//
// *** PROJECT CONVENTION OVERRIDE ***
// This project standardises on cl.hpp (C++ RAII bindings) and forbids raw
// clCreateBuffer / clReleaseMemObject in production module code (see
// .claude/rules/00_master_specs.md §1).  This file intentionally uses the raw
// OpenCL C API as an educational exception: the goal is a fair, style-matched
// comparison against vector_add.cu, which uses the raw CUDA C API.  Do NOT
// use this pattern in any other module.

#include <stdio.h>
#include <stdlib.h>
#include <fstream>
#include <sstream>
#include <string>

#ifdef __APPLE__
#  include <OpenCL/opencl.h>
#else
#  include <CL/cl.h>
#endif

// ---------------------------------------------------------------------------
// Helper: load kernel source from disk.
// WHY runtime compile: OpenCL compiles kernels at runtime on the active driver,
// enabling the same source to target any vendor.  CUDA compiles offline via
// nvcc; there is no equivalent vendor-neutral offline path.
// ---------------------------------------------------------------------------
static std::string load_source(const char* path)
{
    std::ifstream f(path);
    if (!f) { fprintf(stderr, "cannot open kernel: %s\n", path); exit(1); }
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

int main()
{
    const int    N  = 1 << 20;          // 1 M floats (~4 MB per buffer)
    const size_t SZ = (size_t)N * sizeof(float);

    // --- Allocate host memory ---
    float* h_a = (float*)malloc(SZ);
    float* h_b = (float*)malloc(SZ);
    float* h_c = (float*)malloc(SZ);

    for (int i = 0; i < N; ++i) { h_a[i] = (float)i; h_b[i] = 1.0f; }

    // --- Platform / device / context / queue ---
    // WHY explicit setup: OpenCL requires the host to enumerate platforms and
    // devices before any memory allocation.  CUDA skips this — the runtime
    // implicitly initialises on the first API call (cudaMalloc, etc.).
    cl_platform_id platform;
    cl_device_id   device;
    clGetPlatformIDs(1, &platform, NULL);
    clGetDeviceIDs(platform, CL_DEVICE_TYPE_GPU, 1, &device, NULL);

    // WHY cl_context: groups one or more devices that share memory objects.
    // CUDA has no explicit context object in the user API — the runtime manages
    // a per-device implicit context behind the scenes.
    cl_context       context = clCreateContext(NULL, 1, &device, NULL, NULL, NULL);

    // WHY CL_QUEUE_PROFILING_ENABLE: required to read CL_PROFILING_COMMAND_*
    // timestamps.  CUDA events (cudaEventRecord) need no equivalent queue flag.
    cl_command_queue queue   = clCreateCommandQueue(context, device,
                                                    CL_QUEUE_PROFILING_ENABLE, NULL);

    // --- Allocate device memory ---
    // WHY clCreateBuffer: allocates VRAM.  Equivalent to cudaMalloc, but always
    // requires an explicit context.  Released manually via clReleaseMemObject —
    // there is no automatic RAII in the raw OpenCL C API.
    cl_mem d_a = clCreateBuffer(context, CL_MEM_READ_ONLY,  SZ, NULL, NULL);
    cl_mem d_b = clCreateBuffer(context, CL_MEM_READ_ONLY,  SZ, NULL, NULL);
    cl_mem d_c = clCreateBuffer(context, CL_MEM_WRITE_ONLY, SZ, NULL, NULL);

    // --- Copy host → device ---
    // WHY clEnqueueWriteBuffer: explicit staged transfer, same mental model as
    // cudaMemcpy(HostToDevice).  Both are blocking here (CL_TRUE / no stream).
    clEnqueueWriteBuffer(queue, d_a, CL_TRUE, 0, SZ, h_a, 0, NULL, NULL);
    clEnqueueWriteBuffer(queue, d_b, CL_TRUE, 0, SZ, h_b, 0, NULL, NULL);

    // --- Build program from source ---
    // WHY runtime compilation: JIT compilation happens here, not at host binary
    // build time.  CUDA compiles offline via nvcc — this is the most visible
    // structural difference in host setup between the two APIs.
    std::string src      = load_source("vector_add.cl");
    const char* src_ptr  = src.c_str();
    size_t      src_len  = src.size();
    cl_program program = clCreateProgramWithSource(context, 1, &src_ptr, &src_len, NULL);
    cl_int build_err = clBuildProgram(program, 1, &device, NULL, NULL, NULL);
    if (build_err != CL_SUCCESS) {
        size_t log_len;
        clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, 0, NULL, &log_len);
        char* log = (char*)malloc(log_len);
        clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, log_len, log, NULL);
        fprintf(stderr, "build failed:\n%s\n", log);
        free(log); exit(1);
    }

    // --- Launch kernel ---
    // WHY clSetKernelArg + clEnqueueNDRangeKernel: there is no special syntax;
    // arguments are set via typed function calls.  CUDA's <<<grid, block>>>(args...)
    // extension bundles argument passing and launch into a single expression — a
    // deliberate ergonomic trade-off that requires the nvcc compiler.
    cl_kernel kernel = clCreateKernel(program, "vector_add", NULL);
    clSetKernelArg(kernel, 0, sizeof(cl_mem), &d_a);
    clSetKernelArg(kernel, 1, sizeof(cl_mem), &d_b);
    clSetKernelArg(kernel, 2, sizeof(cl_mem), &d_c);
    clSetKernelArg(kernel, 3, sizeof(cl_int), &N);

    const size_t LOCAL  = 256;
    const size_t GLOBAL = ((size_t)N + LOCAL - 1) / LOCAL * LOCAL;

    // WHY NDRange: OpenCL's equivalent of CUDA's grid/block dimensions.
    // A 1-D global size of GLOBAL with a local size of LOCAL maps directly to
    // CUDA's <<<GLOBAL/LOCAL, LOCAL>>> launch configuration.
    clEnqueueNDRangeKernel(queue, kernel, 1, NULL, &GLOBAL, &LOCAL, 0, NULL, NULL);

    // WHY clFinish: blocks until the GPU is idle.
    // CUDA equivalent: cudaDeviceSynchronize().
    clFinish(queue);

    // --- Copy device → host ---
    clEnqueueReadBuffer(queue, d_c, CL_TRUE, 0, SZ, h_c, 0, NULL, NULL);

    // --- Verify first element ---
    printf("c[0] = %.1f  (expected 1.0)\n", h_c[0]);

    // --- Free resources ---
    // WHY manual release: the raw OpenCL C API has no RAII.  Each object needs
    // an explicit clRelease* call — mirroring cudaFree for device memory.
    // The cl.hpp C++ bindings (used everywhere else in this project) automate
    // this via destructors; see the project convention override note at the top.
    clReleaseMemObject(d_a); clReleaseMemObject(d_b); clReleaseMemObject(d_c);
    clReleaseKernel(kernel);
    clReleaseProgram(program);
    clReleaseCommandQueue(queue);
    clReleaseContext(context);
    free(h_a); free(h_b); free(h_c);

    return 0;
}
