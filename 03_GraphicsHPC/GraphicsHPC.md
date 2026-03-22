# Path B: Graphics & HPC

Build a ray tracer from scratch and make it fast enough to render complex triangle scenes at 60 FPS. You start by reaching for a battle-tested math library, discover where naive ray tracing breaks down at scale, then implement the spatial acceleration structure that fixes it.

## Prerequisites
See [main README](../../README.md) for base requirements (OpenCL, CMake, Docker setup).

**Additional:**
- OpenGL + GLFW (live window in B2/B3): `sudo apt install libglfw3-dev libgl-dev` — optional; headless `--output render.bmp` works without it.
- CLBlast (B1) and tinyobjloader (B3/B4) are fetched automatically by CMake at configure time. Offline: `-DCMAKE_PREFIX_PATH=/path/to/clblast/install`.
- Assets in repository root: `assets/bunny.obj` (Stanford Bunny, ~70k triangles — sufficient for the gate; swap for a denser mesh if you want to push further), `assets/cornell_box.obj` (for B4).

## Contents
```
B1_CLBlast_MatMul/         Library vs kernel: when CLBlast beats your handwritten GEMM
B2_Ray_Tracer_Basic/       Minimal ray tracer + OpenGL interop (display without copying)
B3_Ray_Tracer_BVH/         Flagship: Stackless BVH traversal for 100k-triangle scenes
B3_Ray_Tracer_BVH_Dynamic/ Challenge: BVH rebuild vs refit vs static on a moving scene
B4_Device_Enqueue/         Advanced: GPU spawning its own GPU work (OpenCL 2.0+)
```

---

## B1_CLBlast_MatMul — Library vs Hand-Written Kernel

**Goal**: Benchmark CLBlast GEMM against a naive matrix multiplication kernel and develop the instinct for when to reach for a library versus write your own. Preparation for Ray Tracer.

> **Key terms**: GEMM (General Matrix Multiplication) is the operation C = α·A·B + β·C for dense matrices. For a square N×N multiply (α=1, β=0), the operation count is **FLOPS = 2N³** (N³ multiply-add pairs, each counting as 2 FLOPs). GFLOPS is 10⁹ floating-point operations per second — the standard throughput unit for dense linear algebra benchmarks.

### Build & run
```bash
cd B1_CLBlast_MatMul
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/b1_clblast_matmul --size 1024
# GPU=NVIDIA ./build/b1_clblast_matmul --size 2048
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
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/b2_ray_tracer --width 1280 --height 720
# Live window (requires libglfw3-dev): ./build/b2_ray_tracer --live
# Headless (no display): ./build/b2_ray_tracer --output render.bmp
# GPU=NVIDIA ./build/b2_ray_tracer --width 1920 --height 1080
```

### Verify
- Window opens showing a sphere scene with diffuse lighting and shadows
- Console prints per-frame kernel time:
  ```
  Kernel (1280x720):  4.2 ms
  ```
- Framebuffer is never downloaded to CPU during the render loop — verify programmatically: add a `clEnqueueReadBuffer` call in a `--debug-download` mode and confirm frame time increases by several milliseconds. The baseline (no download) is your proof.

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

**Goal**: Extend the basic ray tracer with a Bounding Volume Hierarchy to render complex triangle scenes at 60 FPS, hitting the performance gate. The default scene (`bunny.obj`) has ~70k triangles — already well beyond what brute-force intersection can handle interactively. Swap in a denser mesh if the gate feels too easy.

### Build & run
```bash
cd B3_Ray_Tracer_BVH
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/ray_tracer_bvh --scene ../../../assets/bunny.obj --width 1920 --height 1080
# Live window: ./build/ray_tracer_bvh --scene ../../../assets/bunny.obj --live
# Headless: ./build/ray_tracer_bvh --scene ../../../assets/bunny.obj --output render.bmp --frames 10
# GPU=NVIDIA ./build/ray_tracer_bvh --scene ../../../assets/bunny.obj
```

### Verify
- Scene renders correctly (no missing triangles, no black artifacts)
- Console prints traversal statistics:
  ```
  Scene: bunny.obj (~70k triangles), BVH depth: 16
  [NAIVE ] Render time:  2180.0 ms   (0.5 FPS)
  [BVH   ] Render time:    14.8 ms   (67.6 FPS) ← must be ≥ 60 FPS to pass
  BVH speedup: 147x
  ```

### Core Concept: Why Stackless BVH?

**The recursion problem**: Standard BVH traversal is recursive — a ray hits a node, recurses into children, returns, continues. Recursion requires a call stack. GPU threads have no stack.

**The solution**: Iterative traversal using a bitmask (or parent pointer encoding) to track which sibling nodes still need to be visited.

