# Optimization Toolbox

A library of isolated GPU optimization techniques. Do not read this front-to-back. Come here when your profiler shows a specific bottleneck, find the relevant tool, run it, then return to your project.

## Prerequisites
- Module 1 complete (`01_Host_API/`): `cl.hpp` usage, `cl::Event` profiling, `CL_CHECK`.
- See [main README](../README.md) for base requirements (OpenCL 1.2+, CMake 3.18+).
- Debugging tool: `sudo apt install oclgrind` (optional at build time; required at runtime).
- SVM tool: OpenCL 2.0+ device (AMD APU, Intel iGPU, ARM Mali) — falls back gracefully on 1.2.
- 08_Multi_GPU_Strategy: two OpenCL-capable GPUs on the same system — optional; single-GPU baseline always runs.

## How to Use This Toolbox

Each tool is a standalone project with a measurable before/after. The workflow:

1. Profile your Track project (`02_Multimedia/`, `03_GraphicsHPC/`, or `04_Robotics/`) (`cl::Event` timing on each stage)
2. Identify the bottleneck (upload? kernel? download?)
3. Find the matching tool below
4. Run the tool's demo — see the technique in isolation
5. Apply the technique to your project

## Contents

| Tool | Symptom | Folder |
|:-----|:--------|:-------|
| [Zero-Copy](15_Zero_Copy/ZeroCopy.md) | Upload time dominates frame budget | `15_Zero_Copy/` |
| [Coalesced Access](02_Coalesced_Access/CoalescedAccess.md) | Kernel slow despite simple logic | `02_Coalesced_Access/` |
| [Work-Group Sizing](14_Work_Group_Sizing/WorkGroupSizing.md) | GPU underutilized, low occupancy | `14_Work_Group_Sizing/` |
| [Thread Divergence](13_Thread_Divergence/ThreadDivergence.md) | Kernel slower than expected with conditionals | `13_Thread_Divergence/` |
| [Local Memory](01_Local_Memory/LocalMemory.md) | Kernel re-reads same global data repeatedly | `01_Local_Memory/` |
| [Debugging](03_Debugging/Debugging.md) | Crash / wrong output / silent slowdown | `03_Debugging/` |
| [Kernel Templates](06_Generic_Kernel_Templates/GenericKernelTemplates.md) | Duplicate `.cl` files for each data type | `06_Generic_Kernel_Templates/` |
| [Async Pipelines](16_Async_Multi_Thread/AsyncMultiThread.md) | CPU blocks on GPU between stages | `16_Async_Multi_Thread/` |
| [Multi-GPU](08_Multi_GPU_Strategy/MultiGPUStrategy.md) | Single GPU throughput ceiling reached | `08_Multi_GPU_Strategy/` |
| [Fast Math](05_Fast_Math/FastMath.md) | `sqrt`/`rsqrt`/`sin` calls dominating compute-bound kernel | `05_Fast_Math/` |
| [Sync & Atomics](12_Sync_Atomics/SyncAtomics.md) | Incorrect results with concurrent writes (histograms, counters, reductions) | `12_Sync_Atomics/` |
| [Global Work Offset](07_Global_Work_Offset/GlobalWorkOffset.md) | Full-frame dispatch wastes threads when only a small ROI needs work | `07_Global_Work_Offset/` |
| [OpenCL vs CUDA](09_OpenCL_vs_CUDA/OpenCLvsCUDA.md) | Choosing a GPU compute API for a new project *(NVIDIA GPU + CUDA)* | `09_OpenCL_vs_CUDA/` |
| [SVM Theory](11_SVM_Theory/SVMTheory.md) | Zero-Copy wasn't enough — need the hardware-level theory behind SVM *(OpenCL 2.0)* | `11_SVM_Theory/` |

## What's Next

Return to your track project with the technique applied. If you have exhausted all bottlenecks in your track, continue to [Module 6 Bonus](../06_Bonus/Bonus.md).
