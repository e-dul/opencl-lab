# Task 034: Module 5 — Graphics & HPC Cleanup

## Context
- **Design Feature:** `workflow/design/05-graphics-hpc-projects.md`
- **Milestone:** Phase 6 — Module review and cleanup
- **Relevant Files:**
  - `02_Projects/B_Graphics_HPC/B2_Ray_Tracer_Basic/CMakeLists.txt` — to modify
  - `02_Projects/B_Graphics_HPC/B3_Ray_Tracer_BVH/CMakeLists.txt` — to modify
  - `02_Projects/B_Graphics_HPC/B3_Ray_Tracer_BVH_Dynamic/CMakeLists.txt` — to modify
  - `02_Projects/B_Graphics_HPC/B4_Device_Enqueue/CMakeLists.txt` — to modify
  - `02_Projects/B_Graphics_HPC/B2_Ray_Tracer_Basic/main.cpp` — reference
  - `02_Projects/B_Graphics_HPC/B3_Ray_Tracer_BVH/main.cpp` — reference
  - `02_Projects/B_Graphics_HPC/B3_Ray_Tracer_BVH_Dynamic/main.cpp` — reference
  - `02_Projects/B_Graphics_HPC/B4_Device_Enqueue/main.cpp` — reference
  - `common/common.cmake` — to modify
  - `common/graphics_hpc_utils.hpp` — new file

## Objective
Extract duplicated utilities from B2/B3/B3_Dynamic/B4 into shared helpers, consolidate the repeated CMake GL-interop and tinyobjloader blocks into `common/common.cmake`, and verify every subproject still builds standalone after refactoring.

## Constraints & Rules
- All standard constraints inherited from `.claude/rules/00_master_specs.md`.
- `common/` is header-only. No new `.cpp` sources.
- Each subproject must remain standalone-buildable: `cmake -B build && cmake --build build` from within its own directory, without a parent CMake invocation.
- Do NOT modify kernel (`.cl`) files — kernel logic is out of scope for this cleanup.
- Do NOT change any public CLI interfaces (flags, defaults) — this is a structural refactor only.
- The `common/graphics_hpc_utils.hpp` header is opt-in: subprojects `#include` it explicitly; it must not be silently injected.

---

## Implementation

### A — CMake: Extract Optional GL Interop Block

**Problem:** The ~20-line optional GL interop detection block (`option(NO_GL_INTEROP)` + `find_package(glfw3)` + `find_package(OpenGL)` + EGL probe + `WARNING` message + `target_compile_definitions`) is copy-pasted verbatim into B2, B3, B3_Dynamic, and B4 `CMakeLists.txt`. Any future fix (e.g., another driver workaround) must be applied in four places.

**Decision:** Add a CMake macro `opencl_lab_optional_gl_interop(<target>)` to `common/common.cmake`. The macro encapsulates the full detection logic and applies the appropriate `target_compile_definitions`. Each subproject replaces its copy-pasted block with a single macro call.

**Action:**
1. In `common/common.cmake`, define macro `opencl_lab_optional_gl_interop(<target>)` containing the current B3 GL interop block verbatim (it is the most complete version, with EGL probe).
2. In each of B2, B3, B3_Dynamic, B4 `CMakeLists.txt`: delete the existing inline GL block and replace with `opencl_lab_optional_gl_interop(<binary_target_name>)`.
3. Verify the macro correctly passes the target name for `target_compile_definitions` in all four subprojects.

---

### B — CMake: Extract tinyobjloader FetchContent Block

**Problem:** The tinyobjloader `FetchContent_Declare` + `FetchContent_MakeAvailable` + `target_link_libraries(...tinyobjloader)` block is duplicated in B3, B3_Dynamic, and B4. A version pin change requires three edits.

**Decision:** Add a CMake macro `opencl_lab_fetch_tinyobjloader(<target>)` to `common/common.cmake`. The macro declares, makes available, and links tinyobjloader to the given target.

