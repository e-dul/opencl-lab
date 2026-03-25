# 1.1 — Visual Kernel

**Goal**: Get immediate visual feedback that your GPU code works — apply a brightness/contrast (MAD) filter to a real image and verify the result by eye.

## Prerequisites (delta from module index)

None. This is the first sub-module. See [Module 1: Host API](../HostAPI.md) for base requirements.

## Build & Run

```bash
cd 01_Visual_Kernel
cmake -B build
cmake --build build
./build/visual_kernel --contrast 1.2 --brightness 10
# GPU=NVIDIA ./build/visual_kernel --contrast 1.2 --brightness 10
```

To run the vectorized variant (processes all three RGB channels in a single `float3` operation):

```bash
./build/visual_kernel --contrast 1.2 --brightness 10 --kernel vec3
```

## Verify

- `output.bmp` — the filter result. Open side-by-side with `gradient_input.bmp` (written alongside it) to confirm visibly higher brightness and contrast. No numbers to check — just visual confirmation the filter was applied.

## Key Concepts

### The Host-Side OpenCL Setup Pattern

Every OpenCL program on the host follows the same seven steps. After this sub-module you will have seen each one in `src/main.cpp`:

1. **Platform** — query available OpenCL platforms
2. **Device** — select a GPU (or CPU fallback)
3. **Context** — logical container binding device to host program
4. **Queue** — ordered stream of commands sent to the device
5. **Build Program** — compile `.cl` source at runtime (`cl::Program`)
6. **Kernel** — a named entry point in the compiled program
7. **Buffer → Enqueue → Read** — allocate device memory, dispatch the kernel, copy results back

The full round-trip:

```
Host: enqueueWriteBuffer → enqueueNDRangeKernel → enqueueReadBuffer → queue.finish()
      (upload to GPU)        (run mad.cl)           (download result)   (block until done)
```

### Why C++ Wrapper (cl.hpp) Instead of the Raw C API

The raw C API requires a manual `clRelease*` call for every object — easy to miss, silent to leak. The C++ wrapper uses RAII: destructors release resources automatically.

Raw C API (error-prone):
```c
cl_context ctx = clCreateContext(props, 1, &device, NULL, NULL, &err);
// ... 50 lines later ...
clReleaseContext(ctx);  // forget this = silent resource leak
```

C++ wrapper (RAII):
```cpp
cl::Context ctx(device);  // destructor releases automatically
```

This project uses `cl.hpp` (C++ bindings v1.2) throughout for cleaner, safer code.

### The MAD Kernel (`kernels/mad.cl`)

The scalar kernel dispatches one work-item per byte:

```c
dst[gid] = clamp(src[gid] * contrast + brightness, 0.0f, 255.0f);
```

The `--kernel vec3` variant dispatches one work-item per pixel and processes all three channels with `float3` in a single fused `mad()` call — fewer dispatched work-items, same result.

### Why OpenCL 1.2 (Not 2.0)?

**Short answer**: Nvidia's OpenCL 2.0 support is partial or absent on most drivers.

**Pragmatic choice**: OpenCL 1.2 works everywhere — Nvidia, AMD, Intel, mobile, PoCL.

**What 2.0 adds**: SVM (Shared Virtual Memory), Pipes, device-side enqueue. These are covered in Module 4 where hardware support can be verified at runtime. 1.2 features cover 95% of real workloads.

## Mini-Challenge

Modify `kernels/mad.cl` to invert colors instead of applying brightness/contrast:

```c
dst[gid] = 255 - src[gid];
```

Rebuild and run. Verify that `output.bmp` shows the inverted gradient — what was dark is now bright, what was bright is now dark.

---

[Module 1: Host API](../HostAPI.md)
