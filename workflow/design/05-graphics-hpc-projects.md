# Module 5: Path B — Graphics & HPC

**Version:** 1.0
**Status:** Active — implementation not started
**Module Path:** `02_Projects/B_Graphics_HPC/`

---

## Goal

Build a ray tracer from first principles and scale it to render 100k-triangle scenes at 60 FPS. The core engineering problem across every step is compute throughput: naive per-ray brute-force triangle testing is O(N) per ray — at 100k triangles it is non-interactive. Each step teaches a concrete technique to reduce that cost, culminating in a flagship stackless BVH traversal implementation. An advanced capstone (B4) showcases OpenCL 2.0 Device Enqueue — GPU-spawned secondary kernels eliminating CPU round-trips between ray bounces.

## Non-goals

- OpenCV / camera capture interop (covered in Path A)
- ROS 2 message transport (covered in Path C)
- Model inference or AI postprocessing
- Audio processing
- Full path tracing / global illumination (deferred to Add-on 4.5)
- SVM fine-grained coherency theory (covered in Toolbox: SVM)
- NVDEC / VAAPI hardware decoding

---

## Roadmap / Status

- [ ] Phase 1: B1 — CLBlast MatMul — Benchmark CLBlast GEMM vs naive kernel; establish "library vs custom kernel" decision instinct.
  - *Context*: Executive Summary §Path B item B.1; `GraphicsHPC.md` §B1_CLBlast_MatMul.
- [ ] Phase 2: B2 — Basic Ray Tracer — Minimal sphere scene rendered via OpenGL interop; framebuffer stays on GPU.
  - *Context*: Executive Summary §Path B item B.2; `GraphicsHPC.md` §B2_Ray_Tracer_Basic.
- [ ] Phase 3: B3 — Advanced Ray Tracer with Stackless BVH (Flagship) — CPU SAH-BVH build, flat array upload, per-ray iterative traversal kernel; 60 FPS gate.
  - *Context*: Executive Summary §Path B item B.3; `GraphicsHPC.md` §B3_Ray_Tracer_BVH.
- [ ] Phase 4: B3 Challenge — Dynamic Scene — BVH rebuild vs refit vs partial rebuild per frame; profile upload stage with `cl::Event`.
- [ ] Phase 5: B4 — Device Enqueue (Advanced) — OpenCL 2.0 `enqueue_kernel` for GPU-to-GPU recursive ray bounces; no CPU dispatch between bounces.
  - *Context*: Executive Summary §Path B item B.4; `GraphicsHPC.md` §B4_Device_Enqueue.
- [ ] Phase 6: Module review and cleanup — extract common utils, align naming, verify standalone build.

---

## Specifications

> **Inherits**: `.claude/rules/00_master_specs.md`

**Additional constraints for this path:**

- **OpenGL Interop**: Required for B2 and B3 live window path. Extension `cl_khr_gl_sharing` must be checked at runtime. Headless fallback (`--output render.bmp`) must always be available via CLI11.
- **CLBlast**: Fetched at CMake configure time via `FetchContent`. Must not be pre-installed as a system dependency. Offline note: `-DCMAKE_PREFIX_PATH=/path/to/clblast/install`.
- **OBJ Loading**: Scenes for B3/B4 are loaded from `.obj` files. A lightweight single-header loader (e.g., `tinyobjloader`) is the approved option. No dependency on Assimp or OpenMesh.
- **OpenCL 2.0 Gating (B4)**: All `enqueue_kernel` / device-enqueue code must be wrapped in `#ifdef CL_VERSION_2_0`. B4 must emit a clear runtime error and exit gracefully if the device reports OpenCL C < 2.0.
- **CLI**: All binaries expose `--width`, `--height`, and `--output` (file path for headless). B1 exposes `--size` (matrix dimension). B3/B4 expose `--scene` (OBJ path) and `--frames` (headless frame count).

---

## Architecture (high-level)

### Components

- **CLBlast Benchmark** (`B1_CLBlast_MatMul`): Two compute paths — naive GEMM kernel and CLBlast SGEMM — dispatched for the same matrix size. Reports time in ms and GFLOPS side-by-side. No image output; console table is the artifact.
- **Basic Ray Tracer** (`B2_Ray_Tracer_Basic`): Sphere-only scene. Single OpenCL kernel per frame: ray generation + sphere intersection + Phong shading in one pass. Framebuffer is a `cl::ImageGL` shared with OpenGL — zero PCIe transfer during render loop. Headless mode writes `output.bmp` via `stb_image_write`.
- **BVH Builder (CPU, B3)**: SAH-BVH construction from OBJ triangle soup. Outputs a flat node array with precomputed `hit_link` / `miss_link` pointers. Uploaded once per scene via `cl::Buffer`. Not rebuilt per frame (base implementation).
- **BVH Traversal Kernel (GPU, B3)**: Stackless iterative traversal per ray. Each thread follows its own tree path guided by `hit_link` / `miss_link` without a call stack. Triangle intersection tests happen only at leaf nodes.
- **Device Enqueue Host (B4)**: Allocates a default device-side command queue at context creation (`CL_QUEUE_ON_DEVICE | CL_QUEUE_ON_DEVICE_DEFAULT`). Primary kernel conditionally enqueues the reflection kernel for hits on reflective surfaces.
- **OpenGL Window** (B2, B3): GLFW window loop. Acquires `cl::ImageGL`, dispatches kernel, releases, swaps buffers. Per-frame timing printed via `cl::Event`.

