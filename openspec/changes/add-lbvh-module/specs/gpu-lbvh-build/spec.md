## ADDED Requirements

### Requirement: Scene AABB parallel reduction
The GPU pipeline SHALL compute the axis-aligned bounding box of all triangle centroids via a parallel reduction kernel before Morton code assignment. The result SHALL be stored in a device buffer and used to normalize centroids to [0, 1]³ before Morton encoding.

#### Scenario: Scene AABB encloses all centroids
- **WHEN** scene_bounds kernel runs on N triangles
- **THEN** every triangle centroid lies within [aabb_lo, aabb_hi] with no exceptions

#### Scenario: Degenerate flat scene
- **WHEN** all centroids share the same coordinate on one axis
- **THEN** that axis extent is treated as non-zero (epsilon guard) to avoid division by zero in normalization

---

### Requirement: Morton code computation
The kernel SHALL compute a 30-bit Morton code (10 bits per axis) for each triangle centroid after normalization to [0, 1]³. The 30-bit code SHALL be packed into a `uint` (upper 2 bits zero).

#### Scenario: Correct bit interleaving
- **WHEN** Morton codes are computed for two centroids differing only on the X axis
- **THEN** their codes differ only in the X-interleaved bits, preserving spatial locality

#### Scenario: Tie-breaking index appended
- **WHEN** two or more triangles produce identical 30-bit Morton codes
- **THEN** the Karras delta function uses `clz(i ^ j) + 32` as the fallback LCP, producing a valid non-ambiguous ordering without modifying the stored Morton codes

---

### Requirement: Karras 2012 parallel tree construction
The GPU SHALL construct a binary radix tree over the sorted Morton code array using the Karras 2012 algorithm. Construction SHALL assign one thread per internal node (N−1 threads for N leaves). Each internal node SHALL record `left_child`, `right_child`, and `parent` indices.

#### Scenario: Root node coverage
- **WHEN** tree build completes
- **THEN** internal node 0 covers the full range [0, N−1] of sorted leaf indices

#### Scenario: Every leaf has exactly one parent
- **WHEN** tree build completes
- **THEN** each of the N leaf nodes is referenced as a child by exactly one internal node

#### Scenario: Range boundaries
- **WHEN** the split point for internal node i lands at the first or last valid index
- **THEN** one child is a leaf node and one is an internal node (no zero-range children)

---

### Requirement: Parallel AABB fitting with atomic visit counters
The GPU SHALL compute node AABBs bottom-up using per-node atomic visit counters. Each leaf thread initialises its own AABB and increments its parent's counter. The second thread to arrive at a node SHALL union both children's AABBs, update the node, and propagate upward. IEEE 754 float min/max SHALL be implemented via `atomic_cmpxchg` on reinterpreted `uint` values (valid for non-negative floats only).

#### Scenario: Root AABB encloses all triangles
- **WHEN** AABB fitting completes
- **THEN** internal node 0's AABB contains every triangle vertex in the scene

#### Scenario: Leaf AABB correctness
- **WHEN** AABB fitting completes
- **THEN** each leaf node's AABB is the tight bounding box of its assigned triangle

#### Scenario: No CPU round-trip during fitting
- **WHEN** AABB fitting kernel is enqueued
- **THEN** no `clEnqueueReadBuffer` or host-side computation occurs before the fitting kernel completes
