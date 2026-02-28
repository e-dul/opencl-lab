# 4.2 — OpenCL vs CUDA: Honest Market Analysis

**When to use**: you're starting a new project and need to choose a GPU compute API.

## Prerequisites
See [main README](../../README.md) for base requirements (OpenCL 1.2+, CMake 3.18+).

Any Module 2 track sufficient.

## Build & Run
```bash
cd 04_Addons/4_2_OpenCL_vs_CUDA
cmake -B build && cmake --build build
./build/portability_demo    # runs the same kernel on all detected OpenCL devices
```

## Verify
Console lists all detected platforms and reports kernel time per device:
```
[NVIDIA RTX 3080 ] MAD kernel (1080p): 0.31 ms
[Intel UHD 770   ] MAD kernel (1080p): 1.82 ms
[AMD RX 6700 XT  ] MAD kernel (1080p): 0.44 ms
[PoCL CPU        ] MAD kernel (1080p): 8.91 ms
```

One binary, four devices. Try doing that with CUDA.

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

## The Pragmatic Decision

If you control the deployment environment and target Nvidia, use CUDA. If you need to run on multiple vendors, embedded hardware, or FPGAs — or if you're shipping software to customers with unknown hardware — use OpenCL.

## Mini-Challenge

Run `portability_demo` on a machine with both a discrete GPU and an integrated GPU. Profile the same kernel on both. What is the iGPU/dGPU performance ratio? Does zero-copy (`CL_MEM_USE_HOST_PTR`) change the iGPU result?

---

[Back to Add-ons](../Addons.md)
