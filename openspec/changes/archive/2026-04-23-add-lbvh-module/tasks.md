## 1. Module Scaffold

- [x] 1.1 Create `03_GraphicsHPC/04_Ray_Tracer_LBVH/` directory with `kernels/` subdirectory
- [x] 1.2 Write `CMakeLists.txt`: `project(RayTracerLBVH)`, FetchContent for tinyobjloader, include `common/common.cmake`, `add_executable(ray_tracer_lbvh main.cpp)`, link CLI11, symlink kernels post-build
- [x] 1.3 Verify `cmake -B build && cmake --build build` compiles with zero warnings (main.cpp stub with empty `main()` is sufficient at this stage)

## 2. Shared Types and Host Utilities

- [x] 2.1 Write `lbvh_types.hpp`: `LbvhNode` struct (`left_child`, `right_child`, `parent`, `aabb_lo[3]`, `aabb_hi[3]` as `cl_int`/`cl_float`), `LbvhTree` (flat node buffer, sorted index buffer)
- [x] 2.2 Add `static_assert` on `LbvhNode` size to catch silent ABI padding (document `float3` alignment trap, matching `02_Ray_Tracer_BVH` Common Gotchas)
- [x] 2.3 Write `lbvh_builder.hpp`: class `LbvhBuilder` — constructor takes `cl::Context`, `cl::Device`, `cl::CommandQueue`; loads and builds all 6 kernels from `kernels/`; exposes `build(triangles, num_tris)` returning timing breakdown struct
- [x] 2.4 Use `TriangleCpu`, `TriangleSoa`, `build_triangle_soa`, `sub3`, `cross3`, `dot3`, `safe_rcp`, `moller_trumbore` from `common/bvh_utils.hpp` (do not copy). Use `save_framebuffer` from `common/graphics_hpc_utils.hpp`.

## 3. Kernel: Scene Bounds Reduction (`scene_bounds.cl`)

- [x] 3.1 Implement parallel reduction kernel: each work-group reduces its tile to a local min/max, writes to a partial-results buffer; second pass reduces partials to a single `float6` result (aabb_lo + aabb_hi)
- [x] 3.2 Guard against degenerate axis extents: add epsilon to any axis where `hi == lo` before writing to device buffer
- [x] 3.3 Wrap `enqueueNDRangeKernel` and `setArg` calls in `CL_CHECK`

## 4. Kernel: Morton Code Computation (`morton.cl`)

- [x] 4.1 Implement `expand_bits()` helper (interleave 10-bit integer into 30-bit Morton component) — one thread per triangle, reads centroid, normalises to [0,1]³ using scene AABB buffer
- [x] 4.2 Output two device buffers: `uint morton_codes[N]` and `uint indices[N]` (initialised to 0…N-1; indices travel with codes through sort)
- [x] 4.3 Verify bit interleaving with a CPU unit test for a known centroid (e.g. centroid at scene centre → Morton code 0x1B6DB6DB for a uniform grid)

## 5. Kernels: Radix Sort (`radix_sort.cl`)

- [x] 5.1 Implement histogram kernel: each work-group tallies digit-bucket frequencies for its tile, writes to `histogram[256 × num_groups]`; parameterise active byte via a kernel constant
- [x] 5.2 Implement Blelloch prefix-scan kernel over the 256-bucket global histogram: produces exclusive prefix sums used as scatter base offsets
- [x] 5.3 Implement scatter kernel: each thread reads (key, index) pair, computes destination from prefix-scan offset, writes to output buffers
- [x] 5.4 Implement host-side 4-pass dispatch loop in `LbvhBuilder::sort()`: swap input/output ping-pong buffers each pass
- [x] 5.5 Add CPU reference self-test (spec requirement): sort the same input with `std::sort`, compare with GPU result after pass 4; throw `std::runtime_error` on mismatch

## 6. Kernel: Karras 2012 Tree Build (`lbvh_build.cl`)

- [x] 6.1 Implement `delta(i, j)` device function: `clz(morton[i] ^ morton[j])` when codes differ, `clz(i ^ j) + 32` tie-break when identical; handle out-of-range indices (`j < 0 || j >= N`) by returning -1
- [x] 6.2 Implement `determine_range(i)` — find the extent [left, right] of the minimal range covered by internal node i using sign of `delta(i, i+1) - delta(i, i-1)`
- [x] 6.3 Implement `find_split(left, right)` — binary search for Morton LCP split point within [left, right]
- [x] 6.4 Write parent, left_child, right_child fields to `LbvhNode` array; leaf nodes occupy indices `[N-1 … 2N-2]`
- [x] 6.5 Add small-scene GPU self-test (8 triangles, known Morton codes): verify tree structure matches expected parent/child relationships

## 7. Kernel: Parallel AABB Fitting (`lbvh_aabb.cl`)

