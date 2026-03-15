# Task 033: B4 — Device Enqueue (Advanced Capstone)

## Context
- **Design Feature:** `workflow/design/05-graphics-hpc-projects.md`
- **Milestone:** Phase 5 — B4 Device Enqueue
- **Relevant Files:**
  - `workflow/design/05-graphics-hpc-projects.md` — (read-only: architecture reference)
  - `02_Projects/B_Graphics_HPC/B3_Ray_Tracer_BVH/` — (read-only: copy-forward reference for scene loading, BVH, kernel structure)
  - `02_Projects/B_Graphics_HPC/B4_Device_Enqueue/` — (new directory)
  - `common/ocl_wrapper.hpp` — (read-only: `create_context()`)
  - `common/image_utils.hpp` — (read-only: BMP output)
  - `.claude/rules/00_master_specs.md` — (read-only: global constraints)

## Objective

Implement `B4_Device_Enqueue`: a multi-bounce ray tracer that uses OpenCL 2.0 `enqueue_kernel` to spawn reflection-ray passes from the GPU without CPU dispatch round-trips, demonstrating GPU-to-GPU kernel scheduling and benchmarking it against an equivalent CPU-dispatched path.

## Constraints & Rules

- All standard constraints from `00_master_specs.md` apply (C++17, `cl.hpp`, CLI11, `create_context()`, `cl::Event` profiling, `CL_CHECK`, standalone CMake, kernel copy rule).
- **OpenCL 2.0 gating**: All `enqueue_kernel` / device-enqueue code (host and kernel) must be wrapped in `#ifdef CL_VERSION_2_0`. At runtime, query `CL_DEVICE_OPENCL_C_VERSION` on the selected device. If < 2.0, print a descriptive message and exit with code 0 (no crash, no hang — master_specs §4).
- **Program build flag**: `-cl-std=CL2.0` only when device supports it; fall back gracefully.
- **tinyobjloader**: fetched via `FetchContent` (same pattern as B3).
- **GL interop**: optional — same CMake auto-detect pattern as B2/B3 (`find_package(glfw3)`, `find_package(OpenGL)`); auto-set `-DNO_GL_INTEROP` with `WARNING` if missing. Headless (`--output`) is the primary verification path.
- **FORBIDDEN**: Hard-coded platform/device indices. Hardcoded asset paths in code.

---

## Implementation

### Directory scaffold

Create `02_Projects/B_Graphics_HPC/B4_Device_Enqueue/` with the following layout (per design §Specifications):

```
B4_Device_Enqueue/
├── CMakeLists.txt
├── main.cpp
└── kernels/
    ├── primary_ray.cl
    └── reflection_ray.cl
```

### Steps

1. **CMakeLists.txt** — Standalone build.
   - `cmake_minimum_required(VERSION 3.18)`, `project(b4_device_enqueue CXX)`, `set(CMAKE_CXX_STANDARD 17)`, `set(CMAKE_CXX_EXTENSIONS OFF)`.
   - `find_package(OpenCL REQUIRED)`.
   - `FetchContent` for CLI11 (or include via `common/common.cmake`).
   - `FetchContent` for tinyobjloader.
   - Optional `find_package(glfw3)` + `find_package(OpenGL)`: if either missing, add `-DNO_GL_INTEROP` compile definition and emit `message(WARNING ...)`.
   - EGL detection: `find_package(OpenGL COMPONENTS EGL)`; set `HAS_EGL` compile definition if found.
   - Kernel copy rule: `add_custom_command` POST_BUILD copying `kernels/` to `$<TARGET_FILE_DIR:b4_device_enqueue>/kernels/`.

2. **Runtime OpenCL 2.0 check (main.cpp)** — After `create_context()`, query `CL_DEVICE_OPENCL_C_VERSION` on the selected device. Parse major version. If < 2, print `"Device does not support OpenCL C 2.0 -- Device Enqueue unavailable. Exiting."` and return 0.

3. **Scene loading** — Use tinyobjloader to load `--scene` OBJ (default `assets/cornell_box.obj`). Build triangle list as flat SoA `float` arrays for v0/v1/v2 (coalesced reads — design §Known Issues, Triangle Data Layout). Alternative: pass `--scene builtin:spheres` to activate the hard-coded sphere scene carried over from B2 (no OBJ loading; triangulated sphere meshes or analytic sphere intersector) — useful on systems where the OBJ assets are absent and guarantees reflections are visible on the mirror sphere.

4. **BVH** — Copy `bvh_builder.hpp` from `B3_Ray_Tracer_BVH/` verbatim (no modifications). Upload `BvhNode[]` and triangle buffers to `cl::Buffer` once after load.

5. **Device-side command queue (host setup)** — Create a device-side queue using `clCreateCommandQueueWithProperties` with `CL_QUEUE_ON_DEVICE | CL_QUEUE_ON_DEVICE_DEFAULT`. Pass its handle to the primary kernel as a kernel argument. Guard all device-queue host code in `#ifdef CL_VERSION_2_0`.

