# Task 032: B3 Challenge — Dynamic Scene (BVH Rebuild vs Refit)

## Context
- **Design Feature:** `workflow/design/05-graphics-hpc-projects.md`
- **Milestone:** Phase 4 — B3 Challenge: Dynamic Scene
- **Relevant Files:**
  - `workflow/design/05-graphics-hpc-projects.md` — read-only: architecture, constraints, performance gates
  - `02_Projects/B_Graphics_HPC/B3_Ray_Tracer_BVH/main.cpp` — read-only source to copy forward
  - `02_Projects/B_Graphics_HPC/B3_Ray_Tracer_BVH/bvh_builder.hpp` — read-only source to copy forward
  - `02_Projects/B_Graphics_HPC/B3_Ray_Tracer_BVH/kernels/ray_trace_bvh.cl` — read-only source to copy forward
  - `02_Projects/B_Graphics_HPC/B3_Ray_Tracer_BVH/CMakeLists.txt` — read-only source to copy forward
  - `02_Projects/B_Graphics_HPC/B3_Ray_Tracer_BVH_Dynamic/` — NEW directory (all deliverables go here)

> **IMPORTANT — Snapshots over Branches (master_specs §2)**: All new code lives in
> `B3_Ray_Tracer_BVH_Dynamic/`. The source directory `B3_Ray_Tracer_BVH/` must **not** be
> modified. Mutating an existing directory in-place violates the project's snapshot rule.

## Objective

Create a new standalone directory `02_Projects/B_Graphics_HPC/B3_Ray_Tracer_BVH_Dynamic/` that
copies forward the BVH/kernel infrastructure from `B3_Ray_Tracer_BVH/` and extends it to animate
the scene (rigid rotation of the entire triangle mesh each frame). The extension compares three
per-frame BVH strategies — full SAH rebuild, AABB refit, and no update (static BVH on moving
geometry) — reporting GPU upload time and render time per strategy in a structured console table.

## Files to Copy Forward

The following files are copied verbatim (or near-verbatim) from `B3_Ray_Tracer_BVH/` into
`B3_Ray_Tracer_BVH_Dynamic/` as the starting point:

| Source file (read-only) | Destination file | Notes |
|---|---|---|
| `B3_Ray_Tracer_BVH/kernels/ray_trace_bvh.cl` | `B3_Ray_Tracer_BVH_Dynamic/kernels/ray_trace_bvh.cl` | Verbatim copy; no kernel changes required |
| `B3_Ray_Tracer_BVH/bvh_builder.hpp` | `B3_Ray_Tracer_BVH_Dynamic/bvh_builder.hpp` | Copy, then add `refit()` method |
| `B3_Ray_Tracer_BVH/main.cpp` | `B3_Ray_Tracer_BVH_Dynamic/main.cpp` | Copy, then add animation helper, strategy switch, benchmark loop, CLI extensions |
| `B3_Ray_Tracer_BVH/CMakeLists.txt` | `B3_Ray_Tracer_BVH_Dynamic/CMakeLists.txt` | Copy, then update target name to `b3_ray_tracer_dynamic`; no new deps required |

## Constraints & Rules

All standard constraints from `.claude/rules/00_master_specs.md` apply (C++17, `cl.hpp`, CLI11,
`create_context()`, `cl::Event` profiling, `CL_CHECK`, standalone CMake, kernel copy rule).

Task-specific additions:

- **Animation**: Transform triangle vertex positions on the CPU each frame via a rigid rotation
  matrix (angle incremented by a fixed step). The OBJ is loaded once; vertices are mutated in a
  working copy each frame.
- **Three BVH strategies** (controlled by `--strategy [rebuild|refit|static]` CLI arg, default
  `rebuild`):
  - `rebuild`: Full SAH-BVH construction from the mutated triangle list each frame. CPU-timed via
    `std::chrono::steady_clock`. Upload timed via `cl::Event` on `enqueueWriteBuffer`.
  - `refit`: Walk the existing BVH tree bottom-up, re-expand each node's AABB to enclose its
    updated children/triangles. Do NOT re-sort or re-split. CPU-timed. Upload timed via
    `cl::Event`.
  - `static`: No BVH update. Upload the original BVH once; render the moving scene with a stale
    BVH. Artifact: increasing render errors (black patches / missed triangles) as geometry diverges
    from BVH — expected and intentional for educational contrast.
