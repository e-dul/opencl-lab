# Module 1: Host API and Feedback Loop

Learn how to control the GPU from C++ and measure whether it's actually faster than your CPU. No theory dumps—you'll process real images, see results instantly, and measure exactly where the time goes.

## Prerequisites
See [main README](../README.md) for base requirements (OpenCL, CMake, Docker setup).

## Contents
```
01_VisualKernel/          Basic image filter (MAD operation)
02_VisualKernel_Events/   Add event-based profiling  
03_Buffers_Layout/        Memory management experiments
```

---

## 01_VisualKernel — Visual "Hello World"
**Goal**: Get immediate visual feedback that your GPU code works.

### Build & run
```bash
cd 01_VisualKernel
cmake -B build
cmake --build build
./build/visual_kernel --contrast 1.2 --brightness 10
# Check output.bmp - should be brighter than input
```

### Verify
Output image exists and shows brightness/contrast adjustment (visually compare with input).

### Mini-challenge

- Modify the kernel to invert colors (`255 - pixel_value`) and verify the output image.

---

## 02_VisualKernel_Events — Measure Everything
**Goal**: Build the habit—measure, then optimize.

### Build & run
```bash
cd 02_VisualKernel_Events
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

### Mini-challenge

- Run with different image sizes (256×256 vs 1920×1080 vs 4096×4096). When does GPU start winning over CPU?
- Modify the kernel to invert colors (`255 - pixel`) and verify the output changes. Which profiling stage changes? (Hint: only kernel time.)
  
### Generating test images

```bash
ffmpeg -y -f lavfi -i "color=c=gray:s=4096x4096" -vframes 1 -f image2 -vcodec bmp test_4k.bmp
```

---

## 03_Buffers_Layout — Memory Matters
**Goal**: Understand when data gets copied and how buffer flags affect performance.

### Build & run
```bash
cd 03_Buffers_Layout
cmake -B build
cmake --build build
./build/buffers_layout_demo
```

### Verify
Program runs without errors and prints comparative timing for different buffer strategies.

### Mini-challenge
Profile `CL_MEM_USE_HOST_PTR` vs `CL_MEM_COPY_HOST_PTR` for your test image. Which is faster? Explain in 2–3 sentences why.

---

## Core Concepts

### Why C++ Wrapper (cl.hpp)?

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

- **"No OpenCL platforms found"**: 
  - Docker users: Did you run with `--gpus all`?
  - Native setup: Check `clinfo` output (see Module 0 troubleshooting)

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

## What's Next

Module 2 applies these host-side skills to real integration problems. Choose your track:
- **[Track A: Multimedia](../02_Projects/A_Multimedia/README.md)** — Video AI, OpenCV interop, smart webcam project
- **[Track B: Graphics/HPC](../02_Projects/B_Graphics_HPC/README.md)** — Ray tracing, CLBlast, advanced rendering
- **[Track C: Robotics](../02_Projects/C_Robotics_ROS2/README.md)** — ROS 2 node acceleration, perception pipelines

All tracks use the **[Optimization Toolbox](../99_Toolbox/README.md)** to solve performance bottlenecks.
