# CLBlast Matrix Multiplication

**When to use**: you need GPU-accelerated dense linear algebra and don't want to spend weeks writing and tuning a GEMM kernel. This module also builds the matrix-throughput intuition you need before the Ray Tracer modules.

> **Key terms**: GEMM (General Matrix Multiplication) is the operation `C = α·A·B + β·C` for dense matrices. For a square N×N multiply (α=1, β=0), the operation count is **2N³ FLOPs** (each output element needs N multiply-add pairs = 2 FLOPs each → 2N³ total). GFLOPS (10⁹ FLOPs/s) is the standard throughput unit for dense linear algebra benchmarks.

## Goals

After completing this module you will be able to:

- Explain what BLAS GEMM is and why GPU throughput is measured in GFLOPS.
- Integrate CLBlast as a drop-in OpenCL BLAS library via CMake FetchContent.
- Benchmark a naive GEMM kernel against CLBlast and interpret the speedup.
- Apply the rule: reach for a library when the algorithm is well-studied; write custom kernels when the access pattern is non-standard.

## Prerequisites

See [Bonus.md](../Bonus.md) for base requirements (OpenCL 1.2+, CMake 3.18+).

- Module 1 (`01_Host_API/`) complete — no further prerequisites.
- **CLBlast**: fetched automatically by CMake at configure time (requires internet). Offline: `-DCMAKE_PREFIX_PATH=/path/to/clblast/install`.

## Build & Run

```bash
cd 06_Bonus/01_CLBlast_MatMul
cmake -B build && cmake --build build
./build/clblast_matmul
# Override matrix size:
./build/clblast_matmul --size 2048
# Select GPU vendor:
GPU=NVIDIA ./build/clblast_matmul --size 1024
```

`--size N` sets the dimension for an N×N square GEMM (default: 1024).

## Expected Output

Console prints a structured timing table — no BMP is produced (this is a benchmark module):

```text
Matrix size : 1024 x 1024
┌──────────────────┬────────────┬──────────────┐
│ Implementation   │ Time (ms)  │ GFLOPS       │
├──────────────────┼────────────┼──────────────┤
│ Naive GEMM       │    184.200 │       11.700 │
│ CLBlast SGEMM    │     18.600 │      115.400 │
├──────────────────┼────────────┼──────────────┤
│ Speedup          │            │         9.80x│
└──────────────────┴────────────┴──────────────┘
```

CLBlast should outperform the naive kernel by at least 5× on any modern GPU. Exact numbers are device-specific — CLBlast selects tuning tables per device at runtime.

## Key Concepts

### Why CLBlast Wins

A naive GEMM kernel reads matrix B in column-major order — every thread in a warp accesses a different cache line. CLBlast uses:

- Tiled shared-memory loading to amortise global memory latency.
- Auto-tuned work-group sizes determined per device at first use.

Reproducing this in a custom kernel requires hundreds of lines of tile-loading code plus per-device benchmarking. The rule: use CLBlast (or any BLAS library) for standard dense linear algebra; write custom kernels for non-standard access patterns (image filtering, ray traversal, graph algorithms).

### CLBlast vs cuBLAS

CLBlast is vendor-agnostic — it targets any OpenCL device (AMD, Intel, Nvidia, embedded). cuBLAS is Nvidia-only and ships with the CUDA SDK. If you are writing cross-vendor code or targeting non-Nvidia hardware, CLBlast is the right choice.

### Mini-Challenge

Reduce `--size` to 64 and re-run. Which implementation wins at small sizes, and why? (Hint: kernel launch overhead dominates when the working set fits in L1.)

---

[← Bonus Modules](../Bonus.md)