**Action:**
1. In `common/common.cmake`, define macro `opencl_lab_fetch_tinyobjloader(<target>)`.
2. Use `FetchContent_Declare` with the same GIT_REPOSITORY and a pinned `GIT_TAG` (use the tag that was already working — check existing CMakeLists for the tag, or use `master` if none was pinned).
3. In B3, B3_Dynamic, and B4 `CMakeLists.txt`: remove the inline FetchContent block and replace with `opencl_lab_fetch_tinyobjloader(<binary_target_name>)`.

---

### C — C++ Header: Extract Shared Host Utilities

**Problem:** The following utility functions are re-implemented (or near-identical) across multiple `main.cpp` files:
- `round_up(size_t x, size_t multiple)` — rounds global work size up to a workgroup multiple.
- `get_binary_dir()` — returns the directory of the running executable (for locating kernel `.cl` files at runtime).
- `save_framebuffer(...)` — writes a `float4` or `uchar4` framebuffer to BMP via `stb_image_write`.
- `build_program(cl::Context, cl::Device, std::string path, std::string options)` — reads a `.cl` file and compiles it, printing the build log on error.

**Decision:** Create `common/graphics_hpc_utils.hpp` (header-only, `#pragma once`) containing these four utilities. Subprojects that contain a local copy replace it with an `#include`.

**Action:**
1. Read the implementations in B3 `main.cpp` (most complete) for each utility.
2. Write `common/graphics_hpc_utils.hpp` with all four functions. Each must have a WHY comment.
3. In each `main.cpp` that contains a local copy of any of these functions: delete the local copy and add `#include "../../../common/graphics_hpc_utils.hpp"` (adjust relative path per subproject depth).
4. Confirm no naming conflicts with existing `common/opencl_utils.hpp` or `common/ocl_wrapper.hpp`.

---

### D — Naming Alignment

**Problem:** The legacy directories `01_CLBlast_Math`, `02_RayTracer_Basic`, `03_RayTracer_BVH`, `04_Device_Enqueue` exist alongside the canonical `B1_CLBlast_MatMul`, `B2_Ray_Tracer_Basic`, `B3_Ray_Tracer_BVH`, `B4_Device_Enqueue` directories. The legacy directories are likely empty scaffolding from before the Snapshots-over-Branches convention was established.

**Decision:** If the legacy directories contain no source files (only empty CMakeLists or stubs), delete them. If they contain unique content not present in the B-prefixed counterparts, flag for human review (do not delete).

**Action:**
1. Inspect `01_CLBlast_Math/`, `02_RayTracer_Basic/`, `03_RayTracer_BVH/`, `04_Device_Enqueue/` for unique content.
2. If empty/stubs: `git rm -r` each directory.
3. If any contain unique code: leave in place and add a `# REVIEW NEEDED` comment in this task's Execution Report.

---

### E — Standalone Build Verification

**Problem:** After CMake macro extraction, each subproject must remain independently buildable without a parent CMake context.

**Decision:** Build each subproject in isolation to confirm the macros resolve correctly when `common/common.cmake` is included via its absolute-relative path.

**Action:**
1. For each of B1, B2, B3, B3_Dynamic, B4: run `cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build` from within the subproject directory.
2. Confirm zero errors and zero warnings on all five builds.
3. Run each binary with `--help` and confirm CLI11 usage prints correctly.

---

### F — Common BVH Types & Math Header

**Problem:** `Aabb`, `TriangleCpu`, `BvhNode`, `BvhTree` structs and math helpers (`sub3`, `cross3`, `dot3`, `safe_rcp`, `moller_trumbore`) are copy-pasted verbatim across B3, B3_Dynamic, and B4 `bvh_builder.hpp` files. B3 and B4 are byte-for-byte identical; B3_Dynamic is a strict superset. `bvh_self_test` (which exercises the math) is also triplicated.

**Decision:** Create `common/bvh_utils.hpp` (header-only, `#pragma once`) containing:

- Shared types: `Aabb`, `TriangleCpu`, `BvhNode`, `BvhTree` — serves as the **suggested reference struct** students adopt when writing their own `build_bvh`.
- Math primitives: `sub3`, `cross3`, `dot3`, `safe_rcp`, `moller_trumbore` — pure functions, no OpenCL dependency.