Each node stores two precomputed links: `hit_link` points to the left child (follow when the ray hits the bounding box), and `miss_link` points to the right sibling or the parent's right sibling (follow when the ray misses). Both are set on the CPU during BVH build — the GPU kernel only reads them.

```cl
// Stackless traversal — no function-call stack required
uint node = 0;
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

**Thread divergence**: Rays in the same warp will follow different tree paths. This is inevitable in BVH traversal — you will see it in the profiler after hitting the gate.

### BVH Build (CPU side)

SAH (Surface Area Heuristic) estimates the cost of a split by weighting the probability of a ray hitting a child node by its surface area.

The BVH is built on the CPU using SAH and uploaded once as a flat array. The kernel only traverses — it never modifies the structure.

```text
CPU: build SAH-BVH → flatten to array → cl::Buffer upload (once)
GPU: per-ray stackless traversal (every frame)
```

### Dynamic Scene Challenge
Make one object in the scene move per frame. The BVH must be updated each frame to stay correct. Measure the upload cost with `cl::Event` timing — then ask: is there a cheaper way to keep the BVH valid without a full rebuild?
See details in the [B3_Ray_Tracer_BVH_Dynamic](#b3_ray_tracer_bvh_dynamic--dynamic-scene-challenge) section below — three strategies benchmarked side-by-side with `cl::Event` timing.

### Mini-challenge
Visualize BVH depth per pixel: color each pixel by how many nodes the ray visited (0 = blue, max = red). This is your hotspot map — the red regions tell you where the tree is unbalanced.

---

## B3_Ray_Tracer_BVH_Dynamic — Dynamic Scene Challenge

**Goal**: Extend the BVH ray tracer to animate the scene (rigid Y-axis rotation each frame) and benchmark three per-frame BVH strategies: full rebuild, AABB refit, and static (stale BVH). The timing table makes the rebuild vs refit cost difference concrete and visible.

### Build & run

> Valid `--strategy` values: `rebuild`, `refit`, `static`.

```bash
cd B3_Ray_Tracer_BVH_Dynamic
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build

# Full SAH rebuild every frame
./build/b3_ray_tracer_dynamic --strategy rebuild --scene ../../../../assets/bunny.obj --frames 60 --output render_rebuild.bmp

# Bottom-up AABB refit (topology unchanged)
./build/b3_ray_tracer_dynamic --strategy refit --scene ../../../../assets/bunny.obj --frames 60 --output render_refit.bmp

# Stale BVH — geometry moves, BVH does not (intentional artifacts)
./build/b3_ray_tracer_dynamic --strategy static --scene ../../../../assets/bunny.obj --frames 60 --output render_static.bmp
```

### Verify
Console prints a one-row timing table per run. Reference numbers on NVIDIA RTX 4060 Laptop at 800×600, `bunny.obj` (~70k triangles), 60 frames:
```
Strategy | Depth     | BVH Build (ms) | Upload (ms) | Render (ms) | Total (ms) | FPS
---------|-----------|----------------|-------------|-------------|------------|----
rebuild  | unlimited |          15.91 |        0.57 |        0.65 |      17.12 |  58
refit    | unlimited |           0.93 |        0.57 |        0.59 |       2.09 | 478
static   | unlimited |           0.00 |        0.00 |        0.63 |       0.63 | 1584
```
- `render_rebuild.bmp` / `render_refit.bmp`: correctly shaded bunny.
- `render_static.bmp`: black patches and missing geometry — BVH/geometry divergence after 90° of rotation.

### Core Concept: Rebuild vs Refit

**Rebuild** re-runs the full SAH pipeline each frame — centroid sort, `nth_element`, `hit_link`/`miss_link` patching — O(N log N). Correct BVH quality, highest cost.

**Refit** skips sorting entirely. It walks the flat `BvhNode[]` in reverse index order (leaves before parents, guaranteed by depth-first pre-order) and re-expands each AABB from its children. O(N). ~17× faster than rebuild on this scene, with negligible quality loss for rigid rotation.

**Static** never touches the BVH. GPU traversal tests stale AABBs against moved triangles — rays skip subtrees whose AABBs no longer enclose the actual geometry. This is the failure mode every production engine must avoid.

### `--max-depth` flag
```bash
# Unlimited depth: ~40k nodes, fast traversal
./build/b3_ray_tracer_dynamic --strategy rebuild --max-depth 0 --frames 10 --scene ../../../../assets/bunny.obj

