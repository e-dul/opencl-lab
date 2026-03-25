# Module 1: Host API and Feedback Loop

Learn how to control the GPU from C++ and measure whether it's actually faster than your CPU. No theory dumps — you'll process real images, see results instantly, and measure exactly where the time goes.

## Prerequisites

See [main README](../README.md) for base requirements (OpenCL, CMake 3.18+, Docker setup).

## Heterogeneous Architecture in a Nutshell

**Two processors, two memory spaces, one bus between them.**

```text
┌──────────────────────┐        PCIe bus         ┌──────────────────────┐
│        HOST          │ ──── data transfers ───► │       DEVICE         │
│  (CPU + system RAM)  │ ◄─── results ──────────  │   (GPU + VRAM)       │
│                      │                          │                      │
│  Your C++ program    │                          │  Kernel (.cl file)   │
│  runs here           │                          │  runs here           │
└──────────────────────┘                          └──────────────────────┘
```

The GPU is not a faster CPU. It is a separate processor with its own memory. Every data transfer crosses the PCIe bus — `enqueueWriteBuffer` (upload), `enqueueNDRangeKernel` (dispatch), `enqueueReadBuffer` (download). `clFinish()` blocks the host until all enqueued work is done.

## Contents

| Sub-module | Goal | Doc |
| :--------- | :--- | :-- |
| 01 — Visual Kernel | First GPU kernel: brightness/contrast on a real image | [VisualKernel.md](01_Visual_Kernel/VisualKernel.md) |
| 02 — Visual Kernel Events | Add `cl::Event` profiling: measure upload, kernel, download | [VisualKernelEvents.md](02_Visual_Kernel_Events/VisualKernelEvents.md) |
| 03 — Buffer Flags | Buffer flag experiments: USE_HOST_PTR vs COPY_HOST_PTR | [BufferFlags.md](03_Buffer_Flags/BufferFlags.md) |

## Performance Gate

- `02_Visual_Kernel_Events -p` must print three non-zero timings (Upload, Kernel, Download).
- `01_Visual_Kernel` must produce a valid `output.bmp` showing brightness/contrast adjustment.

## Troubleshooting

- **Wrong GPU / multiple devices**: `GPU=NVIDIA ./build/visual_kernel`, `GPU=AMD`, `GPU=INTEL`. List all: `clinfo -l`.
- **"No OpenCL platforms found"**: Docker users — run with `--gpus all`. Check `clinfo` output.
- **Segmentation fault on readback**: verify buffer size = `width × height × channels × sizeof(uchar)`.
- **Image output blank/corrupted**: check `global_work_size` matches image dimensions.
- **Build fails with "cl.hpp not found"**: check `common/` is in include path; `find_package(OpenCL REQUIRED)` must succeed.

## What's Next

Choose your specialization track:

- **[Track A: Multimedia & AI](../02_Multimedia/Multimedia.md)** — video pipelines, OpenCV interop, smart webcam
- **[Track B: Graphics & HPC](../03_GraphicsHPC/GraphicsHPC.md)** — ray tracing, CLBlast, BVH
- **[Track C: Robotics & ROS 2](../04_Robotics/RoboticsROS2.md)** — ROS 2 node acceleration, perception pipelines

All tracks use the **[Optimization Toolbox](../05_Toolbox/Toolbox.md)** to solve performance bottlenecks.