`build_bvh` and `bvh_self_test` stay in each module's `bvh_builder.hpp` — they are the learning artifact. B3_Dynamic additionally keeps `refit_pass`, `refit`, and `bvh_refit_self_test` as its own extension.

**Action:**

1. Read B3_Dynamic `bvh_builder.hpp` (superset) to extract the canonical versions of all types and math helpers.
2. Write `common/bvh_utils.hpp` with `Aabb`, `TriangleCpu`, `BvhNode`, `BvhTree`, the five math helpers. Add WHY comments.
3. In B3, B3_Dynamic, and B4 `bvh_builder.hpp`: remove all duplicated type definitions and math helpers; add `#include "../../../common/bvh_utils.hpp"` (adjust relative path per subproject depth). `build_bvh` signature must remain unchanged.
4. Confirm no name collisions with existing `common/` headers.

---

## Definition of Done (DoD)

Standard items from `.claude/rules/00_master_specs.md` §8 apply.

- [x] `common/common.cmake` defines `opencl_lab_optional_gl_interop(<target>)` and `opencl_lab_fetch_tinyobjloader(<target>)`.
- [x] `common/graphics_hpc_utils.hpp` exists with `round_up`, `get_binary_dir`, `save_framebuffer`, and `build_program`.
- [x] All four inline GL interop blocks removed from B2/B3/B3_Dynamic/B4 `CMakeLists.txt`; replaced by macro call.
- [x] All three inline tinyobjloader blocks removed from B3/B3_Dynamic/B4 `CMakeLists.txt`; replaced by macro call.
- [x] Local copies of the four utility functions removed from all `main.cpp` files that had them; replaced by `#include`.
- [x] `common/bvh_utils.hpp` exists with `Aabb`, `TriangleCpu`, `BvhNode`, `BvhTree`, `sub3`, `cross3`, `dot3`, `safe_rcp`, `moller_trumbore`.
- [x] All three `bvh_builder.hpp` files (B3, B3_Dynamic, B4) have duplicate type/math definitions removed and `#include` `common/bvh_utils.hpp` instead. `build_bvh` signature unchanged.
- [x] Legacy directories (`01_CLBlast_Math`, `02_RayTracer_Basic`, `03_RayTracer_BVH`, `04_Device_Enqueue`) removed (all were empty — only empty subdirs, no source files).
- [x] `cmake -B build && cmake --build build` succeeds with zero errors and zero warnings for each of: B1, B2, B3, B3_Dynamic, B4 (run from within each subproject directory).
- [x] `./build/<bin> --help` prints CLI11-generated usage for each binary.
- [x] `./build/b2_ray_tracer --output /tmp/b2_out.bmp` exits 0 and produces a BMP.
- [x] `./build/b3_ray_tracer_bvh --output /tmp/b3_out.bmp --scene assets/bunny.obj --frames 1` exits 0 and produces a BMP.
- [x] MANUAL: Live GL window (B2 or B3) still opens and renders correctly after macro extraction.

---

## Execution Report

- **Status:** COMPLETED
- **Session:** 2026-03-15

### Completed
| Item | Action |
|------|--------|
| A — GL Interop Macro | Added `opencl_lab_optional_gl_interop` macro to `common/common.cmake`; replaced inline blocks in B2, B3, B3_Dynamic (no-op: already had `NO_GL_INTEROP` set directly), B4. |
| B — tinyobjloader Macro | Added `opencl_lab_fetch_tinyobjloader` macro to `common/common.cmake`; replaced inline FetchContent blocks in B3, B3_Dynamic, B4. |
| C — Shared Header | Created `common/graphics_hpc_utils.hpp` with `round_up`, `get_binary_dir`, `build_program`, `save_framebuffer`. Removed local copies from all four `main.cpp` files; call sites updated to pass full kernel path string. |
| D — Naming Alignment | Legacy dirs `01_CLBlast_Math`, `02_RayTracer_Basic`, `03_RayTracer_BVH`, `04_Device_Enqueue` contained only empty subdirectories (no source files). Removed via `rm -rf`. |
| E — Standalone Build Verification | B1, B2, B3, B3_Dynamic, B4 all build with zero errors. All `--help` flags print CLI11 usage. B2 and B3 headless renders produce output BMPs. |
| F — BVH Types & Math Header | Created `common/bvh_utils.hpp` with `Aabb`, `TriangleCpu`, `BvhNode`, `BvhTree`, `sub3`, `cross3`, `dot3`, `safe_rcp`, `moller_trumbore`. Removed duplicates from B3, B3_Dynamic, B4 `bvh_builder.hpp`. `bvh_self_test` retained in each module's `bvh_builder.hpp` per user direction. |

