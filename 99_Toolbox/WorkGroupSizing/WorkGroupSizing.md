# Work-Group Sizing & Occupancy

**Symptom**: GPU utilization < 60% in profiler. Kernel time improves significantly when you change `local_work_size` experimentally.

## Prerequisites
Prerequisites: OpenCL 1.2+, CMake 3.18+, `clinfo` installed. See [main README](../../README.md) for base requirements.

## Build & Run
```bash
cd 99_Toolbox/WorkGroupSizing
cmake -B build && cmake --build build
./build/workgroup_sizing --kernel mad --width 1920 --height 1080
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
- [Track B — B3_Ray_Tracer_BVH](../../02_Projects/B_Graphics_HPC/GraphicsHPC.md) (traversal kernel)
- [Track C — C2_Costmap_Inflation](../../02_Projects/C_Robotics_ROS2/RoboticsROS2.md#c2_costmap_inflation--distance-transform-on-gpu)

---

[Back to Toolbox](../Toolbox.md)
