// Naive GEMM kernel: C = A * B (all square, row-major).
// Each work-item computes one output element C[row][col].
// WHY naive: no tiling, no local memory — gives a performance baseline
// that CLBlast's optimized SGEMM should dominate.
__kernel void naive_gemm(
    __global const float* A,
    __global const float* B,
    __global       float* C,
    const int N)
{
    // WHY size_t: get_global_id returns size_t; using int would require a cast.
    size_t col = get_global_id(0);
    size_t row = get_global_id(1);

    // Guard against over-dispatch when NDRange is not a multiple of the work-group size.
    if (col >= (size_t)N || row >= (size_t)N) return;

    float acc = 0.0f;
    for (int k = 0; k < N; ++k) {
        // Row-major indexing: A[row][k], B[k][col].
        acc += A[row * N + (size_t)k] * B[(size_t)k * N + col];
    }
    C[row * N + col] = acc;
}
