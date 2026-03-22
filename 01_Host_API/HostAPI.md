# Module 1: Host API and Feedback Loop

Learn how to control the GPU from C++ and measure whether it's actually faster than your CPU. No theory dumps—you'll process real images, see results instantly, and measure exactly where the time goes.

## Prerequisites
See [main README](../README.md) for base requirements (OpenCL, CMake, Docker setup).

---

## Heterogeneous Architecture in a Nutshell

Before touching code, you need one mental model. Everything in this module (and Module 2) is a consequence of it.

**Two processors, two memory spaces, one bus between them.**

```
┌──────────────────────┐        PCIe bus         ┌──────────────────────┐
│        HOST          │ ──── data transfers ───► │       DEVICE         │
│  (CPU + system RAM)  │ ◄─── results ──────────  │   (GPU + VRAM)       │
│                      │                          │                      │
│  Your C++ program    │                          │  Kernel (.cl file)   │
│  runs here           │                          │  runs here           │
└──────────────────────┘                          └──────────────────────┘
```

The GPU is not a faster CPU. It is a separate processor with its own memory. To use it:
1. **Copy data to the device** (`clEnqueueWriteBuffer`)
2. **Tell the device to run your kernel** (`clEnqueueNDRangeKernel`)
3. **Copy results back to the host** (`clEnqueueReadBuffer`)

Every step costs time. Step 1 and 3 cross the PCIe bus. This is why the first thing you measure (in `02_Visual_Kernel_Events`) is *not* kernel time — it's transfer time.

**Command Queue**: the ordered list of work you submit to the device. You enqueue commands (write, kernel launch, read) and the device executes them asynchronously. `clFinish()` blocks the host until all enqueued work is done.

```
Host thread                    Command Queue              GPU
    │                               │                      │
    ├─ enqueueWriteBuffer ─────────►│                      │
    ├─ enqueueNDRangeKernel ───────►│ ── upload ──────────►│
    ├─ enqueueReadBuffer ──────────►│ ── kernel ──────────►│
    ├─ clFinish() ──────────────────│ ── download ─────────│
    │  (blocks here)                │                      │
    ◄─ returns ────────────────────────────────────────────┘
```

That's it. The entire OpenCL programming model fits in this diagram. The rest is details.

---

## Contents
```
01_Visual_Kernel/         Basic image filter (MAD operation)
02_Visual_Kernel_Events/  Add event-based profiling
03_Buffer_Flags/          Buffer flag experiments (USE_HOST_PTR vs COPY_HOST_PTR)
```

---

### Why C++ Wrapper (cl.hpp)?

The raw C API requires manual `clRelease*` calls for every object — easy to forget and silent to leak. The C++ wrapper uses RAII so destructors handle cleanup automatically.

**Raw C API** (painful):

```c
cl_context ctx = clCreateContext(props, 1, &device, NULL, NULL, &err);
// ...50 lines later, did you remember to:
clReleaseContext(ctx);  // Easy to forget = memory leak
```

**C++ Wrapper** (RAII magic):

```cpp
cl::Context ctx(device);  // Destructor cleans up automatically
```

We use `cl.hpp` throughout this course for cleaner, safer code.

---

## 01_Visual_Kernel — Visual "Hello World"
**Goal**: Get immediate visual feedback that your GPU code works.

### Build & run
```bash
cd 01_Visual_Kernel
cmake -B build
cmake --build build
./build/visual_kernel --contrast 1.2 --brightness 10
# On multi-GPU systems, pin a vendor: GPU=NVIDIA ./build/visual_kernel
# Check output.bmp - should be brighter than input
```

### Verify
Output image exists and shows brightness/contrast adjustment (visually compare with input).

> **Note**: Pass `--kernel vec3` to run a vectorized variant that processes three color channels in a single `float3` operation instead of separate per-channel passes.

### Mini-challenge

- Modify the kernel to invert colors (`255 - pixel_value`) and verify the output image.

---

## 02_Visual_Kernel_Events — Measure Everything
**Goal**: Build the habit—measure, then optimize.

### Build & run
```bash
cd 02_Visual_Kernel_Events
cmake -B build
cmake --build build
./build/visual_kernel_events -p
```

### Verify
Console prints timing breakdown:
- Upload to GPU: X ms
- Kernel execution: Y ms
- Download from GPU: Z ms
- Total: T ms

Two files are written: `gradient_input.bmp` (before) and `output.bmp` (after). Open both side-by-side to confirm the filter was applied.

### Mini-challenge

