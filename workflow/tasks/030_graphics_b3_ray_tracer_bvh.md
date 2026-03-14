# Task 030: B3 — Advanced Ray Tracer with Stackless BVH

## Context
- **Design Feature:** `workflow/design/05-graphics-hpc-projects.md`
- **Milestone:** Phase 3 — B3 Advanced Ray Tracer with Stackless BVH (Flagship)
- **Relevant Files:**
  - `workflow/design/05-graphics-hpc-projects.md` — read-only: architecture, constraints, performance gates
  - `02_Projects/B_Graphics_HPC/B2_Ray_Tracer_Basic/` — read-only: reference for GL interop pattern and CMake structure
  - `common/ocl_wrapper.hpp` — read-only: `create_context()`
  - `common/image_utils.hpp` — read-only: BMP write utility
  - `assets/bunny.obj` — read-only: benchmark scene (~70k triangles)
  - `02_Projects/B_Graphics_HPC/B3_Ray_Tracer_BVH/` — new directory to create

## Objective

Implement a standalone ray tracer that builds a SAH-BVH on the CPU, uploads a flat node array to the GPU, and performs stackless iterative BVH traversal per ray — rendering `assets/bunny.obj` at 1920×1080 at ≥ 60 FPS (measured via `cl::Event`).

## Constraints & Rules

All standard constraints from `.claude/rules/00_master_specs.md` apply (C++17, `cl.hpp`, CLI11, `create_context()`, `cl::Event` profiling, `CL_CHECK`, standalone CMake, kernel copy rule).

Task-specific additions:

- **OBJ Loading**: Use `tinyobjloader` fetched via `FetchContent`. No Assimp or OpenMesh.
- **BVH Builder**: CPU-side SAH-BVH, header-only (`bvh_builder.hpp`). Must produce a flat `BvhNode[]` array with precomputed `hit_link` / `miss_link` per node. A CPU self-test must traverse a known ray and assert the expected leaf is reached before the GPU kernel is written.
- **Traversal Kernel**: Stackless iterative traversal using `hit_link` / `miss_link`. Triangle intersection only at leaf nodes. Triangle data must use SoA layout (not AoS) to ensure coalesced reads.
- **GL Interop**: Controlled by compile-time flag `NO_GL_INTEROP` (default OFF). CMake emits a `WARNING` and auto-sets `NO_GL_INTEROP=ON` when `find_package(glfw3)` or `find_package(OpenGL)` fails. Extension `cl_khr_gl_sharing` must be checked at runtime even when the flag is not set.
- **GL Teardown Order** (master_specs §7.6): All CL objects referencing GL resources must be destroyed before `glfwTerminate()`. Scope them in an explicit `{}` block; reset the shared `cl::Context` explicitly after the inner scope.
- **Interactive Camera (live mode only)**: Orbit (left-mouse drag → spherical coords azimuth + elevation), zoom (scroll wheel → forward axis), pan (middle-mouse drag → look-at target in view plane). Camera state (`cam_pos`, `cam_target`, `fov`) uploaded each frame as kernel args — not hardcoded `#define`.
- **Headless path**: Uses a fixed default camera pose. Produces `render.bmp` via `common/image_utils.hpp` / `stb_image_write`.
- **CLI (CLI11)**: Expose `--width` (default 1920), `--height` (default 1080), `--scene` (OBJ path, default `assets/bunny.obj`), `--output` (headless BMP path, default `render.bmp`), `--frames` (headless frame count, default 1).
- **Timing report**: After rendering, print to console: naive brute-force render time (ms), BVH render time (ms), speedup multiplier, and FPS. GPU times via `cl::Event` only — wall-clock does not satisfy the gate.
- **Integer safety** (master_specs §7.1): Promote first operand before multiplying buffer sizes.
- **No hardcoded asset paths** (master_specs §3): Scene path always via `--scene` CLI arg.
- **Hardware waiver**: If the ≥ 60 FPS gate is not met on the test machine, document the measured FPS and GPU model in the Execution Report. The gate is waived for integrated GPUs; discrete GPU results are the reference.

