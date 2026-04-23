# B.4 — Ray Tracer LBVH: Full GPU BVH Pipeline

**Goal**: Eliminate the CPU from the BVH pipeline entirely. Six OpenCL 1.2 kernels replace the CPU build path: scene-bounds reduction, Morton code generation, 4-pass radix sort, Karras 2012 tree construction, parallel AABB fitting, and stack-based ray traversal. No CPU round-trip between build and render.

> **Headless only.** GL interop live window is out of scope for this module. All output is via `--output render.bmp`.

## Prerequisites (delta from B.3)

- Complete [B3 Ray Tracer BVH Dynamic](../03_Ray_Tracer_BVH_Dynamic/RayTracerBVHDynamic.md) first. B4 builds on all three prior ray tracer modules.
- `assets/bunny.obj` (Stanford Bunny, ~70k triangles) — included in the repository.
- No additional system packages required beyond B3.

## Build & Run

```bash
cd 04_Ray_Tracer_LBVH
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build

# Benchmark: 60-frame average, 800×600
./build/ray_tracer_lbvh --scene assets/bunny.obj --output render.bmp --frames 60
```

## Verify

After a successful run the binary prints a timing table and saves `render.bmp`. Expected output (Intel Iris Xe):

```
Strategy | BVH Build (ms) | Upload (ms) | Render (ms) | Total (ms) | FPS
---------|----------------|-------------|-------------|------------|----
GPU LBVH |          2.344 |       0.000 |       5.774 |      8.118 | 123

Per-stage breakdown (last frame):
  scene_bounds : 0.089 ms
  morton       : 0.034 ms
  radix_sort   : 1.429 ms
  lbvh_build   : 0.172 ms
  lbvh_aabb    : 0.591 ms
  traverse     : 5.774 ms (avg)

Framebuffer stats: mean=0.7175 variance=0.0229
```

- **BVH Build ≤ 17 ms** (B3 CPU SAH rebuild baseline). Observed: ~2.3 ms. ✓
- **Total ≤ 17 ms** (GPU LBVH build + render). Observed: ~8.1 ms. ✓
- Open `render.bmp` — the Stanford Bunny should be clearly recognisable with Phong shading.

### Self-tests (run automatically on first frame)

```
Radix sort self-test PASSED (69451 elements)
Morton bit interleaving self-test PASSED
Karras tree self-test PASSED (69450 internal nodes verified)
Root AABB self-test PASSED
Traversal self-test PASSED (traversal reached geometry)
```

## Key Concepts

### Morton Codes

A 30-bit Morton code interleaves 10 bits from each of X, Y, Z coordinates. Nearby points in 3D space get nearby Morton codes, so sorting by Morton code produces a spatially coherent ordering — the foundation of LBVH.

```
expand_bits(v): insert two 0-bits between each bit of v
morton3d(x,y,z) = expand_bits(x) | (expand_bits(y)<<1) | (expand_bits(z)<<2)
```

Grid: 10 bits per axis → 1024³ resolution. Sufficient for ~70k triangles; uses `uint` (safe on all OpenCL 1.2 devices).

### 4-Pass LSD Radix Sort

Three kernels per pass, four passes (8 bits/pass):

| Kernel | Work | Educational pattern |
| :----- | :--- | :------------------ |
| `radix_histogram` | Count digit frequencies per work-group | Shared-memory reduction |
| `radix_prefix_scan` | Blelloch exclusive scan over 256 buckets | Parallel prefix sum |
| `radix_scatter` | Write (key, index) to sorted destination | Stable stable scatter with local rank |

**Why LSD (Least Significant Digit first):** each pass is O(N); 4 passes total → O(4N) vs O(N log²N) for bitonic sort. For 70k triangles: radix ≈ 1.4 ms vs bitonic ≈ 3–5 ms.

**Stability is required:** each pass must preserve the relative order of elements with identical digits, so prior passes' ordering is retained. The scatter kernel computes within-group rank via a serial scan over local memory — O(WG_SIZE) per work-item, O(WG_SIZE²) per group, but fast from on-chip local memory.

### Karras 2012 Parallel Tree Construction

One GPU thread per internal node (N−1 threads for N sorted leaves). Each thread:

1. `determine_range(i)` — finds the range `[left, right]` of sorted leaves covered by node `i`, using longest-common-prefix (LCP) of Morton codes.
2. `find_split(left, right)` — binary searches for the split point `γ` where the LCP changes.
3. Assigns `left_child` and `right_child` — leaf if range is length 1, else internal node.
4. Writes `parent` pointers to both children (no conflicts — each child has exactly one parent).

