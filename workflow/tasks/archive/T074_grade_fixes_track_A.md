# Task T074: Grade Report Fixes — Track A (00_Setup + 01_Host_API)

## Context
- **Design Feature:** `workflow/design/D12_v2_2_improvements.md`
- **Milestone:** Phase 5 — Grade Report Fixes (Track A)
- **Relevant Files:**
  - `workflow/tasks/grade_report_v2.md` — (read-only: source of all `[x]` items)
  - `workflow/design/D12_v2_2_improvements.md` — (read-only: phase spec)
  - `00_Setup/01_Smoke_Test/kernels/vector_add.cl` — (to modify)
  - `00_Setup/01_Smoke_Test/src/main.cpp` — (to modify)
  - `00_Setup/01_Smoke_Test/CMakeLists.txt` — (to modify)
  - `01_Host_API/01_Visual_Kernel/kernels/mad.cl` — (to modify)
  - `01_Host_API/01_Visual_Kernel/src/main.cpp` — (to modify)
  - `01_Host_API/01_Visual_Kernel/CMakeLists.txt` — (to modify)
  - `01_Host_API/01_Visual_Kernel/VisualKernel.md` — (to modify)
  - `01_Host_API/02_Visual_Kernel_Events/kernels/mad.cl` — (to modify)
  - `01_Host_API/02_Visual_Kernel_Events/src/main.cpp` — (to modify)
  - `01_Host_API/02_Visual_Kernel_Events/CMakeLists.txt` — (to modify)
  - `01_Host_API/02_Visual_Kernel_Events/VisualKernelEvents.md` — (to modify)
  - `01_Host_API/03_Buffer_Flags/kernels/mad.cl` — (to modify)
  - `01_Host_API/03_Buffer_Flags/src/main.cpp` — (to modify)
  - `01_Host_API/03_Buffer_Flags/CMakeLists.txt` — (to modify)
  - `01_Host_API/03_Buffer_Flags/BufferFlags.md` — (to modify)
  - `common/opencl_utils.hpp` — (read-only: `build_program()` reference)
  - `common/common.cmake` — (read-only: `symlink_kernels` / `opencl_lab_target` reference)

## Objective

Apply all `[x]`-approved actionable items from `grade_report_v2.md` for the `00_Setup/01_Smoke_Test` and `01_Host_API/` submodules; rebuild each affected submodule to confirm zero errors and zero warnings.

## Constraints & Rules

- Fix **only** items marked `[x]` in `grade_report_v2.md`. Items marked `[ ]` are explicitly out of scope.
- Do **not** modify any file not listed under Relevant Files above unless a `[x]` item directly mandates it.
- All standard constraints from `00_master_specs.md` apply (C++17, `cl.hpp`, `CL_CHECK`, `size_t gid`, standalone CMake).

---

## Implementation

Work submodule by submodule. Within each, apply all `[x]` items before rebuilding.

### A — 00_Setup / 01_Smoke_Test

**Items to fix (from `grade_report_v2.md` lines 868–873):**

| # | File | Fix |
|---|------|-----|
| 1 | `kernels/vector_add.cl` | Change `int i = get_global_id(0)` → `size_t i = get_global_id(0)`; add bounds guard `if (i < (size_t)n)` |
| 2 | `src/main.cpp` | Wrap `setArg`, `enqueueNDRangeKernel`, `finish()`, `enqueueReadBuffer` in `CL_CHECK(...)` |
| 4 | `CMakeLists.txt` | Replace `file(COPY ...)` with `add_custom_command POST_BUILD create_symlink` pattern (use `symlink_kernels()` from `common.cmake` if already included; otherwise inline the `cmake -E create_symlink` command per §1) |
| 5 | `CMakeLists.txt` | Add `set(CMAKE_CXX_EXTENSIONS OFF)` alongside `set(CMAKE_CXX_STANDARD 17)` |
| 6 | `src/main.cpp` | Add WHY comment on `CL_MEM_READ_ONLY \| CL_MEM_COPY_HOST_PTR` line explaining why both flags are combined |