---

## Implementation

1. Create directory `02_Projects/B_Graphics_HPC/B3_Ray_Tracer_BVH/` with `CMakeLists.txt`, `main.cpp`, `bvh_builder.hpp`, and `kernels/ray_trace_bvh.cl`.
2. `CMakeLists.txt`: standalone build; `FetchContent` for `tinyobjloader`; `find_package(glfw3)` + `find_package(OpenGL)` with auto-fallback to `NO_GL_INTEROP`; include `common/common.cmake`; kernel copy rule.
3. `bvh_builder.hpp`: SAH median-split BVH construction from triangle soup → flat `BvhNode[]` with `hit_link` / `miss_link`; CPU self-test function callable from `main.cpp` before GPU dispatch.
4. `kernels/ray_trace_bvh.cl`: stackless BVH traversal kernel; SoA triangle layout; Phong shading at hit point; output to `cl::ImageGL` (interop) or `cl::Buffer` (headless).
5. `main.cpp`: CLI11 arg parsing; `create_context()`; BVH build + self-test; buffer upload; render loop (interop or headless); `cl::Event` timing; console report.
6. For the naive baseline timing: dispatch a second kernel (or reuse the B2 brute-force approach) on the same scene once before the BVH loop to record the reference time.

---

## Definition of Done (DoD)

<!-- Standard items from master_specs §8 apply. -->

- [ ] `cmake -B build && cmake --build build` succeeds with zero errors and zero warnings from `02_Projects/B_Graphics_HPC/B3_Ray_Tracer_BVH/`.
- [ ] `./build/b3_ray_tracer_bvh --help` prints CLI11-generated usage including `--width`, `--height`, `--scene`, `--output`, `--frames`.
- [ ] `GPU=<vendor> ./build/b3_ray_tracer_bvh` selects the correct device without crashing.
- [ ] `cmake -B build_headless -DNO_GL_INTEROP=ON && cmake --build build_headless` succeeds with zero errors and zero warnings.
- [ ] `./build_headless/b3_ray_tracer_bvh --scene assets/bunny.obj --output render.bmp` exits 0; `render.bmp` exists and is non-empty.
- [ ] MANUAL: Open `render.bmp`; bunny scene is visible with no large black patches (BVH traversal correctness; miss_link paths complete).
- [ ] Console prints naive render time (ms), BVH render time (ms), speedup multiplier, and FPS — all sourced from `cl::Event` profiling.
- [ ] BVH CPU self-test passes (asserted in code before GPU dispatch; binary exits non-zero if it fails).
- [ ] MANUAL: Run `./build/b3_ray_tracer_bvh --scene assets/bunny.obj` with a display; confirm live GLFW window opens, bunny is visible, left-mouse drag orbits, scroll zooms, middle-mouse pans, window closes cleanly.
- [ ] MANUAL: Confirm console reports ≥ 60 FPS on a discrete GPU (e.g. NVIDIA/AMD) at 1920×1080 with `assets/bunny.obj`. If gate not met, record measured FPS and GPU model in Execution Report under the hardware-waiver clause.

---

## Execution Report
<!-- Filled by @coder after implementation. -->

- **Status:** PENDING
- **Session:** [YYYY-MM-DD]

### Validation
```
[output here]
```

### Changed Files
| File | Change |
|------|--------|
| `02_Projects/B_Graphics_HPC/B3_Ray_Tracer_BVH/CMakeLists.txt` | Created |
| `02_Projects/B_Graphics_HPC/B3_Ray_Tracer_BVH/main.cpp` | Created |
| `02_Projects/B_Graphics_HPC/B3_Ray_Tracer_BVH/bvh_builder.hpp` | Created |
| `02_Projects/B_Graphics_HPC/B3_Ray_Tracer_BVH/kernels/ray_trace_bvh.cl` | Created |

### Remaining
- [ ] All DoD items above