- **Benchmarking mode**: `--frames N` (default 60) renders N frames headlessly, accumulates
  per-frame timings, then prints a summary table. Live interactive mode is out of scope for this
  task — headless only.
- **Timing instrumentation**:
  - CPU BVH build/refit time: `std::chrono::steady_clock`, reported in ms.
  - GPU buffer upload time: `cl::Event` on `enqueueWriteBuffer` for the BVH node buffer AND the
    triangle position buffer. Sum both for "upload ms".
  - GPU render time: `cl::Event` on `enqueueNDRangeKernel`, same as existing B3 measurement.
- **Console output format** (artifact — no BMP required for the benchmark table):
  ```
  Strategy | Depth | BVH Build (ms) | Upload (ms) | Render (ms) | Total (ms) | FPS
  ---------|-------|----------------|-------------|-------------|------------|----
  rebuild  |   unlimited |      12.34 |        0.45 |        2.11 |      14.90 |  67
  refit    |   unlimited |       1.02 |        0.45 |        2.11 |       3.58 | 279
  static   |         N/A |       0.00 |        0.00 |        2.11 |       2.11 | 474
  ```
  Values are averages over `--frames` frames.
- **Output BMP**: `--output render.bmp` saves the final frame of whichever strategy is active.
  Required to exist post-run.
- **No new third-party dependencies**: tinyobjloader and CLI11 already present in the copied
  CMakeLists.txt; no additional FetchContent entries.
- **Integer arithmetic safety**: Triangle index arithmetic (`y * width + x`, buffer size
  calculations) must use `static_cast<size_t>(a) * b` promotion pattern (master_specs §7.1).

---

## Implementation

0. **Create `B3_Ray_Tracer_BVH_Dynamic/`**: Make the new directory. Copy the four source files
   listed in "Files to Copy Forward" above. All subsequent edits are made only in the new
   directory; the source directory is not touched.

1. **Update `CMakeLists.txt`**: Change the executable target name from `b3_ray_tracer` to
   `b3_ray_tracer_dynamic`. Verify standalone build (`cmake -B build && cmake --build build` from
   `B3_Ray_Tracer_BVH_Dynamic/`).

2. **Add `refit()` to `bvh_builder.hpp`**: After the existing `build()` method, implement a
   bottom-up AABB expansion pass over the flat `BvhNode[]` array. Leaves recompute their AABB from
   the (now-mutated) triangle positions; internal nodes expand to contain their children. No
   re-sorting or re-splitting. Add a CPU self-test: refit a known small tree after a trivial vertex
   translation, assert node AABBs enclose the translated triangles.

3. **Add animation helper to `main.cpp`**: Implement
   `rotate_vertices(const std::vector<Triangle>& src, float angle_rad) -> std::vector<Triangle>`
   that applies a Y-axis rotation matrix to all triangle vertices. Source triangles are immutable
   (loaded once from OBJ); working copy is mutated each frame.

4. **Extend CLI**: Add `--strategy` (string, choices: `rebuild`, `refit`, `static`), `--frames`
   (int, default 60), `--output` (string, default `render.bmp`), `--max-depth` (int, default 0 =
   unlimited; passed to the SAH builder to cap tree depth). Applies to `rebuild` and `refit`
   strategies; ignored for `static`. Existing `--scene`, `--width`, `--height` flags are unchanged.

