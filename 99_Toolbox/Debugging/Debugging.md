# Debugging & Profiling Tools

**Symptom**: Wrong output / silent crash / kernel gives different results on CPU vs GPU.

## Prerequisites
Prerequisites: OpenCL 1.2+, CMake 3.18+, `clinfo` installed. See [main README](../../README.md) for base requirements.

**Additional**:
- Oclgrind: `sudo apt install oclgrind`
- Nsight Systems (Nvidia): [developer.nvidia.com/nsight-systems](https://developer.nvidia.com/nsight-systems)
- VTune (Intel): `sudo apt install intel-oneapi-vtune`

## Build & Run
```bash
cd 99_Toolbox/Debugging
cmake -B build && cmake --build build

# Run kernel under Oclgrind (memory safety checker)
oclgrind ./build/debug_demo --test out_of_bounds
oclgrind --data-races --uniform-writes ./build/debug_demo --test race_condition
```

## Verify
```
[Oclgrind] ERROR: Invalid write of size 4
           at kernel debug_kernel (debug_kernel.cl:12)
           Work-item: (64, 0, 0)
           Address: 0x7f... (4 bytes past end of allocation)
```

Run both test modes. Confirm Oclgrind catches the injected bugs before reading the tool descriptions below.

## Oclgrind — Memory Safety

Oclgrind is a CPU-based OpenCL simulator that instruments every memory access. It catches:
- Out-of-bounds global/local memory reads and writes
- Work-item data races (concurrent writes to same address)
- Uninitialized memory reads

```bash
# Install
sudo apt install oclgrind

# Run any OpenCL binary under Oclgrind
oclgrind ./build/your_kernel_demo

# Detect write-write data races between work-items
oclgrind --data-races --uniform-writes --max-errors 10 ./build/your_kernel_demo
```

> `--data-races` detects work-item write conflicts: two work-items writing to the same
> address without synchronization. `--uniform-writes` suppresses false positives when all
> work-items write the same value to the same address intentionally.
> Use `--check-api` separately to validate the OpenCL API call sequence (invalid enqueue
> arguments, wrong buffer sizes on host calls) — it operates at the host API level and does
> not detect memory-level data races inside kernels.

Oclgrind runs on CPU — expect 10–50x slowdown. Use it for correctness, not performance.

## Nsight / VTune — Performance Profiling

For GPU timeline analysis (CPU–GPU overlap, kernel bottlenecks, memory bandwidth):

```bash
# Nvidia: Nsight Systems
nsys profile --trace=opencl,osrt ./build/your_demo
nsys-ui report1.nsys-rep   # open timeline

# Intel: VTune
vtune -collect gpu-hotspots -- ./build/your_demo
vtune-gui

# AMD: rocprof (OpenCL via ROCm)
rocprof --hsa-trace ./build/your_demo
```

Look for:
- **Gaps between kernels**: CPU is blocking between launches — use [Async Pipelines](../AsyncMultiThread/AsyncMultiThread.md)
- **Short kernels with long launch overhead**: batch or fuse kernels
- **Low memory bandwidth vs peak**: access pattern is uncoalesced — see [Coalesced Access](../CoalescedAccess/CoalescedAccess.md)

## Mini-Challenge

Add a deliberate off-by-one error to `debug_demo` (read `input[id + 1]` without bounds check). Run under Oclgrind and confirm it reports the exact work-item and address. Then fix the bug and confirm the report is clean.

## Troubleshooting

- **Oclgrind not found after install**: Run `oclgrind --version` to confirm. If `clinfo` shows Oclgrind as a platform, it is working.
- **Nsight profile shows no OpenCL events**: Pass `--trace=opencl` explicitly. Some versions default to CUDA only.
- **rocprof produces empty trace**: Ensure the binary was linked against the ROCm OpenCL runtime, not the Khronos ICD loader.

## Used In
- [Track A — A4_Smart_Webcam](../../02_Projects/A_Multimedia/Multimedia.md#a4_smart_webcam--flagship-project) (Oclgrind validation of background segmentation kernel)
- [Track B — B3_Ray_Tracer_BVH](../../02_Projects/B_Graphics_HPC/GraphicsHPC.md#b3_ray_tracer_bvh--flagship-project) (Nsight profiling of traversal kernel hotspots)
- [Track C — C2_Costmap_Inflation](../../02_Projects/C_Robotics_ROS2/RoboticsROS2.md#c2_costmap_inflation--distance-transform-on-gpu) (race-condition detection in inflation kernel)

---

[Back to Toolbox](../Toolbox.md)
