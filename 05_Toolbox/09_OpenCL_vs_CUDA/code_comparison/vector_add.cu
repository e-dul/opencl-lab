// vector_add.cu — CUDA C++ vector addition (CUDA 11+, host + device)
//
// WHY this file exists: a structural mirror of vector_add.cl so the two APIs
// can be compared line-by-line.  See report.md §Side-by-Side Code Reference.

#include <stdio.h>
#include <stdlib.h>
#include <cuda_runtime.h>

// ---------------------------------------------------------------------------
// Device code (runs on GPU)
// WHY __global__: marks a function as a GPU entry point callable from the host.
// Equivalent to OpenCL's __kernel qualifier.
// ---------------------------------------------------------------------------
__global__ void vector_add(const float* a, const float* b, float* c, int n)
{
    // WHY blockIdx/blockDim/threadIdx: CUDA exposes the 3-D thread hierarchy
    // explicitly.  OpenCL hides this behind get_global_id(0), which computes
    // the same flat index under the hood.
    int gid = blockIdx.x * blockDim.x + threadIdx.x;

    // WHY explicit guard: same reason as the OpenCL version — the grid is
    // padded to a multiple of blockDim, so trailing threads may be OOB.
    if (gid < n) {
        c[gid] = a[gid] + b[gid];
    }
}

// ---------------------------------------------------------------------------
// Host code (runs on CPU)
// ---------------------------------------------------------------------------
int main()
{
    const int N     = 1 << 20;          // 1 M floats (~4 MB per buffer)
    const size_t SZ = (size_t)N * sizeof(float);

    // --- Allocate host memory ---
    float* h_a = (float*)malloc(SZ);
    float* h_b = (float*)malloc(SZ);
    float* h_c = (float*)malloc(SZ);

    for (int i = 0; i < N; ++i) { h_a[i] = (float)i; h_b[i] = 1.0f; }

    // --- Allocate device memory ---
    // WHY cudaMalloc: allocates VRAM.  No explicit "context" creation needed —
    // CUDA implicitly initialises on first API call.  OpenCL requires the host
    // to manually create a cl_context before any cl_mem allocation.
    float *d_a, *d_b, *d_c;
    cudaMalloc(&d_a, SZ);
    cudaMalloc(&d_b, SZ);
    cudaMalloc(&d_c, SZ);

    // --- Copy host → device ---
    // WHY cudaMemcpy: explicit staged transfer, same mental model as
    // clEnqueueWriteBuffer.  Both are blocking by default unless you use
    // the async variants (cudaMemcpyAsync / enqueueWriteBuffer with an event).
    cudaMemcpy(d_a, h_a, SZ, cudaMemcpyHostToDevice);
    cudaMemcpy(d_b, h_b, SZ, cudaMemcpyHostToDevice);

    // --- Launch kernel ---
    // WHY <<<grid, block>>>: CUDA's proprietary triple-angle-bracket extension.
    // There is no equivalent syntax in standard C++ or OpenCL.  OpenCL uses
    // clEnqueueNDRangeKernel() (a plain function call) instead.
    const int BLOCK = 256;
    const int GRID  = (N + BLOCK - 1) / BLOCK;
    vector_add<<<GRID, BLOCK>>>(d_a, d_b, d_c, N);

    // WHY cudaDeviceSynchronize: the kernel launch is asynchronous.  This
    // blocks until the GPU is idle.  OpenCL equivalent: clFinish(queue).
    cudaDeviceSynchronize();

    // --- Copy device → host ---
    cudaMemcpy(h_c, d_c, SZ, cudaMemcpyDeviceToHost);

    // --- Verify first element ---
    printf("c[0] = %.1f  (expected 1.0)\n", h_c[0]);

    // --- Free resources ---
    // WHY cudaFree: mirrors free() for device memory.
    // OpenCL equivalent: clReleaseMemObject() (or cl::Buffer destructor via RAII).
    cudaFree(d_a); cudaFree(d_b); cudaFree(d_c);
    free(h_a); free(h_b); free(h_c);

    return 0;
}