5. **Implement per-frame benchmark loop**:
   - For each frame in `[0, frames)`:
     a. Compute rotated triangle list (`angle = frame * 2π / frames`).
     b. Apply selected strategy (rebuild / refit / no-op), CPU-time the build step.
     c. Upload BVH node buffer + triangle position buffer; wrap both `enqueueWriteBuffer` calls in
        `cl::Event`, extract `CL_PROFILING_COMMAND_END - CL_PROFILING_COMMAND_START`, sum for
        upload_ms.
     d. Dispatch `ray_trace_bvh` kernel; extract render_ms from `cl::Event`.
     e. Accumulate: `build_ms_total`, `upload_ms_total`, `render_ms_total`.
   - After loop: compute averages and fps = 1000.0 / total_avg; print table.
   - Save final frame BMP via `image_utils.hpp`.

6. **Verify `CL_CHECK` coverage**: All `setArg`, `enqueueNDRangeKernel`, `enqueueWriteBuffer`,
   `enqueueReadBuffer`, `finish` calls must be wrapped in `CL_CHECK`.

---

## Definition of Done (DoD)

Standard DoD items from `.claude/rules/00_master_specs.md` §8 apply.

- [ ] `cmake -B build && cmake --build build` succeeds with zero errors and zero warnings inside
      `02_Projects/B_Graphics_HPC/B3_Ray_Tracer_BVH_Dynamic/`.
- [ ] `02_Projects/B_Graphics_HPC/B3_Ray_Tracer_BVH/` is unmodified (git diff shows no changes
      under that path).
- [ ] `./build/b3_ray_tracer_dynamic --strategy rebuild --frames 60 --output render.bmp` runs
      without error; console prints the timing table; `render.bmp` is created.
- [ ] `./build/b3_ray_tracer_dynamic --strategy refit --frames 60 --output render.bmp` runs
      without error; console prints the timing table.
- [ ] `./build/b3_ray_tracer_dynamic --strategy static --frames 60 --output render.bmp` runs
      without error; console prints the timing table (BVH Build = 0.00, Upload = 0.00 for static).
- [ ] `--help` prints CLI11-generated usage including `--strategy`, `--frames`, `--output`,
      `--scene`.
- [ ] Reported `refit` BVH build time is measurably less than `rebuild` BVH build time
      (refit_avg < rebuild_avg).
- [ ] `GPU=<vendor> ./build/b3_ray_tracer_dynamic --strategy rebuild --frames 10` selects the
      correct device and completes without crashing.
- [ ] `render.bmp` is non-black (pixels contain scene content) for `rebuild` and `refit`
      strategies.
- [ ] `--max-depth 1 --strategy rebuild --frames 10` completes without error; reported render time
      is measurably higher than `--max-depth 0` run (shallow BVH → more triangle tests per ray).
- [ ] MANUAL: Inspect `render.bmp` for `--strategy static` after 60 frames of rotation; confirm
      visible rendering artifacts (black patches or missing geometry) demonstrating BVH/geometry
      divergence.

---

## Execution Report
<!-- Filled by @coder after implementation. -->

- **Status:** DONE
- **Session:** 2026-03-15 (revalidated 2026-03-15)