- Run with different image sizes (256×256 vs 1920×1080 vs 4096×4096). At what image size does kernel-only time exceed the upload time?
  - Tip: on multi-GPU systems, results vary by device — use `GPU=NVIDIA` / `GPU=AMD` / `GPU=INTEL` to pin the target.
- Modify the kernel to invert colors (`255 - pixel`) and verify the output changes. Which profiling stage changes? (Hint: only kernel time.)

### Generating test images

Requires `sudo apt install ffmpeg`. Then run:

```bash
ffmpeg -y -f lavfi -i "color=c=gray:s=4096x4096" -vframes 1 -f image2 -vcodec bmp test_4k.bmp
./build/visual_kernel_events -p -i test_4k.bmp
```

---

## 03_Buffer_Flags — Memory Matters
**Goal**: Understand when data gets copied and how buffer flags affect performance.

### Build & run
```bash
cd 03_Buffer_Flags
cmake -B build
cmake --build build
./build/buffers_layout_demo
```

### Verify
Program runs without errors and prints comparative timing for different buffer strategies.

### Mini-challenge
Run `./build/buffers_layout_demo` and read the printed timing table. Which buffer strategy shows the lowest total time? Why does the winner win on your hardware? (Consider: does your GPU share memory with the CPU, or is it discrete?)

- Tip: on multi-GPU systems, results vary by device — use `GPU=NVIDIA` / `GPU=AMD` / `GPU=INTEL` to pin the target.
- Optional deep-dive: [Toolbox: Zero-Copy](../99_Toolbox/ZeroCopy/ZeroCopy.md) — the hardware model behind these flags and when each wins.

---

## Core Concepts

### Host-Side Control Flow

Every OpenCL program follows this pattern:

1. **Setup (once)**: Platform → Device → Context → Queue
2. **Loop (per frame)**:
   - Create/reuse buffers
   - Enqueue kernel
   - Read results
3. **Cleanup**: RAII handles this automatically

---

### Why OpenCL 1.2 (Not 2.0)?

**Short answer**: Nvidia's OpenCL 2.0 support is weak.  
**Pragmatic choice**: 1.2 works everywhere (Nvidia, AMD, Intel, mobile).

**What we're missing**: SVM (Shared Virtual Memory), Pipes.  
**When we'll revisit**: Module 4.2 (OpenCL vs CUDA comparison).

---

### Why "GPU Slower Than CPU"?

Common reasons:
- Memory transfers dominate compute time
- Workload too small to amortize overhead
- Poor memory access patterns

This isn't failure—it's the point of the feedback loop. Measurement tells you when GPU makes sense.

---

## Troubleshooting

- **Wrong GPU selected / multiple devices**:
  - List available platforms/devices: `clinfo -l`
  - Pin by vendor substring (case-insensitive): `GPU=NVIDIA ./build/visual_kernel`
  - Valid values: `NVIDIA`, `AMD`, `INTEL` (or any substring of the vendor string)
  - Default (no `GPU` set): first platform with a GPU; CPU fallback if no GPU found.

- **"No OpenCL platforms found"**:
  - Docker users: Did you run with `--gpus all`?
  - Native setup: Check `clinfo` output (see [Module 0 troubleshooting](../00_Setup/Setup.md))

- **Segmentation fault on buffer readback**:
  - Verify buffer size matches: `width * height * channels * sizeof(uchar)`
  - Wrap OpenCL calls in try-catch blocks with `cl::Error` for better diagnostics

- **Image output is blank/corrupted**:
  - Check kernel `global_work_size` matches image dimensions
  - For RGB images, remember stride = width × 3 bytes per pixel

- **Build fails with "cl.hpp not found"**:
  - Check that `common/` directory is in your include path
  - Verify CMake finds OpenCL: `find_package(OpenCL REQUIRED)`

---

## Performance Gate

This module is complete when:

- Event profiling output for `02_Visual_Kernel_Events -p` must show three non-zero timings (Upload, Kernel, Download)
- `01_Visual_Kernel` produces a valid `output.bmp` showing brightness/contrast adjustment

---

## What's Next

Module 2 applies these host-side skills to real integration problems. Choose your track:

- **[Track A: Multimedia](../02_Projects/A_Multimedia/README.md)** — Video AI, OpenCV interop, smart webcam project
- **[Track B: Graphics/HPC](../02_Projects/B_Graphics_HPC/GraphicsHPC.md)** — Ray tracing, CLBlast, advanced rendering
- **[Track C: Robotics](../02_Projects/C_Robotics_ROS2/RoboticsROS2.md)** — ROS 2 node acceleration, perception pipelines

All tracks use the **[Optimization Toolbox](../99_Toolbox/Toolbox.md)** to solve performance bottlenecks.
