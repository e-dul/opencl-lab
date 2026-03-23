# 4.2 — OpenCL vs CUDA: Honest Market Analysis

**When to use**: you're starting a new project and need to choose a GPU compute API.

## Prerequisites
See [main README](../../README.md) for base requirements (OpenCL 1.2+, CMake 3.18+).

- Any Module 2 track.

## What's in This Add-on

This add-on is a written analysis — there is no binary to build.

- `report.md` — structured analysis: ecosystem comparison table, use-case decision matrix, pragmatic recommendation.
- `code_comparison/vector_add.cu` and `code_comparison/vector_add.cl` — side-by-side implementations of the same algorithm in both APIs, illustrating key structural differences in memory management and kernel dispatch.

## Verify

Confirm the written artifacts are present and non-empty:
```bash
ls -lh 04_Addons/4_2_OpenCL_vs_CUDA/report.md
ls -lh 04_Addons/4_2_OpenCL_vs_CUDA/code_comparison/
```

## Where CUDA Wins

| Advantage | Reason |
|:----------|:-------|
| AI/ML ecosystem | cuDNN, TensorRT, PyTorch, JAX are CUDA-native. No OpenCL equivalent. |
| Nvidia-specific hardware | Tensor Cores, NVLink, NVMe GPUDirect — CUDA-only. |
| Tooling maturity | Nsight, cuda-memcheck, and the Nvidia profiler are significantly more complete. |
| Community | Stack Overflow, papers, and GitHub repos overwhelmingly use CUDA. |

## Where OpenCL Wins

| Advantage | Reason |
|:----------|:-------|
| FPGA | Xilinx/Intel FPGAs use OpenCL as their primary compute API. |
| Mobile and embedded | ARM Mali, Qualcomm Adreno — all OpenCL, no CUDA. |
| Intel iGPU | Every laptop with an Intel chip has an OpenCL runtime. Zero extra drivers. |
| AMD on Linux | ROCm is the primary path; OpenCL is the stable portable layer on top. |
| Portable deployment | One codebase runs on Nvidia, AMD, Intel, and CPU fallback. |

> **Note:** The FPGA row may be outdated. Accurate for Intel FPGAs (Intel oneAPI still supports OpenCL for FPGAs). AMD/Xilinx shifted toward SYCL/HLS C++ as their primary compute path as of 2024 — OpenCL support remains but is no longer the recommended entry point. Verify against current Vitis HLS and oneAPI FPGA documentation before citing this as current best practice.

## The Pragmatic Decision

If you control the deployment environment and target Nvidia, use CUDA. If you need to run on multiple vendors, embedded hardware, or FPGAs — or if you're shipping software to customers with unknown hardware — use OpenCL.

## Mini-Challenge

Read `code_comparison/vector_add.cu` and `vector_add.cl`. Identify three structural differences in memory management between CUDA and OpenCL. Which API requires more explicit resource lifecycle management, and why?

---

[Back to Toolbox.md](../Toolbox.md)
