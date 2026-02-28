# Optimization Toolbox

A library of isolated GPU optimization techniques. Do not read this front-to-back. Come here when your profiler shows a specific bottleneck, find the relevant tool, run it, then return to your project.

## Prerequisites
See [main README](../README.md) for base requirements (OpenCL 1.2+, CMake 3.18+).

## How to Use This Toolbox

Each tool is a standalone project with a measurable before/after. The workflow:

1. Profile your Module 2 project (`cl::Event` timing on each stage)
2. Identify the bottleneck (upload? kernel? download?)
3. Find the matching tool below
4. Run the tool's demo — see the technique in isolation
5. Apply the technique to your project

## Contents

| Tool | Symptom | Folder |
|:-----|:--------|:-------|
| [Zero-Copy](ZeroCopy/ZeroCopy.md) | Upload time dominates frame budget | `ZeroCopy/` |
| [Coalesced Access](CoalescedAccess/CoalescedAccess.md) | Kernel slow despite simple logic | `CoalescedAccess/` |
| [SVM](SVM/SVM.md) | Repeated map/unmap overhead on UMA | `SVM/` |
| [Work-Group Sizing](WorkGroupSizing/WorkGroupSizing.md) | GPU underutilized, low occupancy | `WorkGroupSizing/` |
| [Thread Divergence](ThreadDivergence/ThreadDivergence.md) | Kernel slower than expected with conditionals | `ThreadDivergence/` |
| [Local Memory](LocalMemory/LocalMemory.md) | Kernel re-reads same global data repeatedly | `LocalMemory/` |
| [Debugging](Debugging/Debugging.md) | Crash / wrong output / silent slowdown | `Debugging/` |
| [Kernel Templates](GenericKernelTemplates/GenericKernelTemplates.md) | Duplicate `.cl` files for each data type | `GenericKernelTemplates/` |
| [Async Pipelines](AsyncMultiThread/AsyncMultiThread.md) | CPU blocks on GPU between stages | `AsyncMultiThread/` |
| [Multi-GPU](MultiGPU_Strategy/MultiGPUStrategy.md) | Single GPU throughput ceiling reached | `MultiGPU_Strategy/` |
| [Fast Math](FastMath/FastMath.md) | `sqrt`/`rsqrt`/`sin` calls dominating compute-bound kernel | `FastMath/` |

## What's Next

Return to your Module 2 project with the technique applied. If you have exhausted all bottlenecks in your track, continue to [Module 4 Add-ons](../04_Addons/README.md).