- [x] 7.1 Allocate `visited[N-1]` device buffer (one `int` per internal node), zeroed before each build
- [x] 7.2 Implement leaf initialisation pass: each thread (one per leaf) computes triangle AABB, stores in its leaf node, atomically increments parent's `visited` counter
- [x] 7.3 Implement propagation loop: while parent counter reaches 2, thread unions both children AABBs, writes to parent node, increments grandparent counter; loop until root is processed or thread exits
- [x] 7.4 Implement float min/max atomics via `atomic_cmpxchg` + IEEE 754 bit reinterpretation; comment WHY bit trick is valid for non-negative floats
- [x] 7.5 Verify root AABB (internal node 0) encloses all triangle vertices — add assertion in CPU self-test path

## 8. Kernel: Stack-Based Traversal (`traverse.cl`)

- [x] 8.1 Declare `__local int stack[WG_SIZE][STACK_DEPTH]`; expose `--wg-size` (default 64) and `--stack-depth` (default 32) as CLI11 args; pass values to `LbvhBuilder` constructor which injects them as `-DWG_SIZE=N -DSTACK_DEPTH=N` into `clBuildProgram`; remove `target_compile_definitions` from CMakeLists.txt
- [x] 8.2 Implement traversal loop: push root, pop node, test AABB; if hit and internal → push both children (right first, left second for left-first traversal order); if hit and leaf → test triangle with Möller-Trumbore
- [x] 8.3 Add host-side local memory budget check: query `CL_DEVICE_LOCAL_MEM_SIZE`, compute `WG_SIZE × STACK_DEPTH × 4`, throw `std::runtime_error` if exceeded
- [x] 8.4 Verify traversal output on a 3-triangle test scene matches CPU reference intersection (port `02_Ray_Tracer_BVH`/`03_Ray_Tracer_BVH_Dynamic` `bvh_self_test` logic to use LBVH tree)

## 9. Host Integration (`main.cpp`)

- [x] 9.1 Wire CLI11 flags: `--scene`, `--output`, `--frames`, `--width`, `--height`, `--wg-size`, `--stack-depth`; `--live` intentionally omitted (GL interop out of scope)
- [x] 9.2 Load OBJ via tinyobjloader, build `TriangleSoa`, allocate SoA device buffers
- [x] 9.3 Per-frame dispatch loop: enqueue all 6 kernels with `cl::Event` profiling; accumulate timing into a `TimingStats` struct
- [x] 9.4 Print timing table after `--frames` iterations:
  ```
  Strategy   | BVH Build (ms) | Upload (ms) | Render (ms) | Total (ms) | FPS
  GPU LBVH   |          X.XXX |       X.XXX |       X.XXX |      X.XXX | XX
  ```
- [x] 9.5 Write BMP output using `common/image_utils.hpp` or stb_image_write
- [x] 9.6 After `--frames` completes, compute mean and variance of the final framebuffer (RGBA, channel-wise); throw `std::runtime_error` if mean < 0.01 (all black — broken traversal), mean > 0.99 (all white — shading overflow), or variance < 0.001 (flat colour — geometry not reached)

## 10. Documentation

- [x] 10.1 Write `RayTracerLBVH.md` following `02_Ray_Tracer_BVH`/`03_Ray_Tracer_BVH_Dynamic` format: Goal callout, Prerequisites delta, Build & Run, Verify (with reference timing table from actual run), Key Concepts (Morton codes, radix sort, Karras 2012, parallel AABB atomics, local memory stack), Mini-Challenge, Troubleshooting, Common Gotchas (float3 padding, degenerate Morton codes, stack overflow)
- [x] 10.2 Update `GraphicsHPC.md`: add `04_Ray_Tracer_LBVH` row to Contents table and Performance Gates table (`GPU LBVH total frame time ≤ 03_Ray_Tracer_BVH_Dynamic CPU SAH rebuild total frame time`)

## 11. Definition of Done Verification

- [x] 11.1 `cmake -B build && cmake --build build` succeeds with zero errors and zero warnings
- [x] 11.2 `./build/ray_tracer_lbvh --scene assets/bunny.obj --output render.bmp --frames 60` exits zero and pixel sanity checks pass (task 9.6); **manual**: open `render.bmp` and confirm the Stanford Bunny is recognisably rendered
- [x] 11.3 `--help` prints CLI11-generated usage with all defined flags
- [x] 11.4 `GPU=<vendor> ./build/ray_tracer_lbvh --scene assets/bunny.obj --output render.bmp --frames 1` runs correctly on the available device
- [x] 11.5 Timing table shows GPU LBVH total frame time ≤ `03_Ray_Tracer_BVH_Dynamic` CPU SAH rebuild total (~17 ms); if not, try `GPU=AMD` and note results in README (hardware waiver clause)
