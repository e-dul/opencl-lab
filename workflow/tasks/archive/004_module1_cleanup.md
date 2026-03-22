# Task 004: Module 1 — Cleanup & Consolidation

## Context
- **Design Feature:** `workflow/design/01-host-api.md`
- **Milestone:** Phase 4 — Module review and cleanup
- **Relevant Files:**
  - `01_Host_API/01_Visual_Kernel/src/main.cpp`
  - `01_Host_API/02_Visual_Kernel_Events/src/main.cpp`
  - `01_Host_API/03_Buffer_Flags/src/main.cpp`
  - `01_Host_API/01_Visual_Kernel/CMakeLists.txt`
  - `01_Host_API/02_Visual_Kernel_Events/CMakeLists.txt`
  - `01_Host_API/03_Buffer_Flags/CMakeLists.txt`
  - `common/opencl_utils.hpp`
  - `common/ocl_wrapper.hpp`
  - `common/CMakeLists.txt`

## Objective
Remove code duplication across the three Host API samples and tighten the build
system so each module stays standalone-buildable with no boilerplate drift.

---

## Items

### A — Image IO helpers → `common/`
**Problem:** `make_gradient()` and the stb load/save block are copy-pasted
verbatim in `01` and `02`.

**Action:** Extract into `common/image_utils.hpp` (header-only):
- `load_rgb_image(path, width, height, channels)` → `std::vector<uint8_t>`
- `save_bmp(path, data, width, height, channels)`
- `make_gradient(width, height, channels)` → `std::vector<uint8_t>`

Update `01`, `02`, `03` to `#include "image_utils.hpp"` and remove local copies.

---

### B — Event profiling helper → `common/`
**Problem:** `duration_ms(cl::Event)` exists only in `02`; future modules will
re-invent it.

**Action:** Move `duration_ms()` to `common/opencl_utils.hpp` (or a new
`common/profiling_utils.hpp`).

---

### C — CMake kernel copy fix
**Problem:** All three modules use `install(DIRECTORY kernels/ …)`.
`install()` only fires on `cmake --install`; a plain `cmake --build` leaves
`kernels/` absent next to the binary, so `load_kernel_source("kernels/mad.cl")`
fails at runtime unless the user also runs install.

**Action:** Replace with `add_custom_command(TARGET … POST_BUILD …)` as
noted in the design doc:

```cmake
add_custom_command(TARGET <exe> POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/kernels
            $<TARGET_FILE_DIR:<exe>>/kernels
    COMMENT "Copying kernels to build dir"
)
```

Apply to all three CMakeLists.

---

### D — CMake helper macro for boilerplate (optional)
**Problem:** FetchContent stb block + `add_subdirectory(../../common …)` is
copy-pasted across all three CMakeLists.

**Decision needed:** Is a shared `cmake/HostAPICommon.cmake` worth the added
indirection, given the standalone-buildable constraint? If modules can `include()`
a file by relative path, do so. If it breaks standalone builds, skip.

**Action:** Evaluate feasibility; implement only if it does not violate the
standalone-buildable rule.

---

### E — Rename `03_Buffers_Layout` (discuss)
**Problem:** "Layout" implies struct memory layout; the demo is actually about
`CL_MEM_USE_HOST_PTR` vs `CL_MEM_COPY_HOST_PTR` — buffer *flags* / *strategies*.

**Decision:** `03_Buffer_Flags`.

**Action:** Rename `01_Host_API/03_Buffers_Layout/` → `01_Host_API/03_Buffer_Flags/`
and update all references: `HostAPI.md`, `workflow/design/01-host-api.md`, any
README or doc that mentions the old name.

---

### F — Audit obsolete documents
**Action:** Scan `01_Host_API/` for stale / orphaned Markdown. List findings.
Remove or update files that contradict current code.

---

### G — CLI argument parsing → CLI11
**Problem:** `01` and `02` each duplicate a ~45-line hand-rolled `Args` struct +
`parse_args()` function. `03` has no CLI at all (hardcoded `constexpr` constants).