### Data Flow

#### B1 (CLBlast Benchmark)
1. Allocate `cl::Buffer` A, B, C (random float data, host-initialized).
2. Path 1 (Naive): dispatch `matmul` kernel → record `cl::Event` time → compute GFLOPS.
3. Path 2 (CLBlast): call `CLBlastSgemm` → record execution time → compute GFLOPS.
4. Print comparison table to console. No BMP output.

#### B2 (Basic Ray Tracer)
1. GLFW init → OpenGL context → create GL texture.
2. Build `cl::Context` with `cl_khr_gl_sharing` (shared with GL context).
3. Upload sphere array to `cl::Buffer` (once).
4. Per-frame loop:
   - `enqueueAcquireGLObjects({cl_image})`.
   - Dispatch `ray_trace` kernel (global: width × height).
   - `enqueueReleaseGLObjects({cl_image})`.
   - GLFW swap buffers. Print kernel time via `cl::Event`.
5. Headless: render to `cl::Buffer` → read back → `stb_image_write` BMP.

#### B3 (Stackless BVH Ray Tracer)
1. Load `.obj` → triangle list (CPU).
2. Build SAH-BVH (CPU) → flatten to `BvhNode[]` with `hit_link` / `miss_link`.
3. Upload `cl::Buffer`: triangles + BVH nodes (one-time cost).
4. Per-frame loop (identical to B2 OpenGL path):
   - Acquire GL image.
   - Dispatch `ray_trace_bvh` kernel.
   - Release GL image. Print kernel time.
5. Report: naive render time (brute force) vs BVH render time, speedup multiplier.

#### B4 (Device Enqueue)
1. Create context with `CL_QUEUE_ON_DEVICE_DEFAULT` flag (guarded by `#ifdef CL_VERSION_2_0`).
2. Build program with `-cl-std=CL2.0`.
3. Upload scene (same as B3) + reflectance map buffer.
4. Primary kernel: ray gen → intersection → if reflective, `enqueue_kernel` for reflection pass.
5. `clFinish` on host queue (all GPU-spawned work drains). Read back framebuffer.
6. Print per-bounce timing extracted from device-side event buffer.

---

## Key Decisions (and Rationale)

1. **SAH-BVH Built on CPU, Traversal on GPU**
   - **Why**: BVH construction is a complex, low-frequency operation (once per scene load). SAH quality directly impacts traversal efficiency. Keeping the build on CPU avoids GPU-side sort/reduce complexity and is standard practice in production renderers.

2. **Stackless Traversal via `hit_link` / `miss_link` Precomputation**
   - **Why**: GPU threads have no call stack. Stackless traversal with precomputed sibling/parent links is the canonical GPU-friendly solution (used by NVIDIA RTX hardware). It also enables aggressive use of `select()` to reduce branch divergence in the inner loop.

3. **Flat `BvhNode[]` Array (Structure-of-Arrays preferred for triangle data)**
   - **Why**: Flat array enables coalesced memory access across threads traversing adjacent nodes. SoA layout for triangle attributes (positions, normals) ensures that per-attribute reads are coalesced when all threads in a warp read the same attribute.

4. **OpenGL Interop for Live Window (cl_khr_gl_sharing)**
   - **Why**: The educational point of B2/B3 is that the framebuffer never leaves the GPU during the render loop. Requiring a headless BMP fallback ensures the module remains testable on PoCL / CPU runtimes where `cl_khr_gl_sharing` is unavailable.

5. **CLBlast via FetchContent (Not System Install)**
   - **Why**: Consistency with the standalone-buildable rule. A user copying `B1_CLBlast_MatMul/` to a new machine should not need to install CLBlast manually.

6. **B4 as Optional / Guarded Capstone**
   - **Why**: Nvidia's OpenCL 2.0 Device Enqueue support is incomplete. Treating B4 as an advanced, runtime-checked capstone — rather than a required step — prevents blocking Path B completion on hardware-specific support. The pedagogical value (showing a feature CUDA added years later) is preserved.

7. **Headless Mode Mandatory on All Binaries**
   - **Why**: CI/CD and Docker environments have no display. `--output render.bmp` must always produce a verifiable artifact independent of GLFW/OpenGL availability.

---

## Known Issues / Risks

