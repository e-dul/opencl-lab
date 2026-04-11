# Task T056: New Toolbox Entry — `global_work_offset` & Tiled Benchmark

## Context
- **Design Feature:** `workflow/design/D09_cookbook_v2_pivot.md`
- **Milestone:** Phase 5 — New Toolbox Entry
- **Relevant Files:**
  - `workflow/design/D09_cookbook_v2_pivot.md` — (read-only: architecture spec, "New Toolbox Entry" section)
  - `05_Toolbox/Toolbox.md` — (to modify: add new row to Contents table)
  - `05_Toolbox/LocalMemory/CMakeLists.txt` — (read-only: reference CMake pattern)
  - `05_Toolbox/LocalMemory/main.cpp` — (read-only: reference host code pattern)
  - `common/common.cmake` — (read-only: `opencl_lab_target`, `copy_kernels`, `symlink_assets`)
  - `05_Toolbox/GlobalWorkOffset/` — (new directory: all files created here)

## Objective
Implement the `GlobalWorkOffset` Toolbox recipe: a self-contained recipe that demonstrates launching OpenCL threads exclusively over a sub-region (ROI) of an image buffer using `global_work_offset` + `global_work_size`, and benchmarks a 256×256 tile vs. a full 4K frame using `cl::Event` profiling.

## Constraints & Rules
- All standard constraints from `.claude/rules/00_master_specs.md` apply.
- **No BMP output.** This is a numeric benchmark; the artifact is a structured console timing table (master specs §3 exception).
- **`cl::Event` profiling mandatory** for both tile and full-frame runs (master specs §6). Wall-clock times do not satisfy the performance gate.
- The recipe must be standalone-buildable: `cmake -B build && cmake --build build` from within `05_Toolbox/GlobalWorkOffset/`.
- Sub-module doc (`GlobalWorkOffset.md`) must follow the Toolbox entry template from the design doc (sections: Tool Name, Symptom, Prerequisites, Build & Run, Verify, How it works). Ends with a back-link to `Toolbox.md`.
- No hardcoded platform/device indices. Use `create_context()` from `common/ocl_wrapper.hpp`.
- Kernel GID must use `size_t`, guarded with `if (gid < (size_t)count)`.

---

## Implementation

### A — Directory Scaffold

**Problem:** `05_Toolbox/GlobalWorkOffset/` does not exist.

**Decision:** Mirror the structure of existing Toolbox entries (`LocalMemory/`, `ZeroCopy/`).

**Action:**
1. Create `05_Toolbox/GlobalWorkOffset/` with subdirectory `kernels/`.
2. Create placeholder files: `main.cpp`, `CMakeLists.txt`, `kernels/global_work_offset.cl`, `GlobalWorkOffset.md`.

---

### B — Kernel (`kernels/global_work_offset.cl`)

**Problem:** Need a kernel that uses `get_global_id()` with an offset applied and computes the correct linear buffer index via stride (pitch).

**Decision:** A simple grayscale-invert kernel that writes only to the ROI tile. This makes the offset semantics obvious without obscuring them with complex math.

**Action:**
Write a kernel `roi_invert` with signature:
```c
__kernel void roi_invert(
    __global const uchar* input,
    __global uchar* output,
    int width,          // full image width (stride/pitch)
    int offset_x,       // ROI start column
    int offset_y        // ROI start row
)
```
- `gx = get_global_id(0)`, `gy = get_global_id(1)` — these are 0-based within the tile when an offset is set.
- Compute full-image pixel index: `idx = (offset_y + gy) * width + (offset_x + gx)`.
- WHY comment: explain that `get_global_id()` returns the *work-item index within the NDRange*, which starts at 0 regardless of `global_work_offset`; the offset only controls *which work-items are launched*, not the ID seen inside the kernel. The host offset shifts the dispatch window; the kernel must add the offset manually to compute the correct buffer address.

---

### C — Host Code (`main.cpp`)

**Problem:** Need a benchmark that measures tile vs. full-frame dispatch.

**Decision:** Allocate a synthetic 3840×2160 (4K) grayscale buffer on host, upload once, then run two timed benchmarks in sequence.

**Action:**
1. Parse CLI args via CLI11:
   - `--width` (default 3840), `--height` (default 2160)
   - `--tile-x` (default 256), `--tile-y` (default 256) — tile size
   - `--offset-x` (default 0), `--offset-y` (default 0) — ROI top-left in full image
   - `--iterations` (default 10) — warm-up + timed runs
