# B.3 — Ray Tracer BVH: Flagship Project

**Goal**: Extend the basic ray tracer with a Bounding Volume Hierarchy to render complex triangle scenes at 60 FPS — the default scene (`bunny.obj`, ~70k triangles) is already well beyond what brute-force intersection can handle interactively.

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

---

[Path B: Graphics & HPC](../GraphicsHPC.md)