### Validation
```
1. BUILD — PASS
   cmake --build build: zero errors, zero warnings.
   Output: [100%] Built target b3_ray_tracer_dynamic

2. SOURCE DIRECTORY UNMODIFIED — PASS
   git diff 02_Projects/B_Graphics_HPC/B3_Ray_Tracer_BVH/: no output (clean)

3. REBUILD STRATEGY — PASS
   ./build/b3_ray_tracer_dynamic --strategy rebuild --frames 60 --output render.bmp --scene .../bunny.obj
   Strategy | Depth     | BVH Build (ms) | Upload (ms) | Render (ms) | Total (ms) | FPS
   rebuild  | unlimited |          15.91 |        0.57 |        0.65 |      17.12 |  58
   render.bmp created (1.9 MB). ✓

4. REFIT STRATEGY — PASS
   ./build/b3_ray_tracer_dynamic --strategy refit --frames 60 --output render.bmp --scene .../bunny.obj
   Strategy | Depth     | BVH Build (ms) | Upload (ms) | Render (ms) | Total (ms) | FPS
   refit    | unlimited |           0.93 |        0.57 |        0.59 |       2.09 | 478
   render.bmp created. ✓

5. STATIC STRATEGY — PASS
   ./build/b3_ray_tracer_dynamic --strategy static --frames 60 --output render.bmp --scene .../bunny.obj
   Strategy | Depth     | BVH Build (ms) | Upload (ms) | Render (ms) | Total (ms) | FPS
   static   | unlimited |           0.00 |        0.00 |        0.63 |       0.63 | 1584
   BVH Build=0.00, Upload=0.00 as required. ✓

6. --HELP — PASS
   All required flags present: --strategy, --frames, --output, --scene, --width, --height, --max-depth.
   --strategy TEXT:{rebuild,refit,static} [rebuild]
   --frames INT [60]
   --output TEXT [render.bmp]
   --scene TEXT [assets/bunny.obj]

7. REFIT < REBUILD BVH BUILD TIME — PASS
   refit=0.93ms < rebuild=15.91ms ✓

8. GPU ENV VAR (GPU=NVIDIA) — PASS
   Platform: NVIDIA CUDA [GPU=NVIDIA]
   Device: NVIDIA GeForce RTX 4060 Laptop GPU
   Completed without error. ✓

9. MAX-DEPTH COMPARISON — PASS
   --max-depth 0 (unlimited): Render=0.64ms, BVH nodes=40597
   --max-depth 1 (shallow):   Render=84.77ms, BVH nodes=3
   max-depth 1 render time (84.77ms) >> max-depth 0 (0.64ms). Shallow tree forces brute-force triangle testing. ✓

10. MANUAL (confirmed by user): render.bmp is non-black for rebuild and refit strategies. ✓
11. MANUAL (confirmed by user): render_static.bmp shows visible BVH/geometry divergence artifacts
    (black patches / missing geometry) after 60 frames of rotation. ✓
```

### DoD Checklist
- [x] `cmake -B build && cmake --build build` succeeds with zero errors and zero warnings.
- [x] `02_Projects/B_Graphics_HPC/B3_Ray_Tracer_BVH/` is unmodified (git diff shows no changes).
- [x] `--strategy rebuild --frames 60 --output render.bmp` runs without error; timing table printed; `render.bmp` created.
- [x] `--strategy refit --frames 60 --output render.bmp` runs without error; timing table printed.
- [x] `--strategy static --frames 60 --output render.bmp` runs without error; BVH Build=0.00, Upload=0.00.
- [x] `--help` prints CLI11-generated usage including `--strategy`, `--frames`, `--output`, `--scene`.
- [x] refit BVH build time (0.93ms) < rebuild BVH build time (15.91ms).
- [x] `GPU=NVIDIA` selects correct device and completes without crashing.
- [x] MANUAL (confirmed by user): `render.bmp` is non-black for `rebuild` and `refit` strategies.
- [x] `--max-depth 1` render time (84.77ms) measurably higher than `--max-depth 0` (0.64ms).
- [x] MANUAL (confirmed by user): `render.bmp` for `--strategy static` after 60 frames shows visible artifacts.

### Changed Files
| File | Change |
|------|--------|
| `02_Projects/B_Graphics_HPC/B3_Ray_Tracer_BVH_Dynamic/CMakeLists.txt` | Created — copied from B3, target renamed to `b3_ray_tracer_dynamic` |
| `02_Projects/B_Graphics_HPC/B3_Ray_Tracer_BVH_Dynamic/main.cpp` | Created — copied from B3, adds animation helper, strategy switch, benchmark loop, CLI extensions |
| `02_Projects/B_Graphics_HPC/B3_Ray_Tracer_BVH_Dynamic/bvh_builder.hpp` | Created — copied from B3, adds `refit()` method and CPU self-test |
| `02_Projects/B_Graphics_HPC/B3_Ray_Tracer_BVH_Dynamic/kernels/ray_trace_bvh.cl` | Created — verbatim copy from B3 |

### Remaining
- [x] MANUAL (confirmed by user): `render.bmp` (rebuild/refit) is non-black with visible scene content.
- [x] MANUAL (confirmed by user): `render_static.bmp` shows expected BVH/geometry divergence artifacts.
