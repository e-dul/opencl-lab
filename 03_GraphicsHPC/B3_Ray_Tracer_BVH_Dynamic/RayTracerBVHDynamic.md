# B.3 Dynamic — Ray Tracer BVH: Dynamic Scene Challenge

**Goal**: Animate the scene (rigid Y-axis rotation each frame) and benchmark three per-frame BVH strategies side-by-side — full rebuild, AABB refit, and static (stale BVH) — to make the rebuild vs refit cost difference concrete and observable.

## Prerequisites (delta from module index)

- Complete [B3 Ray Tracer BVH](../B3_Ray_Tracer_BVH/RayTracerBVH.md) first. This module builds directly on those concepts.
- `assets/bunny.obj` (Stanford Bunny, ~70k triangles) — included in the repository.

## Build & Run

Valid `--strategy` values: `rebuild`, `refit`, `static`.

```bash
cd B3_Ray_Tracer_BVH_Dynamic
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build

# Full SAH rebuild every frame
./build/b3_ray_tracer_dynamic --strategy rebuild --scene assets/bunny.obj --frames 60 --output render_rebuild.bmp

# Bottom-up AABB refit (topology unchanged)
./build/b3_ray_tracer_dynamic --strategy refit --scene assets/bunny.obj --frames 60 --output render_refit.bmp

# Stale BVH — geometry moves, BVH does not (intentional artifacts)
./build/b3_ray_tracer_dynamic --strategy static --scene assets/bunny.obj --frames 60 --output render_static.bmp
```

## Verify

Each run prints a one-row timing table. Reference numbers on NVIDIA RTX 4060 Laptop at 800×600, `bunny.obj` (~70k triangles), 60 frames:

```
Strategy | Depth     | BVH Build (ms) | Upload (ms) | Render (ms) | Total (ms) | FPS
---------|-----------|----------------|-------------|-------------|------------|----
rebuild  | unlimited |          15.91 |        0.57 |        0.65 |      17.12 |  58
refit    | unlimited |           0.93 |        0.57 |        0.59 |       2.09 | 478
static   | unlimited |           0.00 |        0.00 |        0.63 |       0.63 | 1584
```

- `render_rebuild.bmp` and `render_refit.bmp`: correctly shaded bunny, no artifacts.
- `render_static.bmp`: black patches and missing geometry — BVH/geometry divergence accumulates after ~90° of rotation.

## Key Concepts

### Rebuild vs Refit

**Rebuild** re-runs the full SAH pipeline each frame — centroid sort, `nth_element`, `hit_link`/`miss_link` patching. O(N log N). Optimal BVH quality, highest cost.

**Refit** skips sorting entirely. It walks the flat `BvhNode[]` in reverse index order (leaves before parents, guaranteed by depth-first pre-order) and re-expands each AABB from its children. O(N). ~17× faster than rebuild on this scene, with negligible quality loss for rigid rotation.

**Static** never touches the BVH. GPU traversal tests stale AABBs against moved triangles — rays skip subtrees whose AABBs no longer enclose the actual geometry. `render_static.bmp` shows exactly what this failure mode looks like in practice.

### `--max-depth` and BVH Acceleration

Use `--max-depth` to observe the BVH benefit directly:

```bash
# Unlimited depth: ~40k nodes, fast traversal
./build/b3_ray_tracer_dynamic --strategy rebuild --max-depth 0 --frames 10 --scene assets/bunny.obj

# Depth 1: 3 nodes, ~35k triangles per leaf — near brute-force
./build/b3_ray_tracer_dynamic --strategy rebuild --max-depth 1 --frames 10 --scene assets/bunny.obj
```

Render time jumps from ~0.6 ms (unlimited) to ~85 ms (depth 1). The BVH acceleration benefit is directly observable with one flag change.

## Mini-Challenge

Run `--strategy refit --max-depth 1` (3-node tree). Does refit still complete in under 1 ms? What does the answer tell you about where refit's cost comes from — the AABB expansion work, or the tree traversal to find which nodes to update?

## Troubleshooting

- **Black patches on rebuild/refit**: `miss_link` pointers are stale from a previous topology. Verify that refit only updates AABBs and never rewrites link pointers.
- **`--strategy static` renders correctly for the first few frames**: expected. BVH divergence accumulates gradually — run at least 60 frames to see the artifact clearly.

---

[Path B: Graphics & HPC](../GraphicsHPC.md)
