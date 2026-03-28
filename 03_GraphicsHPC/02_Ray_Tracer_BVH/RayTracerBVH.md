# B.2 — Ray Tracer BVH: Flagship Project

**Goal**: Extend the basic ray tracer with a Bounding Volume Hierarchy to render complex triangle scenes at 60 FPS — the default scene (`bunny.obj`, ~70k triangles) is already well beyond what brute-force intersection can handle interactively.

> **Requires:** `cl_khr_gl_sharing` extension for the live OpenGL window. Headless BMP output works without the extension. Check availability: `clinfo | grep gl_sharing`.

## Prerequisites (delta from module index)

- `assets/bunny.obj` (Stanford Bunny, ~70k triangles) — included in the repository.
- tinyobjloader: fetched automatically by CMake at configure time. Offline builds: `-DCMAKE_PREFIX_PATH=/path/to/tinyobjloader/install`.

## Build & Run

```bash
cd 02_Ray_Tracer_BVH
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/ray_tracer_bvh --scene assets/bunny.obj --width 1920 --height 1080
# Live window:
./build/ray_tracer_bvh --scene assets/bunny.obj --live
# Headless output:
./build/ray_tracer_bvh --scene assets/bunny.obj --output render.bmp --frames 10
# GPU=NVIDIA ./build/ray_tracer_bvh --scene assets/bunny.obj
```

## Verify

Scene renders with no missing triangles and no black artifacts. Console prints traversal statistics:

```
Scene: bunny.obj (~70k triangles), BVH depth: 16
[NAIVE ] Render time:  2180.0 ms   (0.5 FPS)
[BVH   ] Render time:    14.8 ms   (67.6 FPS)  ← must be ≥ 60 FPS to pass
BVH speedup: 147x
```

The BVH gate passes at ≥ 60 FPS measured via `cl::Event` on the traversal kernel.

## Key Concepts

### Why Stackless BVH

Standard BVH traversal is recursive — a ray hits a node, recurses into children, returns, continues. Recursion requires a call stack. GPU threads have no stack.

The solution: each BVH node stores two precomputed links set on the CPU during build. `hit_link` points to the left child (follow when the ray hits the bounding box). `miss_link` points to the right sibling or the parent's right sibling (follow when the ray misses). The GPU kernel only reads them:

```cl
// Stackless traversal — no function-call stack required on the GPU
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

### BVH Build on CPU (SAH)

For a deeper treatment of SAH and BVH construction, see PBRT §4.3 (free online at [pbr-book.org](https://www.pbr-book.org/3ed-2018/Primitives_and_Intersection_Acceleration/Bounding_Volume_Hierarchies)) — it covers the same algorithm used here.

SAH (Surface Area Heuristic) estimates split cost by weighting the probability of a ray hitting a child node by its surface area. The BVH is built on the CPU once and uploaded as a flat array. The kernel only traverses — it never modifies the structure:

```
CPU: build SAH-BVH → flatten to array → cl::Buffer upload (once at load)
GPU: per-ray stackless traversal (every frame, read-only)
```

### Thread Divergence

Rays in the same warp follow different tree paths through the BVH. This is inevitable — the profiler will show it after you hit the gate. See [Toolbox: Thread Divergence](../../05_Toolbox/13_Thread_Divergence/ThreadDivergence.md) for mitigation strategies.

## Mini-Challenge

Visualize BVH depth per pixel: color each pixel by the number of nodes the ray visited (0 = blue, max depth = red). The resulting heatmap shows where the tree is unbalanced — red regions are candidates for deeper splitting.

## Troubleshooting

- **Under 60 FPS**: profile with `cl::Event` on the traversal kernel and identify which stage dominates — traversal, triangle intersection, or memory reads. Then see [Optimization Toolbox](../../05_Toolbox/Toolbox.md).
- **Wrong GPU**: `GPU=NVIDIA ./build/ray_tracer_bvh` or `GPU=AMD ./build/ray_tracer_bvh`.
- **tinyobjloader not found at configure time**: requires network access. Offline: `-DCMAKE_PREFIX_PATH=/path/to/tinyobjloader`.

## Common Gotchas

### The float3 Alignment Trap

OpenCL aligns `float3` to **16 bytes** — the same as `float4`. A struct with a `float3` member
therefore contains an invisible 4-byte padding hole after it:

```c
// Host C++ struct — appears to be 12 bytes, is actually 16
typedef struct { float x, y, z; } Ray;  // + 4 bytes silent padding
```

This matters because a `float3` array on the host (`std::vector<cl_float3>`) lays out elements
at 16-byte strides, not 12. If you pack ray data as `float x, y, z` with no padding field, the
host and device see different memory layouts — producing corrupted ray directions with zero
symptoms at launch.

**Rule:** Either use `float4` (explicit `w = 0`) or add an explicit `float pad` field and verify
with `static_assert(sizeof(Ray) == 16, "Ray struct ABI mismatch")`.

**AMD-specific reality:** On some AMD drivers, calling `normalize()` on a zero-length `float3`
(e.g., a miss ray hitting the background) silently produces `NaN` components rather than an
implementation-defined result. This causes `NaN` to propagate through shading and surface as
black or corrupted pixels. Defensive fix: replace `dot(a, b)` with an explicit
`dot3(a, b) = a.x*b.x + a.y*b.y + a.z*b.z` helper for `float3` operands, and guard
`normalize()` calls with a length check.

---

[Path B: Graphics & HPC](../GraphicsHPC.md)