# Depth 1: 3 nodes, ~35k triangles per leaf — near brute-force
./build/b3_ray_tracer_dynamic --strategy rebuild --max-depth 1 --frames 10 --scene ../../../../assets/bunny.obj
```
Render time jumps from ~0.6ms (unlimited) to ~85ms (depth 1) — the BVH acceleration benefit made directly observable with one flag.

### Mini-challenge
Run `--strategy refit --max-depth 1` (3-node tree). Does refit still complete in under 1ms? What does this tell you about where refit's cost comes from?

---

## B4_Device_Enqueue — GPU Spawning GPU Work (Advanced)

**Goal**: Use OpenCL 2.0 Device Enqueue (`enqueue_kernel`) to spawn secondary ray kernels (reflections, refractions) from within the primary ray kernel — no CPU round-trip between bounces.

> **Level**: Advanced. Requires OpenCL 2.0+ runtime. Check: `clinfo | grep "Device OpenCL C"` — must show `2.0` or higher.

### Build & run
```bash
cd B4_Device_Enqueue
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/b4_device_enqueue --scene ../../../assets/cornell_box.obj
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

**Compatibility note**: Nvidia's OpenCL 2.0 support is incomplete — Device Enqueue may not work on Nvidia hardware. Verified on AMD (ROCm). Intel NEO (Iris Xe) reports OpenCL C 2.0 but does not implement Device Enqueue (`CL_DEVICE_QUEUE_ON_DEVICE_PROPERTIES == 0`); the binary detects this at runtime and exits gracefully. This is one of OpenCL 2.0's most compelling features; CUDA had equivalent capability earlier (Dynamic Parallelism, CUDA 5.0 / 2012), but `enqueue_kernel` is vendor-neutral.

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
| Advanced Ray Tracer (B3) | Render time | 60 FPS @ bunny.obj (~70k triangles), 1920×1080 |
| B3 Dynamic Scene | refit BVH build time | Measurably less than rebuild (reference: ~1ms vs ~16ms) |
| B3 Dynamic Scene | `--max-depth 1` render time | Measurably higher than `--max-depth 0` (reference: ~85ms vs ~0.6ms) |

**Measure with `cl::Event` profiling** on the kernel, not total frame time. The upload (BVH buffer) is a one-time cost — exclude it from the per-frame measurement.

> **Tip**: Profiling requires `CL_QUEUE_PROFILING_ENABLE` at queue creation — without it, timestamps are zero. Pass this flag when constructing `cl::CommandQueue`: `cl::CommandQueue(ctx, device, CL_QUEUE_PROFILING_ENABLE)`.

**If you're under 60 FPS**: profile with `cl::Event` on the traversal kernel and identify which stage dominates — traversal, triangle intersection, or memory reads. Then consult the [Optimization Toolbox](../../99_Toolbox/Toolbox.md) for the technique that matches your bottleneck.

---

## Troubleshooting

- **OpenGL interop on Optimus/hybrid GPU laptops**: GLFW creates a GL context on the iGPU (drives the display); the NVIDIA OpenCL driver can only share with a GL context it owns. Force GLFW onto the NVIDIA GPU with PRIME render offload:
  ```bash
  __NV_PRIME_RENDER_OFFLOAD=1 __GLX_VENDOR_LIBRARY_NAME=nvidia GPU=NVIDIA ./build/b2_ray_tracer --live
  ```
- **OpenGL interop init fails**: Verify `cl_khr_gl_sharing` extension: `clinfo | grep gl_sharing`. Not available on all CPU-fallback runtimes (PoCL).
- **BVH renders black patches**: Miss-link pointers are wrong — draw the BVH tree to a file and verify parent-child-sibling linkage before running on GPU.
- **`enqueue_kernel` returns `CL_INVALID_OPERATION` (B4)**: Your runtime does not support Device Enqueue. `clinfo | grep "Device OpenCL C"` showing `2.0+` is necessary but not sufficient — Intel NEO (Iris Xe) reports 2.0+ but has `CL_DEVICE_QUEUE_ON_DEVICE_PROPERTIES == 0`. AMD ROCm supports Device Enqueue; Nvidia and Intel NEO typically do not.
- **CLBlast not found (B1)**: CMake FetchContent downloads it at configure time — requires internet access. Offline: set `-DCMAKE_PREFIX_PATH=/path/to/clblast/install`.
- **Wrong GPU**: `GPU=NVIDIA ./build/ray_tracer_bvh`, `GPU=AMD ./build/ray_tracer_bvh`, `GPU=INTEL ./build/ray_tracer_bvh`.

---

## What's Next

[Track A: Multimedia](../A_Multimedia/Multimedia.md) — zero-copy video pipelines and edge AI inference.

[Track C: Robotics/ROS 2](../C_Robotics_ROS2/RoboticsROS2.md) — GPU acceleration inside a ROS 2 node, Lidar perception.

[Optimization Toolbox](../../99_Toolbox/Toolbox.md) — Thread Divergence, Work-Group Sizing, Local Memory, Async Pipelines.

[Module 4 Add-ons](../../04_Addons/Addons.md) — 3D Voxel Mapping (grand finale combining B ray casting with C Lidar data).
