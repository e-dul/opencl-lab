# Zero-Copy — Mapping vs Copying

**Symptom**: Upload stage takes > 5 ms for a 1080p frame. Profiler shows `enqueueWriteBuffer` as the bottleneck.

## Prerequisites
Prerequisites: OpenCL 1.2+, CMake 3.18+, `clinfo` installed. See [main README](../../README.md) for base requirements.

## Build & Run
```bash
cd 05_Toolbox/15_Zero_Copy
cmake -B build && cmake --build build
./build/zero_copy --width 1920 --height 1080
# GPU=NVIDIA ./build/zero_copy --width 3840 --height 2160
```

## Verify
```
Strategy              Kernel Time (ms)
──────────────────────────────────────
COPY_HOST_PTR            8.400        (pageable → GPU copy)
ALLOC_HOST_PTR           1.100        (pinned memory, faster DMA)
USE_HOST_PTR             0.000        (iGPU: buffer is physically shared)
MAP_UNMAP               <varies>      (most portable zero-copy pattern)
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

For a hardware-level explanation of why UMA changes the cost of each flag, see [4.4 SVM Theory](../../05_Toolbox/11_SVM_Theory/SVMTheory.md).

## Mini-Challenge

Switch the demo to `CL_MEM_ALLOC_HOST_PTR` and profile with `--width 3840 --height 2160`. At what resolution does the gap between `COPY_HOST_PTR` and `ALLOC_HOST_PTR` exceed 10 ms?

## Troubleshooting

- **`USE_HOST_PTR` shows no speedup on discrete GPU**: Expected. The driver is forced to copy to VRAM at kernel launch time. True zero-copy requires UMA hardware.
- **Map/Unmap missing from output**: Ensure the build includes profiling flag (`CL_QUEUE_PROFILING_ENABLE`).

## Used In
- [Track A — 01_OpenCV_Interop](../../02_Multimedia/01_OpenCV_Interop/OpenCVInterop.md)
- [Track C — 03_Perception_Node](../../04_Robotics/03_Perception_Node/PerceptionNode.md)

---

[Back to Toolbox](../Toolbox.md)
