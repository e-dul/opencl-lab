# B.2 — Ray Tracer Basic: Rays, Intersections, No Copies

**Goal**: Implement a minimal OpenCL ray tracer and display results via OpenGL interop — the framebuffer lives in GPU memory for both rendering and display, eliminating PCIe transfers entirely.

## Prerequisites (delta from module index)

- OpenGL + GLFW (live window only): `sudo apt install libglfw3-dev libgl-dev` — optional; headless `--output render.bmp` works without a display.
- Assets: none required for the default sphere scene.

## Build & Run

```bash
cd B2_Ray_Tracer_Basic
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/ray_tracer --width 1280 --height 720
# Live window (requires libglfw3-dev):
./build/ray_tracer --live
# Headless (no display):
./build/ray_tracer --output render.bmp
# GPU=NVIDIA ./build/ray_tracer --width 1920 --height 1080
```

## Verify

- Window shows a sphere scene with diffuse lighting and shadows.
- Console prints per-frame kernel time:
  ```
  Kernel (1280x720):  4.2 ms
  ```
- The framebuffer is never downloaded to CPU during the render loop. To confirm: pass `--debug-download` and observe frame time increase by several milliseconds compared to baseline.

## Key Concepts

### OpenGL Interop (`cl_khr_gl_sharing`)

Without interop, the frame loop is: render → download to CPU → upload to OpenGL texture → display. With `cl_khr_gl_sharing`, the OpenCL kernel writes directly into a GL texture object:

```cpp
// Acquire GL texture as an OpenCL image object — GPU-side ownership handoff
cl::ImageGL cl_image(context, CL_MEM_WRITE_ONLY, GL_TEXTURE_2D, 0, tex_id);
std::vector<cl::Memory> gl_objects = {cl_image};
queue.enqueueAcquireGLObjects(&gl_objects);
kernel.setArg(0, cl_image);
queue.enqueueNDRangeKernel(kernel, cl::NullRange, global, local);
queue.enqueueReleaseGLObjects(&gl_objects);
// OpenGL can now display the texture — no memcpy, no PCIe transfer
```

### Why No PCIe Transfer

A 1280×720 RGBA framebuffer is ~3.5 MB. At PCIe Gen 3 bandwidth (~12 GB/s) a round-trip costs ~0.6 ms — over 36% of a 60 FPS frame budget. At 4K it exceeds 3 ms. Interop eliminates this cost by keeping the framebuffer in GPU-resident memory throughout.

## Mini-Challenge

Add a second light source in the kernel. Profile with `cl::Event` before and after: does kernel time scale linearly with light count, or sub-linearly? The answer reveals whether the bottleneck is arithmetic or memory latency.

## Troubleshooting

- **`cl_khr_gl_sharing` not listed**: run `clinfo | grep gl_sharing`. Not available on all CPU-fallback runtimes (PoCL).
- **Optimus/hybrid GPU — interop init fails**: GLFW creates a GL context on the iGPU; the NVIDIA OpenCL driver can only share with a GL context it owns. Force the NVIDIA GPU:
  ```bash
  __NV_PRIME_RENDER_OFFLOAD=1 __GLX_VENDOR_LIBRARY_NAME=nvidia GPU=NVIDIA ./build/ray_tracer --live
  ```

---

[Path B: Graphics & HPC](../GraphicsHPC.md)