2. Create context/queue/program via `common/ocl_wrapper.hpp`. Enable `CL_QUEUE_PROFILING_ENABLE`.
3. Allocate `cl::Buffer` for input and output (full frame, `CL_MEM_READ_ONLY` / `CL_MEM_WRITE_ONLY`).
4. Fill input buffer with synthetic data (ramp pattern — host side, before timing).
5. Benchmark loop A — **Tile dispatch**:
   - `global_work_offset = {offset_x, offset_y}`
   - `global_work_size = {tile_x, tile_y}` (round up to `local_work_size` boundary if needed)
   - Record `cl::Event`; accumulate `CL_PROFILING_COMMAND_START` → `CL_PROFILING_COMMAND_END` in nanoseconds.
6. Benchmark loop B — **Full-frame dispatch**:
   - `global_work_offset = {0, 0}`
   - `global_work_size = {width, height}`
   - Same event profiling.
7. Print structured timing table:
```
===== global_work_offset Benchmark =====
Image:        3840 x 2160
Tile:         256 x 256  @ (0, 0)
Iterations:   10

Mode            Avg (ms)    Min (ms)    Max (ms)
------------------------------------------------
Tile (256x256)   X.XXX       X.XXX       X.XXX
Full frame 4K    X.XXX       X.XXX       X.XXX
Speedup:         X.XXx  (tile vs full frame)
```
8. Wrap all OpenCL calls in `CL_CHECK`. Use `CL_HPP_ENABLE_EXCEPTIONS`.
9. Integer safety: buffer size computed as `static_cast<size_t>(width) * height * sizeof(cl_uchar)`.

---

### D — CMake (`CMakeLists.txt`)

**Problem:** New module needs a standalone-buildable CMake project.

**Decision:** Follow the exact pattern from `05_Toolbox/LocalMemory/CMakeLists.txt`.

**Action:**
```cmake
cmake_minimum_required(VERSION 3.18)
project(GlobalWorkOffset CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../common/common.cmake)

add_executable(global_work_offset main.cpp)
opencl_lab_target(global_work_offset)
copy_kernels(global_work_offset)
symlink_assets(global_work_offset)
```

---

### E — Sub-module Doc (`GlobalWorkOffset.md`)

**Problem:** Recipe needs documentation following the Toolbox entry template.

**Decision:** Use the Toolbox entry template from D09 (Symptom → Prerequisites → Build & Run → Verify → How it works), ending with a back-link to `Toolbox.md`.

**Action:** Write `GlobalWorkOffset.md` with:
1. `# global_work_offset & Tiled Benchmark`
2. **Symptom** — Processing a full frame when only a small ROI needs work (waste of GPU threads; in-kernel `if-else` masking still launches dead threads).
3. **Prerequisites** — delta only: OpenCL 1.2+, Module 1 complete.
4. **Build & Run** — standalone cmake commands + example invocation.
5. **Verify** — expected console table format.
6. **How it works** — explain NDRange offset semantics: `global_work_offset` shifts the dispatch window; work-item IDs still start at 0 inside the kernel; kernel must add the offset to compute the correct buffer index. Include the stride/pitch formula.
7. Back-link: `[Back to Toolbox](../Toolbox.md)`.

---

### F — Update `Toolbox.md` Index

**Problem:** `05_Toolbox/Toolbox.md` does not list the new entry.

**Decision:** Append a row to the Contents table and update the "What's Next" link if it still references the old `04_Addons` path.

**Action:**
Add to the Contents table:
```markdown
| [Global Work Offset](GlobalWorkOffset/GlobalWorkOffset.md) | Full-frame dispatch wastes threads when only a small ROI needs work | `GlobalWorkOffset/` |
```
Update "What's Next" to reference `06_Bonus/Bonus.md` instead of `04_Addons/Addons.md`.

---


## Definition of Done (DoD)

Standard items from `.claude/rules/00_master_specs.md §8` apply.

- [x] `cmake -B build && cmake --build build` succeeds with zero errors and zero warnings from `05_Toolbox/GlobalWorkOffset/`.
- [x] `./build/global_work_offset` runs without arguments and prints the structured timing table.
- [x] `./build/global_work_offset --help` prints CLI11-generated usage with all defined flags (`--width`, `--height`, `--tile-x`, `--tile-y`, `--offset-x`, `--offset-y`, `--iterations`).
- [x] Timing table includes both "Tile" and "Full frame" rows with `cl::Event`-sourced times (milliseconds, 3 decimal places) and a speedup ratio.
- [x] `GPU=<vendor> ./build/global_work_offset` selects the correct device without crashing.
- [x] `05_Toolbox/GlobalWorkOffset/GlobalWorkOffset.md` exists and contains all six Toolbox template sections plus back-link.
- [x] `05_Toolbox/Toolbox.md` Contents table contains the new `GlobalWorkOffset` row.
- [x] `05_Toolbox/Toolbox.md` "What's Next" link points to `06_Bonus/Bonus.md`.
- [x] No depth-relative `../../../assets/` paths in any new file.
- [x] `assets/` symlink present in `05_Toolbox/GlobalWorkOffset/build/` after build.
- [x] Use educator agent and readme creation skill to review `05_Toolbox/GlobalWorkOffset/GlobalWorkOffset.md`

