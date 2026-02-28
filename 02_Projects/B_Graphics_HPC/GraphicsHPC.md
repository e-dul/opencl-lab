# Path B: Graphics & HPC

Build a ray tracer from scratch and make it fast enough to render 100k-triangle scenes at 60 FPS. You start by reaching for a battle-tested math library, discover where naive ray tracing breaks down at scale, then implement the spatial acceleration structure that fixes it.

## Prerequisites
See [main README](../../README.md) for base requirements (OpenCL, CMake, Docker setup).

**Additional:**
- OpenGL + GLFW (for B2 display interop): `sudo apt install libglfw3-dev libgl-dev`
- CLBlast (fetched automatically by CMake via FetchContent in B1)

## Contents
```
B1_CLBlast_MatMul/    Library vs kernel: when CLBlast beats your handwritten GEMM
B2_Ray_Tracer_Basic/  Minimal ray tracer + OpenGL interop (display without copying)
B3_Ray_Tracer_BVH/    Flagship: Stackless BVH traversal for 100k-triangle scenes
B4_Device_Enqueue/    Advanced: GPU spawning its own GPU work (OpenCL 2.0+)
```

---

## B1_CLBlast_MatMul — Library vs Hand-Written Kernel

**Goal**: Benchmark CLBlast GEMM against a naive matrix multiplication kernel and develop the instinct for when to reach for a library versus write your own. Preparation for Ray Tracer.

### Build & run
```bash
cd B1_CLBlast_MatMul
cmake -B build
cmake --build build
./build/matmul_demo --size 1024
# GPU=NVIDIA ./build/matmul_demo --size 2048
```

### Verify
Console prints a comparison table:
```
Matrix size: 1024x1024
[NAIVE  ] Kernel time:   184.2 ms   GFLOPS:  11.7
[CLBlast] GEMM time:      18.6 ms   GFLOPS: 115.4
Speedup: 9.8x
```
CLBlast should win by at least 5× on any modern GPU. If it doesn't, check that CLBlast found the right device (driver-level tuning tables are device-specific).

### Core Concept: Why CLBlast Wins
A naive GEMM kernel reads matrix B in a column-major access pattern — every thread in a warp reads a different cache line. CLBlast uses tiled shared memory and auto-tuned work-group sizes per device. Writing a kernel that matches it requires hundreds of lines of tile-loading code and per-device benchmarking. The rule: use a library for standard linear algebra; write custom kernels for non-standard memory access patterns (image filtering, ray traversal, graph algorithms).

### Mini-challenge
Reduce the matrix size to 64×64 and re-run. Which implementation wins now, and why? (Hint: kernel launch overhead.)

---

## B2_Ray_Tracer_Basic — Rays, Intersections, No Copies

**Goal**: Implement a minimal OpenCL ray tracer and display results via OpenGL interop — the framebuffer lives in GPU memory for both rendering and display.

### Build & run
```bash
cd B2_Ray_Tracer_Basic
cmake -B build
cmake --build build
./build/ray_tracer_basic --width 1280 --height 720 --spheres 16
# Headless (no display): ./build/ray_tracer_basic --output render.bmp
# GPU=NVIDIA ./build/ray_tracer_basic --width 1920 --height 1080
```

### Verify
- Window opens showing a sphere scene with diffuse lighting and shadows
- Console prints per-frame kernel time:
  ```
  Kernel (16 spheres, 1280x720):  4.2 ms
  ```
- Framebuffer is never downloaded to CPU during the render loop — verify by watching GPU memory bandwidth in `nvtop` or `radeontop`

### Core Concept: OpenGL Interop
Without interop, the loop is: render → download to CPU → upload to OpenGL texture → display. With `cl_khr_gl_sharing`, the OpenCL kernel writes directly into a GL texture object. The display path becomes: render → display. No PCIe transfer.

```cpp
// Acquire GL texture as an OpenCL image object
cl::ImageGL cl_image(context, CL_MEM_WRITE_ONLY, GL_TEXTURE_2D, 0, tex_id);
std::vector<cl::Memory> gl_objects = {cl_image};
queue.enqueueAcquireGLObjects(&gl_objects);
kernel.setArg(0, cl_image);
queue.enqueueNDRangeKernel(kernel, cl::NullRange, global, local);
queue.enqueueReleaseGLObjects(&gl_objects);
```