6. **Reflectance buffer** — Allocate a `cl::Buffer` of per-triangle reflectance values (`float`). Initialize to 0.0f for non-reflective triangles; a configurable subset (e.g., floor plane or a material group) set to 0.8f.

7. **Framebuffer buffer** — `cl::Buffer` RGBA float (width × height × 4). In headless mode this is the output buffer. In GL interop mode, use `cl::ImageGL`.

8. **Kernels**:
   - `primary_ray.cl`: ray generation + BVH traversal + Phong shading. For each hit on a reflective surface (`reflectance > 0`), call `enqueue_kernel` on the device-side queue to dispatch `reflection_ray` for that pixel. Guarded by `#if __OPENCL_C_VERSION__ >= 200`.
   - `reflection_ray.cl`: accepts ray origin/direction + bounce depth counter. Performs BVH traversal + shading. Recurses (via `enqueue_kernel`) up to `--bounces` limit. Writes accumulated color to framebuffer at its pre-assigned pixel index.
   - Both kernels: `size_t gid = get_global_id(0)`; guard `if (gid < (size_t)pixel_count)`.

9. **CPU-dispatched baseline path** — A second host-side render loop that dispatches the reflection kernel explicitly from the CPU for each bounce (no `enqueue_kernel`). Controlled by `--mode cpu|gpu` CLI arg (default `gpu`).

10. **Timing** — Both paths timed via `cl::Event` per bounce. After `clFinish`, extract `CL_PROFILING_COMMAND_START` / `CL_PROFILING_COMMAND_END`. Print table:
    ```
    Bounce | CPU-dispatched (ms) | GPU-spawned (ms)
    ```

11. **Headless output** — After render, `enqueueReadBuffer` to host RGBA float buffer, convert to RGBA uint8, write BMP via `common/image_utils.hpp` or `stb_image_write`. CLI arg `--output` (default `render.bmp`).

12. **CLI args** (CLI11):
    - `--width` (int, default 1280)
    - `--height` (int, default 720)
    - `--output` (string, default `render.bmp`)
    - `--scene` (string, default `assets/cornell_box.obj`; pass `builtin:spheres` for analytic sphere scene)
    - `--frames` (int, default 1 in headless)
    - `--bounces` (int, default 3)
    - `--mode` (string enum `cpu`/`gpu`, default `gpu`)

---

## Definition of Done (DoD)

Standard items from `00_master_specs.md §8` apply:
- [ ] `cmake -B build && cmake --build build` succeeds with zero errors and zero warnings.
- [ ] Binary runs without arguments and completes without error.
- [ ] `--help` prints CLI11-generated usage including all defined flags (`--width`, `--height`, `--output`, `--scene`, `--frames`, `--bounces`, `--mode`).
- [ ] `GPU=<vendor> ./build/b4_device_enqueue` selects the correct device without crashing.

Task-specific:
- [ ] On a device without OpenCL C 2.0 support, binary prints a descriptive message and exits with code 0 (no crash, no hang).
- [ ] Headless `--output render.bmp` produces a non-black BMP showing the cornell box (or sphere scene) with at least one visible reflection region; no black-patch artifacts from incorrect BVH traversal.
- [ ] Console prints the per-bounce timing table (CPU-dispatched ms vs GPU-spawned ms, 3 decimal places).
- [ ] `--mode cpu` and `--mode gpu` both complete successfully.
- [ ] Performance gate: GPU-spawned path ≤ 50% of CPU-dispatched latency at 3 bounces. Hardware-waiver: if device does not support Device Enqueue at runtime, gate is waived and logged as `N/A`.
- [ ] No unchecked OpenCL return codes: all `setArg`, `finish`, `enqueueNDRangeKernel`, `enqueueReadBuffer`, `enqueueWriteBuffer`, `enqueueUnmapMemObject` wrapped in `CL_CHECK`.
- [ ] Integer size promotion: `static_cast<size_t>(width) * height` — no `int * int` before cast.
- [ ] MANUAL: Run `./build/b4_device_enqueue --mode gpu --output render.bmp`; confirm `render.bmp` shows the scene with visible reflection highlights (not all-black).
- [ ] MANUAL: Run `./build/b4_device_enqueue --mode cpu --output render_cpu.bmp`; confirm `render_cpu.bmp` is visually equivalent to `render.bmp`.

---

## Execution Report
<!-- Filled by @coder after implementation. -->

- **Status:** PENDING
- **Session:** —

### Validation
```
[output here]
```

### Changed Files
| File | Change |
|------|--------|
| `02_Projects/B_Graphics_HPC/B4_Device_Enqueue/CMakeLists.txt` | Created |
| `02_Projects/B_Graphics_HPC/B4_Device_Enqueue/main.cpp` | Created |
| `02_Projects/B_Graphics_HPC/B4_Device_Enqueue/kernels/primary_ray.cl` | Created |
| `02_Projects/B_Graphics_HPC/B4_Device_Enqueue/kernels/reflection_ray.cl` | Created |

### Remaining
- [ ] Implementation pending
