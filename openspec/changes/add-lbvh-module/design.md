## Context

`01_Ray_Tracer_Basic`–`03_Ray_Tracer_BVH_Dynamic` established a GPU ray tracer where the BVH is always built on the CPU and uploaded as a flat array each frame. `03_Ray_Tracer_BVH_Dynamic`'s fastest strategy (CPU SAH refit, ~0.9 ms) still stalls the GPU command queue while the host finishes the build. For scenes with arbitrary topology changes, refit is invalid and full rebuild (~16 ms) is required. `04_Ray_Tracer_LBVH` eliminates the CPU stall entirely by building the BVH inside a single GPU dispatch chain.

Constraints inherited from project standards: OpenCL 1.2 baseline, no device-side enqueue, `cl.hpp` RAII wrappers, CLI11, SoA triangle layout, `cl::Event` profiling mandatory.

## Goals / Non-Goals

**Goals:**
- Complete 6-kernel GPU BVH build pipeline with no host-side computation between build and render.
- Per-frame LBVH rebuild faster than `03_Ray_Tracer_BVH_Dynamic`'s CPU SAH rebuild (~16 ms), competitive with refit (~0.9 ms).
- Teach: radix sort (histogram + prefix scan + scatter), Karras 2012 parallel tree construction, parallel AABB fitting via IEEE 754 float atomics.
- README timing table comparing CPU SAH rebuild / refit against GPU LBVH total frame time.

**Non-Goals:**
- SAH tree quality parity — LBVH is Morton-order, render time may be slightly higher than `02_Ray_Tracer_BVH`/`03_Ray_Tracer_BVH_Dynamic` SAH traversal.
- GL interop live window — `--live` flag may be added if time allows; headless `--output` is the primary mode.
- Deduplication with `02_Ray_Tracer_BVH`/`03_Ray_Tracer_BVH_Dynamic` traversal kernels — `04_Ray_Tracer_LBVH` uses its own stack-based kernel; no shared kernel files.
- 64-bit Morton codes — OpenCL 1.2 `ulong` is optional on some devices; 30-bit codes (10 bits/axis) are sufficient for bunny.obj.

## Decisions

### D1: Radix sort over bitonic sort

**Chosen**: 4-pass LSD radix sort, 8 bits/pass — histogram kernel, Blelloch prefix scan, scatter kernel.

**Rationale**: Teaches three fundamental GPU patterns (histogram, scan, scatter) that reappear in many GPU algorithms. O(N) vs O(N log²N) bitonic. For 70 k triangles, radix ≈ 0.3 ms vs bitonic ≈ 2–4 ms.

**Alternative considered**: Single-kernel bitonic sort — simpler, no prefix scan complexity, but slower and less educational.

### D2: Karras 2012 for parallel tree construction

**Chosen**: Each internal node i (0 … N-2) is assigned to a thread. The thread determines its coverage range [left, right] using the longest-common-prefix (LCP) of Morton codes, then finds the split position within that range. Parent pointers are written atomically after both children update theirs.

**Delta function tie-breaking**: When `morton[i] == morton[j]`, `delta(i,j) = clz(i ^ j) + 32`. This guarantees strict ordering when codes collide (benign for bunny.obj; documented caveat for dense/coplanar meshes).

**Rationale**: O(N) parallel construction with no serialisation. The only published O(N) parallel BVH build algorithm suitable for OpenCL 1.2 (no recursion, no device-side enqueue).

### D3: Parallel AABB fitting via atomic visit counters

**Chosen**: Each leaf thread initialises its node AABB and atomically increments a `visited[]` counter on its parent. The first thread to arrive (counter == 1) exits — its sibling hasn't run yet. The second thread (counter == 2) reads both children's AABBs, unions them, writes the parent, and propagates upward. Repeats until the root.

**Float atomics**: OpenCL 1.2 has no native float atomics. IEEE 754 guarantees that for non-negative floats, the bit representation is monotonically ordered. `atomic_min`/`atomic_max` for non-negative floats are implemented as `atomic_cmpxchg` spin loops on the integer reinterpretation.

**Alternative considered**: CPU AABB fitting after GPU topology build (hybrid approach, Option B) — simpler but reintroduces a CPU stall, defeating the module goal.

### D4: Stack-based traversal with local memory

**Chosen**: Each work-item maintains a 32-entry `int` stack in local memory (`__local int stack[WG_SIZE][32]`). Stack depth 32 covers trees up to depth 31 (bunny.obj SAH depth ≈ 16; LBVH depth ≈ log₂(70 k) ≈ 17).

**Rationale**: Karras outputs parent/child pointers, not hit/miss links. Stack-based traversal requires no post-processing conversion kernel and is a natural complement to the stack-based build. Hit/miss link conversion (Option A path) would add a 7th kernel and duplicate `02_Ray_Tracer_BVH`/`03_Ray_Tracer_BVH_Dynamic` traversal logic without adding educational value.

**Alternative considered**: Convert to hit/miss links (Path A) to reuse `02_Ray_Tracer_BVH`/`03_Ray_Tracer_BVH_Dynamic` traversal kernel — avoids new traversal kernel, but requires a conversion kernel and makes the module dependent on their kernel format.

### D5: Node buffer layout

Internal nodes and leaf nodes share one flat `LbvhNode[]` array (N−1 internal + N leaf nodes = 2N−1 total). Layout:

```
indices [0 … N-2]   : internal nodes  (Karras convention)
indices [N-1 … 2N-2]: leaf nodes
```

Leaf node i stores triangle index `i` (after radix sort reordering). Internal node i stores `left_child`, `right_child`, `parent` as `int`, and `aabb_lo[3]` / `aabb_hi[3]` as `float`.

## Risks / Trade-offs

- **Radix sort correctness** → Prefix scan off-by-one errors are common. Mitigated by: CPU reference sort self-test in `main.cpp` before first GPU sort dispatch.
- **Karras LCP range detection** → Boundary conditions at i=0 and i=N-2 are subtle. Mitigated by: small-scene GPU self-test (8 triangles, known Morton codes, verified tree structure).
- **Float atomic correctness on AMD/PoCL** → `atomic_cmpxchg` spin loops may stall on drivers with weak memory model guarantees. Mitigated by: `barrier(CLK_GLOBAL_MEM_FENCE)` between AABB propagation passes if single-pass atomics produce incorrect results on a given device.
- **Stack overflow for LBVH deep trees** → LBVH can produce unbalanced trees (depth > 32) for adversarial Morton code distributions. Mitigated by: depth is bounded at log₂(N) for uniformly distributed centroids; document as a known limitation for degenerate inputs.
- **LBVH render quality** → Looser AABBs than SAH → more nodes traversed → slightly higher render time. Expected and documented; total frame time (build + render) is the comparison metric.

## Open Questions

*(none — all design decisions resolved during exploration session)*