**Note on item #3 ([ ] excluded):** `platforms.front()` / `devices.front()` is intentionally retained to keep the smoke test self-contained. Do not touch it.

**Build gate:** `cmake -B build && cmake --build build` from `00_Setup/01_Smoke_Test/` must succeed with zero warnings.

---

### B — 01_Host_API / 01_Visual_Kernel

**Items to fix (from `grade_report_v2.md` lines 880–885):**

| # | File | Fix |
|---|------|-----|
| 1 | `kernels/mad.cl` | Both kernels: change `int gid = get_global_id(0)` → `size_t gid`; add bounds guard per §7.4 |
| 2 | `src/main.cpp` | Wrap `setArg` (×4), `enqueueNDRangeKernel`, `finish()`, `enqueueReadBuffer` in `CL_CHECK(...)` |
| 3 | `src/main.cpp` | Change `static_cast<size_t>(width * height * channels)` → `static_cast<size_t>(width) * height * channels` |
| 4 | `CMakeLists.txt` | Add `set(CMAKE_CXX_EXTENSIONS OFF)` alongside `set(CMAKE_CXX_STANDARD 17)` |
| 5 | `src/main.cpp` | Replace hand-rolled `program.build()` / `getBuildInfo` try-catch with `build_program()` from `common/opencl_utils.hpp` |
| 6 | `VisualKernel.md` | Add inline cross-references to `main.cpp` line numbers for each of the seven steps (e.g., "Step 3 — see `main.cpp` line N") |

**Build gate:** `cmake -B build && cmake --build build` from `01_Host_API/01_Visual_Kernel/` with zero warnings.

---

### C — 01_Host_API / 02_Visual_Kernel_Events

**Items to fix (from `grade_report_v2.md` lines 891–896):**

| # | File | Fix |
|---|------|-----|
| 1 | `kernels/mad.cl` | Both kernels: change `int gid` → `size_t gid`; add bounds guard in vec3 kernel (which currently has none) per §7.4 |
| 2 | `src/main.cpp` | Change `static_cast<size_t>(width * height * channels)` → `static_cast<size_t>(width) * height * channels`; add `if (pixel_count > INT_MAX) throw std::runtime_error(...)` guard per §7.1 |
| 3 | `src/main.cpp` | Wrap `setArg`, `finish()`, `enqueueWriteBuffer`, `enqueueNDRangeKernel`, `enqueueReadBuffer` in `CL_CHECK(...)` |
| 4 | `CMakeLists.txt` | Add `set(CMAKE_CXX_EXTENSIONS OFF)` |
| 5 | `src/main.cpp` | Replace hand-rolled `program.build()` try-catch with `build_program()` |
| 6 | `VisualKernelEvents.md` | Relabel timing diagram: clarify that all three `enqueue*` calls are issued before `queue.finish()` blocks (the arrows currently imply sequential blocking) |

**Build gate:** `cmake -B build && cmake --build build` from `01_Host_API/02_Visual_Kernel_Events/` with zero warnings.

---

### D — 01_Host_API / 03_Buffer_Flags

**Items to fix (from `grade_report_v2.md` lines 902–907):**

| # | File | Fix |
|---|------|-----|
| 1 | `kernels/mad.cl` | Change `int gid` → `size_t gid`; add bounds guard `if (gid < total_bytes)` per §7.4 |
| 2 | `src/main.cpp` | Change `static_cast<size_t>(width * height * channels)` → `static_cast<size_t>(width) * height * channels` per §7.1 |
| 3 | `src/main.cpp` | Wrap `setArg`, `enqueueWriteBuffer`, `enqueueNDRangeKernel`, `enqueueReadBuffer` in `CL_CHECK(...)` across all three strategy branches |
| 4 | `CMakeLists.txt` | Add `set(CMAKE_CXX_EXTENSIONS OFF)` |
| 5 | `src/main.cpp` | Replace hand-rolled `program.build()` try-catch with `build_program()` |
| 6 | `BufferFlags.md` | Add sentence in Verify section explaining why `*`-marked rows carry a hidden cost (`CL_MEM_COPY_HOST_PTR` triggers an immediate DMA upload at buffer-creation time) |

