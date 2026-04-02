# Work-Group Sizing & Occupancy

**Symptom**: GPU utilization < 60% in profiler. Kernel time improves significantly when you change `local_work_size` experimentally.

## Prerequisites
Prerequisites: OpenCL 1.2+, CMake 3.18+, `clinfo` installed. See [main README](../../README.md) for base requirements.

## Build & Run
```bash
cd 05_Toolbox/14_Work_Group_Sizing
cmake -B build && cmake --build build
./build/work_group_sizing --kernel mad --width 1920 --height 1080
```

## Verify
```
local_work_size=  8: 12.4 ms  occupancy: 12.5%
local_work_size= 32:  6.8 ms  occupancy: 50.0%
local_work_size= 64:  3.1 ms  occupancy: 87.5%  ← sweet spot
local_work_size=128:  3.3 ms  occupancy: 75.0%  (register spill)
local_work_size=256:  3.9 ms  occupancy: 50.0%  (LDS pressure)
```

Run the sweep first. Identify which `local_work_size` is fastest on your device before reading the explanation below.

## Concept

Occupancy is the ratio of active warps to the maximum possible warps on a compute unit. Low occupancy means the GPU stalls waiting for memory — nothing else to run. High occupancy hides latency by switching to a ready warp.

**The constraints** (all three must fit simultaneously per compute unit):
1. **Work-items per group** — hardware maximum (typically 256–1024)
2. **Registers per work-item** — total register file shared across all active groups
3. **Local memory per group** — total LDS shared across all active groups

Increasing `local_work_size` beyond the register/LDS limits reduces the number of active groups, killing occupancy. The tool automates this sweep.

**Rule of thumb**: Start with `local_work_size = 64`. Profile. If fast, try 128. If no improvement, 64 is your ceiling. Autotuning (sweeping at startup) is cleaner than hard-coding.

## Mini-Challenge

Run the sweep with `--kernel mad` but vary `--width` and `--height` to change the total work-item count. Observe how the optimal `local_work_size` shifts as the workload grows. Explain why occupancy improves at larger dimensions.

## Troubleshooting

- **All sizes show the same time**: The kernel may be compute-bound rather than latency-bound. Occupancy tuning only helps memory-latency-bound kernels.
- **`local_work_size` must divide `global_work_size`**: Pad your image dimensions to a multiple of the largest work-group size you plan to test.

## Used In
- [Track B — 02_Ray_Tracer_BVH](../../03_GraphicsHPC/02_Ray_Tracer_BVH/RayTracerBVH.md) (traversal kernel)
- [Track C — 02_Costmap_Inflation](../../04_Robotics/02_Costmap_Inflation/CostmapInflation.md)

---

## Advanced Challenge: Register Pressure & Spilling

Occupancy is controlled by three resource limits (work-items, registers, LDS). The previous sections cover work-item count and LDS. Register pressure is the third — and the hardest to see without vendor tools.

**The register file budget:**

Each compute unit has a fixed register file. On Nvidia Ampere (e.g. RTX 3080), that is **65536 32-bit registers per SM**. Those registers are shared across all concurrently resident threads. The occupancy formula:

```text
max_concurrent_warps = register_file_size / (registers_per_thread × warp_size)
                     = 65536            / (registers_per_thread × 32)
```

Example: a kernel using 32 registers per thread → `65536 / (32 × 32)` = **64 concurrent warps** per SM (100% occupancy on Ampere). A kernel using 128 registers per thread → only **16 concurrent warps** (25% occupancy) — the GPU stalls waiting for memory with almost nothing to hide latency.

**A kernel with excessive register pressure:**

```cl
__kernel void register_hungry(__global float* out, int n) {
    size_t gid = get_global_id(0);
    if (gid >= (size_t)n) return;

    // 16 private variables — forces the compiler to allocate registers for each.
    // On register-constrained devices, some will be spilled to L1/VRAM (slow).
    float a0 = out[gid * 16 + 0],  a1 = out[gid * 16 + 1];
    float a2 = out[gid * 16 + 2],  a3 = out[gid * 16 + 3];
    float a4 = out[gid * 16 + 4],  a5 = out[gid * 16 + 5];
    float a6 = out[gid * 16 + 6],  a7 = out[gid * 16 + 7];
    float a8 = out[gid * 16 + 8],  a9 = out[gid * 16 + 9];
    float a10 = out[gid * 16 + 10], a11 = out[gid * 16 + 11];
    float a12 = out[gid * 16 + 12], a13 = out[gid * 16 + 13];
    float a14 = out[gid * 16 + 14], a15 = out[gid * 16 + 15];

    // Compiler cannot eliminate these — all values used in output.
    out[gid] = a0+a1+a2+a3+a4+a5+a6+a7+a8+a9+a10+a11+a12+a13+a14+a15;
}
```

**How to observe register and spill counts by vendor:**

*Nvidia (`ptxas` verbose output):*

```bash
# Pass -cl-nv-verbose to the OpenCL compiler at clBuildProgram time.
# The driver pipes ptxas output to stderr:
#   ptxas info: Used 48 registers, 0 bytes lmem, 0 bytes smem, ...
clBuildProgram(..., "-cl-nv-verbose", ...)
```

*AMD (ROCm):*

```bash
# Pass additional build flags via AMD_OCL_BUILD_OPTIONS_APPEND at runtime.
# -v enables verbose compiler output; look for "ScratchSize = N" in stderr
# (N > 0 means register spilling to scratch/VRAM — a direct occupancy killer).
AMD_OCL_BUILD_OPTIONS_APPEND="-v" ./build/work_group_sizing
```

*Intel (IGC — Integrated Graphics Compiler):*

```bash
# Set IGC_ShaderDumpEnable=1 to dump per-kernel stats to /tmp/IntelIGC/
IGC_ShaderDumpEnable=1 ./build/work_group_sizing
# Look for .asm files — GRF count and spill bytes are reported in the header comments.
```

**Reducing register pressure:**

- Break large kernels into a pipeline of smaller kernels (each uses fewer registers).
- Use `__attribute__((reqd_work_group_size(X,Y,Z)))` to hint the compiler about the expected work-group size — it can sometimes pack registers more efficiently.
- Move rarely-used temporaries into `__local` if their lifetime spans a barrier anyway.

---

[Back to Toolbox](../Toolbox.md)
