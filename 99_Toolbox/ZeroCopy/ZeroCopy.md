# Zero-Copy — Mapping vs Copying

**Symptom**: Upload stage takes > 5 ms for a 1080p frame. Profiler shows `enqueueWriteBuffer` as the bottleneck.

## Prerequisites
Prerequisites: OpenCL 1.2+, CMake 3.18+, `clinfo` installed. See [main README](../../README.md) for base requirements.

## Build & Run
```bash
cd 99_Toolbox/ZeroCopy
cmake -B build && cmake --build build
./build/zero_copy --width 1920 --height 1080
# GPU=NVIDIA ./build/zero_copy --width 3840 --height 2160
```

## Verify
```
[COPY_HOST_PTR  ] Upload: 8.4 ms   (pageable → GPU copy)
[ALLOC_HOST_PTR ] Upload: 1.1 ms   (pinned memory, faster DMA)
[USE_HOST_PTR   ] Upload: 0.0 ms   (iGPU: buffer is physically shared)
```

Run the demo. Once you see the timing difference, read the concept below.

## Concept

Three `cl::Buffer` creation flags, three different memory contracts:

| Flag | What OpenCL does | When to use |
|:-----|:-----------------|:------------|
| `CL_MEM_COPY_HOST_PTR` | Copies your data into its own allocation | Default, always safe |
| `CL_MEM_ALLOC_HOST_PTR` | Allocates pinned (page-locked) host memory | Discrete GPU, repeated uploads |
| `CL_MEM_USE_HOST_PTR` | Uses your pointer directly | UMA / iGPU, zero latency |

`CL_MEM_ALLOC_HOST_PTR` + `enqueueMapBuffer` is the most portable zero-copy pattern on discrete GPUs. `CL_MEM_USE_HOST_PTR` is only truly zero-copy on UMA architectures (Intel iGPU, ARM Mali).

## Mini-Challenge

Switch the demo to `CL_MEM_ALLOC_HOST_PTR` and profile with `--width 3840 --height 2160`. At what resolution does the gap between `COPY_HOST_PTR` and `ALLOC_HOST_PTR` exceed 10 ms?

## Troubleshooting

- **`USE_HOST_PTR` shows no speedup on discrete GPU**: Expected. The driver is forced to copy to VRAM at kernel launch time. True zero-copy requires UMA hardware.
- **Map/Unmap missing from output**: Ensure the build includes profiling flag (`CL_QUEUE_PROFILING_ENABLE`).

## Used In
- [Track A — A1_OpenCV_Interop](../../02_Projects/A_Multimedia/Multimedia.md#a1_opencv_interop--kill-the-copy)
- [Track C — C3_Perception_Node](../../02_Projects/C_Robotics_ROS2/RoboticsROS2.md#c3_perception_node--flagship-project)

---

[Back to Toolbox](../Toolbox.md)