**Decision:** Replace with [CLI11](https://github.com/CLIUtils/CLI11) — header-only,
BSD-3, FetchContent, zero transitive deps.

**Action:**

1. Add CLI11 to `common/common.cmake`:
   ```cmake
   FetchContent_Declare(
       CLI11
       GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
       GIT_TAG        v2.4.2
   )
   FetchContent_MakeAvailable(CLI11)
   ```
   Inside `opencl_lab_target()`, add:
   ```cmake
   target_link_libraries(${TARGET_NAME} PRIVATE
       OpenCL::OpenCL
       common
       CLI11::CLI11   # ← new
   )
   ```

2. Rewrite `01_Visual_Kernel/src/main.cpp` CLI section:
   ```cpp
   #include <CLI/CLI.hpp>
   // ...
   CLI::App app{"MAD image filter (contrast/brightness)"};
   float contrast = 1.0f;  int brightness = 0;
   std::string input_path, kernel = "scalar";
   app.add_option("-c,--contrast",   contrast,    "Contrast multiplier (default: 1.0)");
   app.add_option("-b,--brightness", brightness,  "Brightness addend   (default: 0)");
   app.add_option("-i,--input",      input_path,  "Source image (BMP/PNG/JPG)");
   app.add_option("-k,--kernel",     kernel,      "Kernel: scalar (default) or vec3");
   CLI11_PARSE(app, argc, argv);
   ```
   Remove local `struct Args` and `parse_args()`.

3. Rewrite `02_Visual_Kernel_Events/src/main.cpp` CLI section:
   Same as `01` plus:
   ```cpp
   bool profile = false;
   app.add_flag("-p,--profile", profile, "Enable event profiling");
   ```

4. Rewrite `03_Buffers_Layout/src/main.cpp`:
   Add proper CLI (currently hardcoded):
   ```cpp
   CLI::App app{"Buffer strategy benchmark"};
   float contrast = 1.2f;  int brightness = 10;
   app.add_option("-c,--contrast",   contrast,   "Contrast multiplier (default: 1.2)");
   app.add_option("-b,--brightness", brightness, "Brightness addend   (default: 10)");
   CLI11_PARSE(app, argc, argv);
   ```
   Replace `CONTRAST` / `BRIGHTNESS` constexprs with the parsed values.

---

## Constraints & Rules
- **No Design Changes:** Do not modify `workflow/design/*.md`.
- **Standalone buildable:** Each sub-project must still `cmake -B build && cmake --build build` independently after changes.
- **Language/Standard:** C++17. Header-only additions to `common/` only.
- **CLI11 approved** (see Item G). No other new external libs without explicit approval.
- **stb definitions** (`STB_IMAGE_IMPLEMENTATION`, `STB_IMAGE_WRITE_IMPLEMENTATION`)
  must remain in exactly one `.cpp` per binary — do NOT move them into a header.

## Definition of Done (DoD)
- [x] `common/image_utils.hpp` exists with `load_rgb_image`, `save_bmp`, `make_gradient`.
- [x] `duration_ms()` accessible from `common/` headers.
- [x] All three `CMakeLists.txt` use `add_custom_command(POST_BUILD)` for kernel copy.
- [x] All three modules build cleanly: `cmake -B build && cmake --build build`.
- [x] All three executables run and produce `output.bmp` without errors.
- [x] `03_Buffers_Layout` renamed to `03_Buffer_Flags`; all references updated.
- [x] Obsolete document audit complete; stale files removed or updated.
- [x] No `install(DIRECTORY kernels/ …)` lines remain.
- [x] CLI11 integrated in `common/common.cmake`; `01`, `02`, `03` use `CLI::App` with no local `parse_args()`.

## Execution Report

- **Status:** COMPLETE
- **Session:** 2026-02-23

### Completed
| Item | Action |
|------|--------|
| C — CMake kernel copy fix | Replaced `install(DIRECTORY …)` with `copy_kernels()` POST_BUILD in all three modules |
| D — CMake helper macro | Created `common/common.cmake`; each module now `include()`s it (standalone-safe) |
| — | Fixed `common/CMakeLists.txt` `${CMAKE_SOURCE_DIR}/vendor` → `${CMAKE_CURRENT_SOURCE_DIR}/../vendor` |
| A — Image IO helpers | Created `common/image_utils.hpp` (`load_rgb_image`, `save_bmp`, `make_gradient`); removed local copies from all three modules |
| B — Profiling helper | Added `duration_ms()` to `common/opencl_utils.hpp`; removed local copies from `02` and `03` |
| E — Rename | `01_Host_API/03_Buffers_Layout/` → `01_Host_API/03_Buffer_Flags/`; all doc references updated |
| F — Audit | Removed stub READMEs (`VisualKernel.md`, `VisualKernelEvents.md`, `BuffersLayout.md`); fixed wrong dir names in `HostAPI.md` |
| G — CLI11 | Integrated via `common/common.cmake`; all three modules use `CLI::App`, no local `parse_args()` |

### Changed Files
| File | Change |
|------|--------|
| `common/common.cmake` | Created — `opencl_lab_target()`, `copy_kernels()`, OpenCL + stb + CLI11 wiring |
| `common/CMakeLists.txt` | Fixed vendor include path for standalone builds |
| `common/image_utils.hpp` | Created — `load_rgb_image`, `save_bmp`, `make_gradient` |
| `common/opencl_utils.hpp` | Added `duration_ms()` |
| `01_Host_API/01_Visual_Kernel/CMakeLists.txt` | Rewritten (~40 → 12 lines) |
| `01_Host_API/01_Visual_Kernel/src/main.cpp` | CLI11, image_utils; removed local Args/parse_args/make_gradient |
| `01_Host_API/02_Visual_Kernel_Events/CMakeLists.txt` | Rewritten (~40 → 12 lines) |
| `01_Host_API/02_Visual_Kernel_Events/src/main.cpp` | CLI11, image_utils; removed local Args/parse_args/make_gradient/duration_ms |
| `01_Host_API/03_Buffer_Flags/CMakeLists.txt` | Rewritten (~40 → 12 lines) |
| `01_Host_API/03_Buffer_Flags/src/main.cpp` | CLI11, image_utils; removed constexprs/make_gradient/duration_ms; added argc/argv |
| `01_Host_API/HostAPI.md` | Fixed directory names; updated section headers |
| `workflow/design/01-host-api.md` | Updated Phase 3 name and directory reference |

### Validation
All three modules verified (post-rename, clean build):
- `cmake -B build && cmake --build build` — clean
- Binary executes, produces `output.bmp`
- `--help` prints CLI11-generated usage for all three
- Tested with: `-c 1.5 -b 20`, `-c 1.2 -p`, `-c 1.5 -b 5`
