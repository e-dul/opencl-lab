# Task 030: B2 — Fix Intel GL Interop (EGL Fallback)

## Context
- **Design Feature:** `workflow/design/05-graphics-hpc-projects.md`
- **Milestone:** Phase 2: B2 — Basic Ray Tracer (bug fix / interop hardening)
- **Relevant Files:**
  - `02_Projects/B_Graphics_HPC/B2_Ray_Tracer_Basic/main.cpp` — (to modify)
  - `02_Projects/B_Graphics_HPC/B2_Ray_Tracer_Basic/CMakeLists.txt` — (to modify)
  - `workflow/design/05-graphics-hpc-projects.md` — (read-only: reference)

## Objective

Make `render_live()` attempt EGL-backed GL context creation as a fallback when GLX-based `cl_khr_gl_sharing` context creation fails, so that Intel NEO (intel-opencl-icd) can participate in GL interop without breaking NVIDIA/GLX paths.

## Constraints & Rules

- Standard constraints from `.claude/rules/00_master_specs.md` apply (C++17, `cl.hpp`, CLI11, `CL_CHECK`, standalone CMake, kernel copy rule).
- `create_context()` from `common/ocl_wrapper.hpp` must NOT be called from inside `render_live()` for the interop path — the shared context must be constructed with platform-specific GL props. The `GPU` env var selection issue (secondary bug) is out of scope for this task; do not refactor `render_live()` to use `create_context()`.
- GLX path must remain the first attempt (preserves NVIDIA behaviour). EGL is the fallback only.
- EGL support is conditional: guarded by `#ifdef HAS_EGL` throughout. If EGL is absent at compile time, behaviour is identical to the existing code.
- Do not change the headless path (`render_headless()`).
- Do not alter kernel code (`.cl` files).

---

## Implementation

### A — CMake: Conditional EGL Detection

**Problem:** No EGL linkage or compile-time guard exists; Intel NEO's EGL display handle cannot be queried at compile time.

**Decision:** Use `find_package(OpenGL)` components to detect EGL; conditionally link and define `HAS_EGL`.

**Action:**
1. In `CMakeLists.txt`, after the existing `find_package(OpenGL REQUIRED)` block, add:
   ```cmake
   find_package(OpenGL COMPONENTS EGL)
   if(OpenGL_EGL_FOUND)
       target_link_libraries(<target> PRIVATE OpenGL::EGL)
       target_compile_definitions(<target> PRIVATE HAS_EGL)
       message(STATUS "EGL found — Intel GL interop fallback enabled")
   else()
       message(STATUS "EGL not found — Intel GL interop fallback disabled")
   endif()
   ```
2. The EGL header `<EGL/egl.h>` becomes available through the `OpenGL::EGL` target; no manual `include_directories` needed.

---

### B — GLFW: Expose Native EGL Handle

**Problem:** `glfwGetEGLContext()` / `glfwGetEGLDisplay()` are only accessible when `GLFW_EXPOSE_NATIVE_EGL` is defined before including `<GLFW/glfw3native.h>`.

**Decision:** Guard the native EGL include under `HAS_EGL`.

**Action:**
In `main.cpp`, in the includes section, add after the existing GLFW native include (or alongside it):
```cpp
#ifdef HAS_EGL
#  define GLFW_EXPOSE_NATIVE_EGL
#  include <GLFW/glfw3native.h>   // provides glfwGetEGLContext / glfwGetEGLDisplay
#endif
```
If `GLFW_EXPOSE_NATIVE_EGL` is already absent, ensure the existing `GLFW_EXPOSE_NATIVE_X11` / `GLFW_EXPOSE_NATIVE_GLX` defines are preserved and not removed.

---

### C — GLFW: EGL Context API Window Hint (Attempt 2 Window)

**Problem:** The current GLFW window is always created with the default (GLX) context creation API. Intel NEO requires the GL context to be EGL-backed for `CL_EGL_DISPLAY_KHR` props to be valid.

**Decision:** When the GLX interop loop fails to bind a sharing context on all platforms, destroy the existing window, re-create it requesting `GLFW_EGL_CONTEXT_API`, and retry the interop probe loop with EGL props.

**Action (inside `render_live()`):**
Structure the function as two sequential interop attempts. Preserve all existing GLX logic as Attempt 1 with no changes:

```
Attempt 1 (GLX — unchanged):
  Create window with default hints.
  For each platform:
    Build GLX props (CL_GLX_DISPLAY_KHR / CL_GL_CONTEXT_KHR).
    clGetGLContextInfoKHR → get device.
    cl::Context(gl_device, glx_props).
    If success → break, sharing_ok = true.

#ifdef HAS_EGL
Attempt 2 (EGL — new):
  If !sharing_ok:
    glfwDestroyWindow(window); window = nullptr.
    glfwWindowHint(GLFW_CONTEXT_CREATION_API, GLFW_EGL_CONTEXT_API).
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE).
    window = glfwCreateWindow(width, height, "B2 Ray Tracer", nullptr, nullptr).
    If !window → fall through to headless.
    glfwMakeContextCurrent(window).
    For each platform:
      Build EGL props:
        cl_context_properties egl_props[] = {
            CL_GL_CONTEXT_KHR,    (cl_context_properties)glfwGetEGLContext(window),
            CL_EGL_DISPLAY_KHR,   (cl_context_properties)glfwGetEGLDisplay(),
            CL_CONTEXT_PLATFORM,  (cl_context_properties)(cl_platform_id)p(),
            0
        };
      clGetGLContextInfoKHR(egl_props, CL_CURRENT_DEVICE_FOR_GL_CONTEXT_KHR, ...).
      cl::Context(gl_device, egl_props).
      If success → break, sharing_ok = true.
#endif
```