**Delta tie-break:** when `morton[i] == morton[j]`, use `clz(i ^ j) + 32` as the LCP. The `+32` ensures tie-broken values are strictly larger than any 30-bit XOR result, preventing ambiguity in range direction selection.

**Complexity:** O(N) parallel — no serialisation, no recursion, no device-side enqueue.

### Parallel AABB Fitting with Atomic Visit Counters

After topology is built, each leaf thread:
1. Computes its triangle AABB and stores it in the leaf node.
2. Atomically increments its parent's `visited[]` counter.
   - Counter == 1: exit (sibling not done yet).
   - Counter == 2: union both children's AABBs, write to parent, propagate upward.

Propagation continues until the second thread to reach each node writes the union and moves to the grandparent. Root AABB is complete when any thread reaches internal node 0.

**Float atomics:** OpenCL 1.2 has no native `float atomic_min/max`. Implemented via `atomic_cmpxchg` spin loop on reinterpreted `uint` bits — safe for any float value (positive or negative).

### Stack-Based Local Memory Traversal

The Karras tree uses `left_child`/`right_child` pointers (not hit/miss links). Traversal maintains a 32-entry per-work-item stack in local memory:

```c
__local int stack[WG_SIZE][STACK_DEPTH]
```

Push right child first, left second → left-first traversal order. Stack depth 32 covers LBVH trees up to depth 31; bunny.obj LBVH depth ≈ 17.

`WG_SIZE` and `STACK_DEPTH` are passed as `--wg-size` and `--stack-depth` CLI args (defaults 64 and 32). The host injects them as `-D` flags into `clBuildProgram` at startup — no rebuild required to tune.

## Mini-Challenge

1. **Vary `--frames`** to see how BVH build time amortises (it doesn't — each frame is a full rebuild). Compare with B3's refit.
2. **Change `--wg-size`** (try 32, 64, 128) and observe traversal time changes — no rebuild needed.
3. **Extend `--output` to `--frames 1`** and check single-frame latency.
4. **Add `--max-depth`** that limits BVH depth by skipping the sort for small subtrees — see how quality degrades.

## Troubleshooting

- **All self-tests pass but image is black:** check that `assets/bunny.obj` symlink resolves — verify `ls build/assets/bunny.obj`.
- **Radix sort self-test FAILED:** GPU sort output doesn't match `std::sort`. Driver bug or memory bandwidth contention. Try `GPU=AMD` or `GPU=NVIDIA`. The self-test catches this before rendering.
- **`std::runtime_error`: stack exceeds local memory:** pass `--wg-size 32` or `--stack-depth 16` to reduce the budget.
- **Traversal self-test completes but image has patches:** AABB propagation incomplete — check that `visited[]` buffer is zeroed before each build (it is in `lbvh_builder.hpp`).
- **Wrong GPU:** `GPU=NVIDIA ./build/ray_tracer_lbvh`.

## Common Gotchas

### `float3` Alignment Trap (ABI Mismatch)

OpenCL's built-in `float3` type occupies **16 bytes** (4-element alignment), but C++'s `struct { float x, y, z; }` occupies 12 bytes. A struct with `cl_float3 aabb_lo` would be 12 bytes on the host and 16 bytes on the device — silent mismatch, wrong renders.

**Fix:** Use `float aabb_lo[3]` plus an explicit `float aabb_lo_pad` field on both sides. `LbvhNode` is 48 bytes and verified by `static_assert(sizeof(LbvhNode) == 48)`.

### Degenerate Morton Codes

Many triangles with identical centroids produce identical 30-bit Morton codes. The `delta()` function's `clz(i ^ j) + 32` tie-break guarantees a valid unique ordering even with all-identical codes. Rendering quality degrades (LBVH becomes a path, depth = N) but correctness is preserved.

### Stack Overflow for Adversarial Inputs

LBVH depth can exceed `STACK_DEPTH=32` for pathological Morton code distributions (e.g., all centroids at the same point, or a highly skewed mesh). For uniformly-distributed scenes like bunny.obj (depth ≈ 17), depth 32 provides 2× headroom. Degenerate inputs are a known limitation; pass `--stack-depth 64` if needed.

### Radix Sort Stability

LSD radix sort requires a **stable** sort at each pass. The scatter kernel computes within-group rank by scanning local digits — not via `atomic_inc`, which would assign positions in execution order (unstable). This is O(WG²) per group but runs from local memory and is correct for all inputs.