---

## Execution Report
<!-- Filled by @coder after implementation. -->

- **Status:** VALIDATED
- **Session:** 2026-03-23

### Completed
| Item | Action |
|------|--------|
| A — Directory Scaffold | Created `05_Toolbox/GlobalWorkOffset/` with `kernels/` subdirectory |
| B — Kernel | `kernels/global_work_offset.cl` — `roi_invert` kernel using `get_global_id()` absolute coords + `get_global_offset()` for tile guard |
| C — Host Code | `main.cpp` — CLI11 parsing, profiling queue, tile + full-frame benchmark loops, structured timing table |
| D — CMake | `CMakeLists.txt` — standalone, mirrors `LocalMemory/` pattern, `opencl_lab_target` + `copy_kernels` + `symlink_assets` |
| E — Sub-module Doc | `GlobalWorkOffset.md` — all six sections (Symptom, Prerequisites, Build & Run, Verify, How it Works, back-link) |
| F — Toolbox.md Update | Added `GlobalWorkOffset` row; "What's Next" points to `06_Bonus/Bonus.md` |

### Validation
```
$ cd 05_Toolbox/GlobalWorkOffset && cmake -B build && cmake --build build
-- Configuring done (1.3s)
-- Generating done (0.0s)
-- Build files have been written to: <repo>/05_Toolbox/GlobalWorkOffset/build
[  0%] Built target CLI11
[ 50%] Building CXX object CMakeFiles/global_work_offset.dir/main.cpp.o
[100%] Linking CXX executable global_work_offset
Copying kernels for global_work_offset
Symlinking assets for global_work_offset
[100%] Built target global_work_offset

$ ./build/global_work_offset
Platform : Intel(R) OpenCL Graphics
Device   : Intel(R) Iris(R) Xe Graphics

===== global_work_offset Benchmark =====
Image:        3840 x 2160
Tile:         256 x 256  @ (0, 0)
Iterations:   10

Mode                  Avg (ms)    Min (ms)    Max (ms)
------------------------------------------------------
Tile (256x256)           0.016       0.009       0.025
Full frame 4K            0.829       0.634       0.988
Speedup:         52.33x  (tile vs full frame)

$ ./build/global_work_offset --help
GlobalWorkOffset — ROI tile vs full-frame dispatch benchmark
Usage: ./build/global_work_offset [OPTIONS]

Options:
  -h,--help                   Print this help message and exit
  --width INT                 Full image width  (default 3840)
  --height INT                Full image height (default 2160)
  --tile-x INT                ROI tile width    (default 256)
  --tile-y INT                ROI tile height   (default 256)
  --offset-x INT              ROI start column  (default 0)
  --offset-y INT              ROI start row     (default 0)
  --iterations INT            Benchmark iterations (default 10)

$ GPU=INTEL ./build/global_work_offset
Platform : Intel(R) OpenCL Graphics  [GPU=INTEL]
Device   : Intel(R) Iris(R) Xe Graphics

===== global_work_offset Benchmark =====
Image:        3840 x 2160
Tile:         256 x 256  @ (0, 0)
Iterations:   10

Mode                  Avg (ms)    Min (ms)    Max (ms)
------------------------------------------------------
Tile (256x256)           0.011       0.009       0.028
Full frame 4K            1.058       0.744       1.427
Speedup:         92.37x  (tile vs full frame)

$ ls -la build/assets
lrwxrwxrwx 1 emil emil 62 mar 23 09:22 build/assets -> <repo>/05_Toolbox/GlobalWorkOffset/../../assets
```

### Changed Files
| File | Change |
|------|--------|
| `05_Toolbox/GlobalWorkOffset/CMakeLists.txt` | Created |
| `05_Toolbox/GlobalWorkOffset/main.cpp` | Created |
| `05_Toolbox/GlobalWorkOffset/kernels/global_work_offset.cl` | Created |
| `05_Toolbox/GlobalWorkOffset/GlobalWorkOffset.md` | Created |
| `05_Toolbox/Toolbox.md` | Modified — new row + updated What's Next link |

### Remaining
- None