### Mini-challenge
Add a second light source. Where does the kernel time increase — linearly with light count, or sub-linearly? Profile with `cl::Event` to find out.

---

## B3_Ray_Tracer_BVH — Flagship Project

**Goal**: Extend the basic ray tracer with a Bounding Volume Hierarchy to render scenes with 100k+ triangles at 60 FPS, hitting the performance gate.

### Build & run
```bash
cd B3_Ray_Tracer_BVH
cmake -B build
cmake --build build
./build/ray_tracer_bvh --scene ../../../assets/bunny.obj --width 1920 --height 1080
# Headless: ./build/ray_tracer_bvh --scene ../../../assets/bunny.obj --output render.bmp --frames 10
# GPU=NVIDIA ./build/ray_tracer_bvh --scene ../../../assets/bunny.obj
```

### Verify
- Scene renders correctly (no missing triangles, no black artifacts)
- Console prints traversal statistics:
  ```
  Scene: 100k triangles, BVH depth: 17
  [NAIVE ] Render time:  3240.0 ms   (0.3 FPS)
  [BVH   ] Render time:    14.8 ms   (67.6 FPS) ← must be ≥ 60 FPS to pass
  BVH speedup: 219x
  ```

### Core Concept: Why Stackless BVH?

**The recursion problem**: Standard BVH traversal is recursive — a ray hits a node, recurses into children, returns, continues. Recursion requires a call stack. GPU threads have no stack.

**The solution**: Iterative traversal using a bitmask (or parent pointer encoding) to track which sibling nodes still need to be visited.

```cl
// Stackless traversal — no function-call stack required
uint node = 0;
uint hit_link = MISS;
while (node != MISS) {
    if (intersects_aabb(ray, bvh[node].bounds)) {
        if (bvh[node].is_leaf) {
            test_triangles(ray, bvh[node]);
            node = bvh[node].miss_link;
        } else {
            node = bvh[node].hit_link;
        }
    } else {
        node = bvh[node].miss_link;
    }
}
```

