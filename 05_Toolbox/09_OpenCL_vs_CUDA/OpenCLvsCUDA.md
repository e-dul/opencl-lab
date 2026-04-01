# OpenCL vs CUDA: Honest Market Analysis

**When to use**: you're starting a new project and need to choose a GPU compute API.

> **Requires:** NVIDIA GPU with CUDA toolkit installed (for the CUDA side of the comparison). The OpenCL path runs on any supported GPU; only the CUDA build requires NVIDIA hardware.

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
ls -lh report.md
ls -lh code_comparison/
```

## Ecosystem Comparison

For a full side-by-side comparison of CUDA vs OpenCL advantages (ecosystem, hardware targets, tooling, portability) see [report.md §1 — Ecosystem Comparison](report.md#1-ecosystem-comparison), which covers all the same axes in detail.

## The Pragmatic Decision

If you control the deployment environment and target Nvidia, use CUDA. If you need to run on multiple vendors, embedded hardware, or FPGAs — or if you're shipping software to customers with unknown hardware — use OpenCL.

## Mini-Challenge

Read `code_comparison/vector_add.cu` and `vector_add.cl`. Identify three structural differences in memory management between CUDA and OpenCL. Which API requires more explicit resource lifecycle management, and why?

---

[Back to Toolbox.md](../Toolbox.md)
