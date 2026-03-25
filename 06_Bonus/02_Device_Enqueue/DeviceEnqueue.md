# Device-Side Enqueue

**When to use**: you want to generate GPU work from inside a GPU kernel — eliminating the round-trip to the host between reflection bounces in a ray tracer.

> **Requires:** OpenCL 2.0+ device with `cl_device_device_enqueue_support`. Falls back gracefully with an informational message on devices that do not support device-side enqueue.

## Goals

After completing this module you will be able to:

- Explain the parent/child kernel relationship in OpenCL 2.0 device-side enqueue.
- Compare GPU-spawned dispatch vs CPU-dispatched dispatch across multiple reflection bounces.
- Identify when device enqueue reduces total render time and when host overhead is negligible.
- Read the per-bounce timing table and connect dispatch strategy to observed latency.

## Prerequisites

See [Bonus.md](../Bonus.md) for base requirements (OpenCL 1.2+, CMake 3.18+).

- Module 1 (`01_Host_API/`) complete — no further prerequisites for the CPU-dispatched path.
- **OpenCL 2.0 capable device** — required for the GPU-spawned path (`--mode gpu`).

> **On unsupported hardware**: if the selected device does not report OpenCL C 2.0 or does not implement `CL_DEVICE_QUEUE_ON_DEVICE_PROPERTIES`, the binary prints a descriptive message and automatically falls back to `--mode cpu`. The process exits with code 0 — no crash, no silent hang.

## Build & Run

```bash
cd 06_Bonus/02_Device_Enqueue
cmake -B build && cmake --build build
# Default: GPU-spawned path, built-in sphere scene, 3 bounces
./build/device_enqueue
# Explicit mode comparison:
./build/device_enqueue --mode cpu --bounces 3
./build/device_enqueue --mode gpu --bounces 3
# Load an OBJ scene (assets/ is at repo root, two levels up):
./build/device_enqueue --scene ../../assets/cornell_box.obj --output render.bmp
# Select GPU vendor:
GPU=AMD ./build/device_enqueue --mode gpu --width 1920 --height 1080
```

| Flag | Default | Description |
| :--- | :------ | :---------- |
| `--mode` | `gpu` | Dispatch strategy: `gpu` (device enqueue) or `cpu` (host loop) |
| `--scene` | `builtin:spheres` | OBJ path or `builtin:spheres` |
| `--bounces` | `3` | Number of reflection bounces |
| `--frames` | `1` | Frame count for timing average |
| `--width` / `--height` | 1280 / 720 | Output resolution |
| `--output` | `render.bmp` | Output BMP path |

## Expected Output

The binary saves `render.bmp` and prints a per-bounce timing table followed by a summary:

```text
OpenCL C version: OpenCL C 2.0 AMD-APP (3513.0)
Triangles: 576
BVH nodes: 1105 for 576 triangles
Reflective triangles: 192 / 576

Running CPU-dispatched path (3 bounces, 1 frame(s))...
Running GPU-spawned path (OpenCL 2.0 device enqueue, 3 bounces, 1 frame(s))...

  Bounce | CPU-dispatched (ms)   | GPU-spawned (ms)
primary  |                 4.123 |           12.450
bounce 1 |                 1.871 |              N/A
bounce 2 |                 1.654 |              N/A
bounce 3 |                 1.522 |              N/A

Total CPU:  9.170 ms
Total GPU: 12.450 ms
Speedup (CPU/GPU): 0.74x
```

> Device enqueue overhead can exceed the per-bounce savings for small scenes — the speedup is scene and device dependent. The goal is to observe and understand the tradeoff, not to hit a fixed ratio.

The output BMP shows a multi-bounce reflection render of the scene.

## Key Concepts

### Parent/Child Kernel Relationship

In the CPU-dispatched path the host calls `enqueueNDRangeKernel` once per bounce:

```text
Host → primary_ray kernel → [return to host] → reflection_ray kernel → ...
```

Each arrow is a PCIe round-trip plus kernel launch overhead.

In the GPU-spawned path the primary kernel calls `enqueue_kernel()` from inside OpenCL C 2.0:

```text
Host → primary_ray kernel → [spawns] → reflection_ray → [spawns] → reflection_ray → ...
```

The host sees a single dispatch event. Reflection passes are scheduled entirely on the device.

### OpenCL 2.0 Device Queue

Device-side enqueue requires a `device_queue` object created on the host and passed as a kernel argument:

```c
// OpenCL C 2.0 kernel side
__kernel void primary_ray(__write_only queue_t dev_q, ...) {
    // spawn reflection pass without returning to host
    enqueue_kernel(dev_q, CLK_ENQUEUE_FLAGS_NO_WAIT,
                   ndrange_1D(pixel_count),
                   ^{ reflection_ray(...); });
}
```

This is an OpenCL 2.0 feature. Devices that report OpenCL 3.0 but do not implement the optional device-enqueue extension will have `CL_DEVICE_QUEUE_ON_DEVICE_PROPERTIES == 0` — the binary detects this and falls back cleanly.

### When to Use Device Enqueue

Device enqueue removes host round-trips between dependent GPU passes. The benefit is largest when:

- Each bounce is short (low per-pass latency) — PCIe round-trip cost dominates.
- Many bounces are chained — overhead compounds.

For long passes or shallow bounce counts the host-dispatch path is simpler and comparable in speed.

> **Historical note**: CUDA had equivalent capability (Dynamic Parallelism) since CUDA 5.0 (2012). `enqueue_kernel` brings the same model to any OpenCL device — vendor-neutral, no CUDA SDK required.

## Mini-Challenge

Compare three dispatch strategies for 3-bounce reflections:

1. `--mode cpu` — host dispatches each bounce explicitly (OpenCL 1.2 style).
2. `--mode gpu` — primary kernel spawns reflection passes via `enqueue_kernel` (OpenCL 2.0).
3. Modify the primary kernel to unroll all bounces into a single loop — no secondary dispatch at all.

Which wins on your hardware? The answer depends on the ratio of divergent vs convergent rays and whether your device's device-queue implementation has low overhead. Profile each strategy with `cl::Event` timing and record the per-bounce ms.

---

[← Bonus Modules](../Bonus.md)
