# Multi-GPU Strategy

**Symptom**: Single GPU throughput ceiling reached. Adding a second GPU shows no benefit because workload is pinned to one device.

**Performance gate**: >= 1.65x speedup with 2x GPUs on the same workload.

## Prerequisites
Prerequisites: OpenCL 1.2+, CMake 3.18+, `clinfo` installed. See [main README](../../README.md) for base requirements.

**Additional**: Two OpenCL-capable GPU devices on the same system.

## Build & Run
```bash
cd 05_Toolbox/08_Multi_GPU_Strategy
cmake -B build && cmake --build build
```

Run with `--gpus 1` first to establish the single-GPU baseline, then with `--gpus 2`:

```bash
./build/multi_gpu_strategy --width 3840 --height 2160 --gpus 1
./build/multi_gpu_strategy --width 3840 --height 2160 --gpus 2
```

## Verify

```text
[GPU 0 only ] 4K process time: 24.1 ms
[GPU 0 + 1  ] 4K process time: 13.8 ms   Speedup: 1.75x  ← must be >= 1.65x
```

## Concept

Three-step recipe:

```cpp
// 1. Enumerate all GPU devices
std::vector<cl::Device> devices;
for (auto& platform : platforms)
    platform.getDevices(CL_DEVICE_TYPE_GPU, &devices);

// 2. Shared context across all devices (enables buffer sharing)
cl::Context ctx(devices);

// 3. Per-device queue and workload split
int slice = height / devices.size();
for (int i = 0; i < (int)devices.size(); i++) {
    cl::CommandQueue q(ctx, devices[i], CL_QUEUE_PROFILING_ENABLE);
    // Upload slice i, dispatch kernel, download results
    // Use cl::Event to synchronize slices before final merge
}
```

**Workload splitting strategies**:
- **Static (equal slices)**: simple, works when GPUs are identical
- **Dynamic (work-stealing queue)**: needed when GPUs have different throughput
- **Proximity (NUMA-aware)**: assign data that lives near the GPU to that GPU (PCIe topology)

**Synchronization**: use `clWaitForEvents` on all per-device completion events before accessing the merged result. Never use `clFinish` on one queue while another is still running — they are independent.

## Mini-Challenge

Implement dynamic load balancing: give GPU 0 60% of the rows and GPU 1 40%. Measure whether a faster/slower GPU pair performs better with a static equal split or with a split proportional to each device's single-GPU throughput.

## Troubleshooting

- **Only one GPU detected**: Check `clinfo -l` for all available platforms. Mixed-vendor setups (Nvidia + Intel iGPU) may require the Khronos ICD loader to enumerate both.
- **Speedup below 1.65x**: PCIe bandwidth is likely the bottleneck, not compute. Profile with Nsight/VTune to confirm whether transfer or kernel time dominates at 4K.
- **Shared context across different vendors fails**: A `cl::Context` spanning devices from different platforms is not possible in OpenCL 1.2. Create separate contexts per vendor and synchronize on the host side.
- **Total N-GPU time shows an absurd value (e.g. 1.7e12 ms)**: Cross-platform profiling timestamps (e.g. NVIDIA + AMD) use independent device clocks with no shared epoch. `max(read_end) - min(write_start)` across platforms is meaningless. Use `GPU=<vendor>` to restrict to a single platform, or interpret only the per-device kernel times.
- **N-GPU leg is slower than single-GPU baseline**: Devices are heterogeneous(e.g. discrete NVIDIA vs integrated AMD). Equal row partitioning makes total time = slowest device. The Mini-Challenge's proportional split is the fix.

## Used In
- [Track B — 02_Ray_Tracer_BVH](../../03_GraphicsHPC/02_Ray_Tracer_BVH/RayTracerBVH.md) (multi-GPU BVH rendering split by tile rows)
- [Track C — 03_Perception_Node](../../04_Robotics/03_Perception_Node/PerceptionNode.md) (point cloud partitioning across two GPUs)

---

[Back to Toolbox](../Toolbox.md)