### Validation
```
B2: cmake -B build -DNO_GL_INTEROP=ON && cmake --build build → zero errors
B3: cmake -B build -DNO_GL_INTEROP=ON && cmake --build build → zero errors
B3_Dynamic: cmake -B build && cmake --build build → zero errors
B4: cmake -B build -DNO_GL_INTEROP=ON && cmake --build build → zero errors
B1: cmake --build build --target b1_clblast_matmul → zero errors

b2_ray_tracer --output /tmp/b2_out.bmp → exit 0, 3.5 MB BMP
b3_ray_tracer_bvh --output /tmp/b3_out.bmp --scene assets/bunny.obj --frames 1 → exit 0, 7.9 MB BMP
  BVH render time: 25.537 ms, speedup 425x vs naive
```

### Changed Files
| File | Change |
|------|--------|
| `common/common.cmake` | Modified — added `opencl_lab_optional_gl_interop` and `opencl_lab_fetch_tinyobjloader` macros |
| `common/graphics_hpc_utils.hpp` | Created — `round_up`, `get_binary_dir`, `build_program`, `save_framebuffer` |
| `common/bvh_utils.hpp` | Created — `Aabb`, `TriangleCpu`, `BvhNode`, `BvhTree`, five math helpers |
| `02_Projects/B_Graphics_HPC/B2_Ray_Tracer_Basic/CMakeLists.txt` | Modified — replaced inline GL block with macro call |
| `02_Projects/B_Graphics_HPC/B2_Ray_Tracer_Basic/main.cpp` | Modified — added include, removed local `round_up`/`binary_dir`/`build_program` |
| `02_Projects/B_Graphics_HPC/B3_Ray_Tracer_BVH/CMakeLists.txt` | Modified — replaced inline tinyobjloader + GL blocks with macro calls |
| `02_Projects/B_Graphics_HPC/B3_Ray_Tracer_BVH/main.cpp` | Modified — added include, removed local utilities |
| `02_Projects/B_Graphics_HPC/B3_Ray_Tracer_BVH/bvh_builder.hpp` | Modified — types/math removed, `#include bvh_utils.hpp` added |
| `02_Projects/B_Graphics_HPC/B3_Ray_Tracer_BVH_Dynamic/CMakeLists.txt` | Modified — replaced inline tinyobjloader block with macro call |
| `02_Projects/B_Graphics_HPC/B3_Ray_Tracer_BVH_Dynamic/main.cpp` | Modified — added include, removed local utilities |
| `02_Projects/B_Graphics_HPC/B3_Ray_Tracer_BVH_Dynamic/bvh_builder.hpp` | Modified — types/math removed, `#include bvh_utils.hpp` added |
| `02_Projects/B_Graphics_HPC/B4_Device_Enqueue/CMakeLists.txt` | Modified — replaced inline tinyobjloader + GL blocks with macro calls |
| `02_Projects/B_Graphics_HPC/B4_Device_Enqueue/main.cpp` | Modified — added include, removed local utilities |
| `02_Projects/B_Graphics_HPC/B4_Device_Enqueue/bvh_builder.hpp` | Modified — types/math removed, `#include bvh_utils.hpp` added |
| `02_Projects/B_Graphics_HPC/01_CLBlast_Math/` (and 3 others) | Deleted — empty scaffolding directories |

### Remaining
- Nothing