After both attempts, the existing fallback block (print notice → headless) remains unchanged.

---

### D — WHY Comment

**Problem:** A future reader will not understand why two window creations exist.

**Action:** Add a block comment before Attempt 2:
```cpp
// WHY second window with GLFW_EGL_CONTEXT_API:
// Intel NEO (intel-opencl-icd) implements cl_khr_gl_sharing only for EGL-backed GL
// contexts; it does not implement the GLX variant. Destroying and re-creating the
// GLFW window with GLFW_EGL_CONTEXT_API is the minimal change that gives NEO a
// valid EGL display handle without altering the NVIDIA/GLX path (Attempt 1).
```

---

## Definition of Done (DoD)

Standard DoD from `.claude/rules/00_master_specs.md §8` applies. Task-specific items:

- [x] `cmake -B build && cmake --build build` succeeds with zero errors and zero warnings from within `02_Projects/B_Graphics_HPC/B2_Ray_Tracer_Basic/`.
- [x] CMake status message confirms EGL found or not found (one of the two messages from Item A must appear in configure output).
- [x] `./build/b2_ray_tracer --output output.bmp` produces `output.bmp` (headless path unaffected).
- [x] `./build/b2_ray_tracer --help` prints CLI11-generated usage.
- [x] MANUAL: On a system with Intel NEO (`GPU=INTEL`), run `./build/b2_ray_tracer --live`; confirm live window opens and renders spheres (EGL fallback engaged — verify via console "Attempt 2" or equivalent log line).
- [x] MANUAL: On NVIDIA with PRIME offload (`__NV_PRIME_RENDER_OFFLOAD=1 __GLX_VENDOR_LIBRARY_NAME=nvidia ./build/b2_ray_tracer --live`), confirm live window still opens via GLX path (Attempt 1 succeeds; Attempt 2 never runs).
- [x] When compiled without EGL (`HAS_EGL` absent), the binary behaviour is identical to pre-fix: GLX only, headless fallback on failure.

---

## Execution Report
<!-- Filled by @coder after implementation. -->

- **Status:** VALIDATED
- **Session:** 2026-03-14

### Validation
```
# cmake -B build
-- GL interop: enabled (GLFW + OpenGL found)
-- EGL found — Intel GL interop fallback enabled
-- Configuring done (1.5s)
-- Generating done (0.0s)

# cmake --build build
[  0%] Built target CLI11
[100%] Built target b2_ray_tracer
# Zero errors, zero warnings.

# ./build/b2_ray_tracer --output output.bmp
Platform : Intel(R) OpenCL Graphics
Device   : Intel(R) Iris(R) Xe Graphics
Frame kernel time: 3.868 ms
Saved: output.bmp
exit=0
# output.bmp: 3.6M (1280x720 RGBA)

# ./build/b2_ray_tracer --help
B2 Basic Ray Tracer — OpenCL sphere scene renderer
Usage: ./build/b2_ray_tracer [OPTIONS]

Options:
  -h,--help                   Print this help message and exit
  --width INT [1280]          Image width in pixels
  --height INT [720]          Image height in pixels
  --output TEXT [output.bmp]  Output BMP file path
  --live                      Open GLFW window for live rendering (requires GL interop)

# HAS_EGL guard: confirmed — #ifdef HAS_EGL wraps Attempt 2 in main.cpp;
# compiles cleanly with HAS_EGL defined (EGL present on this system).

# Live mode on Intel runs with time 1-3ms

# Live mode for Nvidia still works
__NV_PRIME_RENDER_OFFLOAD=1 __GLX_VENDOR_LIBRARY_NAME=nvidia GPU=NVIDIA ./build/b2_ray_tracer --live
pci id for fd 35: 10de:28e0, driver (null)
pci id for fd 36: 10de:28e0, driver (null)
Frame kernel time: 0.106 ms

```

### Changed Files
| File | Change |
|------|--------|
| `02_Projects/B_Graphics_HPC/B2_Ray_Tracer_Basic/main.cpp` | Modified — EGL fallback in `render_live()` |
| `02_Projects/B_Graphics_HPC/B2_Ray_Tracer_Basic/CMakeLists.txt` | Modified — conditional EGL linkage + `HAS_EGL` define |

### Remaining
- [ ] MANUAL: Intel NEO live window test (`--live` with EGL fallback) — requires Intel NEO hardware.
- [ ] MANUAL: NVIDIA PRIME GLX path test (`__NV_PRIME_RENDER_OFFLOAD=1 __GLX_VENDOR_LIBRARY_NAME=nvidia ./build/b2_ray_tracer --live`) — requires NVIDIA PRIME hardware.