Each node stores a `hit_link` (left child) and `miss_link` (right sibling or parent's right sibling), precomputed on the CPU during BVH build.

**Thread divergence**: Rays in the same warp will follow different tree paths. Replace `if (intersects_aabb(...))` branching with `select()` where possible. See [Toolbox: Thread Divergence](../../99_Toolbox/ThreadDivergence/README.md).

### BVH Build (CPU side)
The BVH is built on the CPU using Surface Area Heuristic (SAH) and uploaded once as a flat array. The kernel only traverses — it never modifies the structure.

```
CPU: build SAH-BVH → flatten to array → cl::Buffer upload (once)
GPU: per-ray stackless traversal (every frame)
```

### Privacy Mode Challenge — Dynamic Scene
Make one object in the scene move per frame. The BVH must be rebuilt (or refitted) each frame. Compare:
1. Full rebuild on CPU each frame
2. BVH refit (update leaf AABBs without restructuring)
3. Partial rebuild (only the subtree containing the moving object)

Profile all three with `cl::Event` timing on the upload stage.

### Mini-challenge
Visualize BVH depth per pixel: color each pixel by how many nodes the ray visited (0 = blue, max = red). This is your hotspot map — the red regions tell you where the tree is unbalanced.

---

## B4_Device_Enqueue — GPU Spawning GPU Work (Advanced)

**Goal**: Use OpenCL 2.0 Device Enqueue (`enqueue_kernel`) to spawn secondary ray kernels (reflections, refractions) from within the primary ray kernel — no CPU round-trip between bounces.

> **Level**: Advanced (Theory: 9/10). Requires OpenCL 2.0+ runtime. Check: `clinfo | grep "Device OpenCL C"` — must show `2.0` or higher.

### Build & run
```bash
cd B4_Device_Enqueue
cmake -B build
cmake --build build
./build/device_enqueue_demo --scene ../../../assets/cornell_box.obj --bounces 3
```

### Verify
- Reflective surfaces show correct multi-bounce lighting
- Console prints bounce timing:
  ```
  Primary rays:      4.1 ms
  Reflection pass 1: 2.8 ms  (spawned from GPU)
  Reflection pass 2: 1.4 ms  (spawned from GPU)
  Total (3 bounces): 8.3 ms  vs CPU-dispatched: 18.7 ms
  ```

### Core Concept: Device Enqueue
In OpenCL 1.2, the CPU dispatches each bounce: kernel finishes → CPU reads hit data → CPU dispatches next kernel. Each round-trip adds ~0.5–2 ms of latency.

In OpenCL 2.0+, a kernel can enqueue child kernels directly:

```cl
// Inside the primary ray kernel (OpenCL 2.0+)
if (hit.is_reflective) {
    queue_t q = get_default_queue();
    ndrange_t range = ndrange_1D(num_reflective_hits);
    enqueue_kernel(q, CLK_ENQUEUE_FLAGS_NO_WAIT, range,
        ^{ reflection_kernel(hit_buffer, output); });
}
```

No CPU involvement between primary and secondary rays.

**Compatibility note**: Nvidia's OpenCL 2.0 support is incomplete — Device Enqueue may not work on Nvidia hardware. Verified on AMD (ROCm) and Intel (NEO) drivers. This is one of OpenCL 2.0's most compelling features that CUDA (as `cudaLaunchKernel` from device) only added later.

### Mini-challenge
Compare three dispatch strategies for 3-bounce reflections:
1. CPU-dispatched (OpenCL 1.2 style)
2. Device-enqueued (OpenCL 2.0)
3. Unrolled into a single kernel with a loop

Which wins on your hardware? The answer depends on the ratio of divergent vs convergent rays.

---

## Performance Gate

This track is complete when:

| Project | Metric | Target |
|:--------|:-------|:-------|
| Advanced Ray Tracer (B3) | Render time | 60 FPS @ 100k triangles, 1920×1080 |

**Measure with `cl::Event` profiling** on the kernel, not total frame time. The upload (BVH buffer) is a one-time cost — exclude it from the per-frame measurement.

**Hint**: If you're under 60 FPS, profile first. Common culprits in order of frequency:
1. Thread divergence in traversal (fix: `select()` in inner loop)
2. Uncoalesced triangle data reads (fix: Structure-of-Arrays layout)
3. Work-group size not tuned for occupancy (fix: [Toolbox: Work-Group Sizing](../../99_Toolbox/WorkGroupSizing/README.md))
4. `rsqrt`/`sqrt` in ray normalisation still using IEEE path (fix: [Toolbox: Fast Math](../../99_Toolbox/FastMath/README.md))

---

## Troubleshooting

- **OpenGL interop init fails**: Verify `cl_khr_gl_sharing` extension: `clinfo | grep gl_sharing`. Not available on all CPU-fallback runtimes (PoCL).
- **BVH renders black patches**: Miss-link pointers are wrong — draw the BVH tree to a file and verify parent-child-sibling linkage before running on GPU.
- **`enqueue_kernel` returns `CL_INVALID_OPERATION` (B4)**: Your runtime does not support Device Enqueue. Check: `clinfo | grep "Device OpenCL C"` for `2.0+`. AMD ROCm and Intel NEO both support it; Nvidia OpenCL typically does not.
- **CLBlast not found (B1)**: CMake FetchContent downloads it at configure time — requires internet access. Offline: set `-DCMAKE_PREFIX_PATH=/path/to/clblast/install`.
- **Wrong GPU**: `GPU=NVIDIA ./build/ray_tracer_bvh`, `GPU=AMD ./build/ray_tracer_bvh`, `GPU=INTEL ./build/ray_tracer_bvh`.

---

## What's Next

[Track A: Multimedia](../A_Multimedia/Multimedia.md) — zero-copy video pipelines and edge AI inference.

[Track C: Robotics/ROS 2](../C_Robotics_ROS2/RoboticsROS2.md) — GPU acceleration inside a ROS 2 node, Lidar perception.

[Optimization Toolbox](../../99_Toolbox/Toolbox.md) — Thread Divergence, Work-Group Sizing, Local Memory, Async Pipelines.

[Module 4 Add-ons](../../04_Addons/Addons.md) — 3D Voxel Mapping (grand finale combining B ray casting with C Lidar data).