- **`cl_khr_gl_sharing` Unavailable on PoCL / CPU Runtimes**: The interop path silently fails init on CPU-only drivers. Headless mode (`--output`) is the mitigation; the DoD for B2/B3 must include a headless verification step.
- **Nvidia OpenCL 2.0 Device Enqueue**: `enqueue_kernel` returns `CL_INVALID_OPERATION` on most Nvidia drivers. B4 must detect this at runtime and exit with a descriptive error, not a crash.
- **BVH `miss_link` Correctness**: Incorrectly computed `miss_link` pointers produce black patches (rays terminate early) or infinite loops. The BVH builder must include a CPU-side self-test (traverse a known ray, assert expected leaf is reached) before the kernel is written.
- **SAH Split Quality vs Build Time**: For very large meshes (1M+ triangles), SAH median-split may be too slow for interactive loading. Out of scope for this module (100k triangle gate is the target).
- **Triangle Data Layout vs Coalescing**: Array-of-Structs `{float3 v0, v1, v2}` per triangle is easy to build but causes uncoalesced reads when all threads access different triangle indices. SoA (`float* v0x, *v0y, *v0z, ...`) must be used in the final BVH kernel to satisfy the performance gate.
- **GLFW Dependency in Headless Docker**: Docker images used in CI must have `libGL` and `libEGL` present or the CMake `find_package(OpenGL)` call will fail even when building in headless mode. CMake must gate the OpenGL interop build on `CL_KHR_GL_SHARING` availability, not unconditionally.

---

## Performance Gates (Path Completion)

| Project | Metric | Target |
| :--- | :--- | :--- |
| B1 CLBlast MatMul | CLBlast speedup over naive GEMM | ≥ 5× at matrix size 1024×1024 |
| B2 Basic Ray Tracer | Kernel time | < 10 ms per frame @ 1280×720, 16 spheres |
| B3 BVH Ray Tracer | Render time | ≥ 60 FPS @ 100k triangles, 1920×1080 |
| B3 BVH vs Naive | Speedup | Reported in console (expected ~100–250×) |
| B4 Device Enqueue | Per-bounce latency vs CPU-dispatched | GPU-spawned path ≤ 50% of CPU-dispatched time (3 bounces) |

All timing reported via `cl::Event` profiling in milliseconds to 3 decimal places. Wall-clock estimates do not satisfy the gate.

---

## Specifications & Standards

- **Directory Structure**:
  ```
  02_Projects/B_Graphics_HPC/
  ├── GraphicsHPC.md             (existing user-facing README)
  ├── B1_CLBlast_MatMul/
  │   ├── CMakeLists.txt
  │   ├── main.cpp
  │   └── kernels/naive_gemm.cl
  ├── B2_Ray_Tracer_Basic/
  │   ├── CMakeLists.txt
  │   ├── main.cpp
  │   └── kernels/ray_trace.cl
  ├── B3_Ray_Tracer_BVH/
  │   ├── CMakeLists.txt
  │   ├── main.cpp
  │   ├── bvh_builder.hpp        (CPU SAH-BVH, header-only)
  │   └── kernels/ray_trace_bvh.cl
  └── B4_Device_Enqueue/
      ├── CMakeLists.txt
      ├── main.cpp
      └── kernels/primary_ray.cl
      └── kernels/reflection_ray.cl
  ```
- **Verification Standard**:
  - B1: Console table with naive/CLBlast time in ms and GFLOPS. No BMP required.
  - B2: Headless `output.bmp` showing ≥ 1 sphere with shadow. Live window optional.
  - B3: Headless `render.bmp` showing OBJ scene correctly (no black patches). Console prints naive vs BVH timing and speedup. Live window optional.
  - B4: Console prints per-bounce timing for CPU-dispatched vs device-enqueued paths. `render.bmp` showing multi-bounce reflections.
- **Tooling**:
  - `cl.hpp` (C++ bindings, OpenCL 1.2 baseline).
  - `stb_image_write` for BMP/PNG headless output.
  - `CLI11` via `common/common.cmake` for all argument parsing.
  - `find_package(OpenCL REQUIRED)` in every `CMakeLists.txt`.
  - `common/ocl_wrapper.hpp` → `create_context()` for GPU selection.
  - `CL_CHECK(err)` macro from `common/opencl_utils.hpp` for all error handling.
  - CLBlast: `FetchContent_Declare` in B1 `CMakeLists.txt`.
  - GLFW + OpenGL: `find_package(glfw3)` + `find_package(OpenGL)`, gated on availability.
  - tinyobjloader: `FetchContent_Declare` in B3/B4 `CMakeLists.txt`.
- **OpenCL 2.0+ Gating**: All Device Enqueue code must be wrapped in `#ifdef CL_VERSION_2_0`. B4 binary must emit a human-readable error and exit with code 1 if device does not support OpenCL C 2.0.

---

## Prerequisites

- Module 1 completed (`01_Host_API/`): `cl.hpp` usage, `cl::Event` profiling, `CL_CHECK` error handling.
- OpenGL + GLFW (for live window in B2/B3): `sudo apt install libglfw3-dev libgl-dev`. Not required for headless build.
- CLBlast and tinyobjloader are fetched automatically by CMake at configure time (internet required on first build).
- `assets/bunny.obj` (or equivalent 100k-triangle OBJ) and `assets/cornell_box.obj` present in repository root.

See [main README](../../README.md) for base requirements (OpenCL 1.2+, CMake 3.18+, Docker setup).