**Build gate:** `cmake -B build && cmake --build build` from `01_Host_API/03_Buffer_Flags/` with zero warnings.

---

## Definition of Done (DoD)

Standard items from `00_master_specs.md §8` apply to all four submodules.

- [x] `cmake -B build && cmake --build build` passes with zero errors and zero warnings in `00_Setup/01_Smoke_Test/`.
- [x] `cmake -B build && cmake --build build` passes with zero errors and zero warnings in `01_Host_API/01_Visual_Kernel/`.
- [x] `cmake -B build && cmake --build build` passes with zero errors and zero warnings in `01_Host_API/02_Visual_Kernel_Events/`.
- [x] `cmake -B build && cmake --build build` passes with zero errors and zero warnings in `01_Host_API/03_Buffer_Flags/`.
- [x] Each binary runs without arguments and exits cleanly (BMP output produced for `01_Visual_Kernel` and `02_Visual_Kernel_Events`; timing table produced for `03_Buffer_Flags`).
- [x] `grep -rn "int i = get_global_id\|int gid = get_global_id" 00_Setup/ 01_Host_API/` returns zero matches.
- [x] `grep -rn "CMAKE_CXX_EXTENSIONS" 00_Setup/01_Smoke_Test/CMakeLists.txt 01_Host_API/01_Visual_Kernel/CMakeLists.txt 01_Host_API/02_Visual_Kernel_Events/CMakeLists.txt 01_Host_API/03_Buffer_Flags/CMakeLists.txt` shows `OFF` in all four files.
- [x] `grep -rn "file(COPY" 00_Setup/01_Smoke_Test/CMakeLists.txt` returns zero matches (symlink pattern in place). *(matches are in comments only — no functional `file(COPY` directive)*
- [x] No files outside the four submodule directories listed under Relevant Files are modified.
- [x] Items marked `[ ]` in `grade_report_v2.md` (items #3 and #7 for `00_Setup`) remain untouched.

---

## Execution Report
<!-- Filled by @coder after validation. -->

- **Status:** VALIDATED
- **Session:** 2026-03-31

### Completed

| Item | Action |
|------|--------|
| All DoD items | Verified pass — see Validation section below |

### Validation

```
00_Setup/01_Smoke_Test:
  cmake --build build → [100%] Built target smoke_test  (0 errors, 0 warnings)
  ./build/smoke_test  → SUCCESS: Vector Add (1.0 + 2.0 = 3.0) verified!

01_Host_API/01_Visual_Kernel:
  cmake --build build → [100%] Built target visual_kernel  (0 errors, 0 warnings)
  ./build/visual_kernel → Written: output.bmp (256x256)  exit 0

01_Host_API/02_Visual_Kernel_Events:
  cmake --build build → [100%] Built target visual_kernel_events  (0 errors, 0 warnings)
  ./build/visual_kernel_events → Written: output.bmp (256x256)  exit 0

01_Host_API/03_Buffer_Flags:
  cmake --build build → [100%] Built target buffer_flags  (0 errors, 0 warnings)
  ./build/buffer_flags → timing table printed, Written: output.bmp  exit 0

grep int get_global_id  → exit 1 (zero matches — PASS)
grep CMAKE_CXX_EXTENSIONS → OFF in all four files — PASS
grep file(COPY (functional) → zero matches — PASS (comment-only references remain)
```

### Changed Files

| File | Change |
|------|--------|
| — | Validation only; no source files modified |

### Remaining

- [x] All four submodules built and validated.
