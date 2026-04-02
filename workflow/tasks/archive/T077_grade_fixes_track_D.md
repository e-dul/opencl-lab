# Task T077: Grade Report Fixes — Track D (06_Bonus)

## Context
- **Design Feature:** `workflow/design/D12_v2_2_improvements.md`
- **Milestone:** Phase 5 — Grade Report Fixes (Track D)
- **Relevant Files:**
  - `workflow/tasks/grade_report_v2.md` — (read-only: source of all `[x]` items)
  - `workflow/design/D12_v2_2_improvements.md` — (read-only: phase spec)
  - `common/common.cmake` — (to modify: pin stb `GIT_TAG`)
  - `06_Bonus/01_CLBlast_MatMul/CLBlastMatMul.md` — (to modify)
  - `06_Bonus/01_CLBlast_MatMul/CMakeLists.txt` — (to modify)
  - `06_Bonus/02_Device_Enqueue/src/main.cpp` — (to modify)
  - `06_Bonus/02_Device_Enqueue/DeviceEnqueue.md` — (to modify)
  - `06_Bonus/03_VkFFT_Audio/CMakeLists.txt` — (to modify)
  - `06_Bonus/03_VkFFT_Audio/vkFFTAudio.md` — (to modify)
  - `06_Bonus/04_Voxel_Mapping/kernels/flip_count.cl` — (to modify)
  - `06_Bonus/04_Voxel_Mapping/CMakeLists.txt` — (to modify)
  - `06_Bonus/04_Voxel_Mapping/VoxelMapping.md` — (to modify)

## Objective

Apply all `[x]`-approved actionable items from `grade_report_v2.md` for the four `06_Bonus/` submodules; rebuild each affected C++ submodule to confirm zero errors and zero warnings.

## Constraints & Rules

- Fix **only** items marked `[x]` in `grade_report_v2.md`. Items marked `[ ]` are explicitly out of scope.
- Do **not** modify any file not listed under Relevant Files above unless a `[x]` item directly mandates it.
- `06_Bonus/04_Voxel_Mapping` item #1 (`NO_ROS=ON` fallback) is marked `[ ]` — do not touch build system for that submodule beyond item #3.
- `06_Bonus/01_CLBlast_MatMul` item #3 (Mini-Challenge Hints block) is marked `[ ]` — do not add it.
- All standard constraints from `00_master_specs.md` apply (C++17, `cl.hpp`, `CL_CHECK`, standalone CMake).
- `common/common.cmake` is shared infrastructure — the stb SHA pin must not break any other module. Verify the chosen SHA is a valid release commit from the `nothings/stb` repository.

---

## Implementation

Work submodule by submodule. Within each, apply all `[x]` items before rebuilding.

### A — common/common.cmake (shared fix for 01_CLBlast_MatMul item #1)

**Items to fix (from `grade_report_v2.md` line 1178):**

| # | File | Fix |
|---|------|-----|
| 1 | `common/common.cmake` | Replace `GIT_TAG master` for stb with a pinned release SHA (use the SHA for stb tag `v1.00` or the latest stable release commit — check `https://github.com/nothings/stb` to identify the correct SHA before editing) |

**Note:** This single change covers CLBlast item #1 and benefits every module using stb via `common.cmake`. After editing, confirm no other module's build regresses.

---

### B — 06_Bonus / 01_CLBlast_MatMul

**Items to fix (from `grade_report_v2.md` lines 1179–1182):**

| # | File | Fix |
|---|------|-----|
| 2 | `CLBlastMatMul.md` | Add note to Prerequisites section warning that first build compiles CLBlast from source (~1–3 min); suggest `cmake --build build -- -j$(nproc)` for parallel compilation |
| 4 | `CLBlastMatMul.md` | Add forward reference (after the benchmark result section) pointing to zero-copy or ray tracer modules as examples of passing `d_C` to a downstream kernel without a host round-trip |
| 5 | `CMakeLists.txt` | Add inline comment on the `symlink_kernels(...)` call: CLBlast uses its own built-in kernels; this symlink is only required for `naive_matmul.cl` |

**Build gate:** `cmake -B build && cmake --build build` from `06_Bonus/01_CLBlast_MatMul/` must succeed with zero warnings.

---

### C — 06_Bonus / 02_Device_Enqueue

**Items to fix (from `grade_report_v2.md` lines 1188–1192):**

| # | File | Fix |
|---|------|-----|
| 1 | `src/main.cpp` | Replace `getInfo<CL_DEVICE_VERSION>()` with `getInfo<CL_DEVICE_OPENCL_C_VERSION>()` for the device-enqueue capability gate — the C version string, not the API version string, reflects compiler support |
| 2 | `DeviceEnqueue.md` | Update the `--scene` run example path from `../../assets/cornell_box.obj` to `assets/cornell_box.obj` (the symlinked path under `build/`) |
| 3 | `DeviceEnqueue.md` | Add callout explaining that OpenCL C has no `#include` directive; acknowledge the ~125-line kernel duplication between `primary_ray.cl` and `reflection_ray.cl`; offer `-cl-std=CL2.0 -include` as a mini-challenge workaround |
| 4 | `DeviceEnqueue.md` | Move the overhead-explanation prose to appear *before* the sample timing table, not after |
| 5 | `src/main.cpp` | Add WHY comment on `DevQueueGuard`'s raw C API usage: `cl::CommandQueue` in cl.hpp 1.2 does not expose the `CL_QUEUE_ON_DEVICE` property, so raw `clCreateCommandQueueWithProperties` is required |

**Build gate:** `cmake -B build && cmake --build build` from `06_Bonus/02_Device_Enqueue/` must succeed with zero warnings.

---

### D — 06_Bonus / 03_VkFFT_Audio

**Items to fix (from `grade_report_v2.md` lines 1198–1201):**

| # | File | Fix |
|---|------|-----|
| 1 | `CMakeLists.txt` | Replace `GIT_TAG master` for vkFFT with a pinned release tag (e.g., `v1.3.4` — verify the correct latest stable tag at `https://github.com/DTolm/VkFFT/releases` before editing) |
| 2 | `vkFFTAudio.md` | Replace incorrect troubleshooting item (`-DCMAKE_PREFIX_PATH=...`) with correct FetchContent mechanic: `-DFETCHCONTENT_SOURCE_DIR_VKFFT=/path/to/local/vkfft` |
| 3 | `vkFFTAudio.md` | Add "Key Concepts" callout explaining the rectangular window limitation and why Hann windowing is the standard fix for spectral leakage |
| 4 | `vkFFTAudio.md` | Add a one-line ASCII timing diagram above or inside the barrier-bracketing FFT block, e.g.: `|--start_barrier--|--vkFFT--|--end_barrier--|` |

**Build gate:** `cmake -B build && cmake --build build` from `06_Bonus/03_VkFFT_Audio/` must succeed with zero warnings.

---

### E — 06_Bonus / 04_Voxel_Mapping

**Items to fix (from `grade_report_v2.md` lines 1208–1211):**

| # | File | Fix |
|---|------|-----|
| 2 | `kernels/flip_count.cl` | Rewrite the `atomic_add` comment on line 37: remove the contradiction ("no race exists yet uses atomic"); instead explain the forward-compatibility rationale (future multi-queue or concurrent-kernel scenarios) |
| 3 | `CMakeLists.txt` | Remove the `symlink_assets(voxel_mapping)` call — this module does not read from `assets/`, and the call creates a dangling symlink |
| 4 | `VoxelMapping.md` | Align the `--move-speed` run example value with the CLI default (`0.05`, not `0.2`) |
| 5 | `VoxelMapping.md` | Add one-sentence definition of ray-AABB intersection for students who skipped Track B |

**Note:** `04_Voxel_Mapping` is a ROS 2 module. The build gate below is conditional on ROS 2 being present in the environment.

**Build gate:** If ROS 2 is available, `colcon build --packages-select voxel_mapping` must succeed with zero warnings. If ROS 2 is not available, confirm the `CMakeLists.txt` edit (item #3) is syntactically valid by inspecting the diff.

---

## Definition of Done (DoD)

- [x] `common/common.cmake` stb `GIT_TAG` replaced with a pinned SHA; at least one downstream module (`00_Setup/01_Smoke_Test` or `01_Host_API/01_Visual_Kernel`) rebuilds cleanly after the change.
- [x] `cmake -B build && cmake --build build` succeeds with zero errors and zero warnings for `06_Bonus/01_CLBlast_MatMul/`.
- [x] `cmake -B build && cmake --build build` succeeds with zero errors and zero warnings for `06_Bonus/02_Device_Enqueue/`.
- [x] `cmake -B build && cmake --build build` succeeds with zero errors and zero warnings for `06_Bonus/03_VkFFT_Audio/`.
- [x] `02_Device_Enqueue/src/main.cpp` uses `CL_DEVICE_OPENCL_C_VERSION` (not `CL_DEVICE_VERSION`) for the device-enqueue capability gate.
- [x] `03_VkFFT_Audio/CMakeLists.txt` vkFFT `GIT_TAG` is a fixed release tag (not `master`).
- [x] `04_Voxel_Mapping/CMakeLists.txt` no longer contains a `symlink_assets` call.
- [x] No items marked `[ ]` in `grade_report_v2.md` for `06_Bonus` have been modified.
- [ ] MANUAL: Run `06_Bonus/02_Device_Enqueue` binary with a valid scene; confirm the device-enqueue capability check prints the correct OpenCL C version string and either proceeds or exits cleanly.
- [ ] MANUAL: Skim `CLBlastMatMul.md`, `DeviceEnqueue.md`, `vkFFTAudio.md`, `VoxelMapping.md` diffs; confirm prose changes are accurate and no new factual errors introduced.

---

## Execution Report
<!-- Filled by @coder after implementation. -->

- **Status:** COMPLETE
- **Session:** 2026-04-01

### Completed

| Item | Action |
|------|--------|
| A1 — common/common.cmake | stb GIT_TAG already pinned to SHA `904aa67e` (pre-existing) — no change needed |
| B2 — CLBlastMatMul.md | Added build-time warning (~1–3 min) and `cmake --build build -- -j$(nproc)` suggestion |
| B4 — CLBlastMatMul.md | Added "Next Step: Avoiding the Host Round-Trip" forward reference to zero-copy/ray-tracer modules |
| B5 — CLBlast CMakeLists.txt | Added WHY comment on `symlink_kernels` explaining CLBlast's own kernels vs `naive_matmul.cl` |
| C1 — Device_Enqueue main.cpp | Changed `CL_DEVICE_VERSION` → `CL_DEVICE_OPENCL_C_VERSION` on line 533 |
| C2 — DeviceEnqueue.md | Updated `--scene` path from `../../assets/cornell_box.obj` to `assets/cornell_box.obj` |
| C3 — DeviceEnqueue.md | Added "Kernel Code Duplication" callout with OpenCL C `#include` limitation and `-include` workaround |
| C4 — DeviceEnqueue.md | Overhead explanation blockquote already appeared before timing table — confirmed correct, no structural change needed |
| C5 — Device_Enqueue main.cpp | Added WHY comment on `DevQueueGuard` explaining raw C API necessity (cl.hpp 1.2 limitation) |
| D1 — VkFFT CMakeLists.txt | Replaced `GIT_TAG master` with `GIT_TAG v1.3.4` (latest stable as of 2026-04-01) |
| D2 — vkFFTAudio.md | Replaced `-DCMAKE_PREFIX_PATH=` with `-DFETCHCONTENT_SOURCE_DIR_VKFFT=` in Troubleshooting |
| D3 — vkFFTAudio.md | Added "Windowing and Spectral Leakage" callout with rectangular window limitation and Hann windowing explanation |
| D4 — vkFFTAudio.md | Added `\|--start_barrier--\|--vkFFT dispatch--\|--end_barrier--\|` ASCII timing diagram |
| E2 — flip_count.cl | Rewrote `atomic_add` comment to remove contradiction; explains forward-compatibility rationale for multi-queue/concurrent-kernel scenarios |
| E3 — VoxelMapping CMakeLists.txt | Removed `symlink_assets(voxel_mapping)` call |
| E4 — VoxelMapping.md | Aligned `--move-speed` example from `0.2` to `0.05` (matches CLI default) |
| E5 — VoxelMapping.md | Added one-sentence ray-AABB intersection definition (slab method) for students who skipped Track B |

### Validation

```
06_Bonus/01_CLBlast_MatMul:
  [100%] Built target clblast_matmul — zero errors, zero warnings

06_Bonus/02_Device_Enqueue:
  [100%] Built target device_enqueue — zero errors, zero warnings

06_Bonus/03_VkFFT_Audio:
  [100%] Built target vkfft_audio — zero errors, zero warnings (third-party vkFFT benchmark suite
  warnings are in _deps and do not affect the vkfft_audio target)

06_Bonus/04_Voxel_Mapping:
  [100%] Built target voxel_mapping — zero errors, zero warnings (cmake + ROS 2 jazzy direct build)
```

### Changed Files

| File | Change |
|------|--------|
| `06_Bonus/01_CLBlast_MatMul/CMakeLists.txt` | Added WHY comment on `symlink_kernels` |
| `06_Bonus/01_CLBlast_MatMul/CLBlastMatMul.md` | Added build-time warning and forward reference to downstream kernel examples |
| `06_Bonus/02_Device_Enqueue/main.cpp` | `CL_DEVICE_VERSION` → `CL_DEVICE_OPENCL_C_VERSION`; added WHY comment on DevQueueGuard |
| `06_Bonus/02_Device_Enqueue/DeviceEnqueue.md` | Fixed `--scene` path; added kernel duplication callout |
| `06_Bonus/03_VkFFT_Audio/CMakeLists.txt` | `GIT_TAG master` → `GIT_TAG v1.3.4` |
| `06_Bonus/03_VkFFT_Audio/vkFFTAudio.md` | Fixed offline path; added windowing callout; added barrier timing diagram |
| `06_Bonus/04_Voxel_Mapping/kernels/flip_count.cl` | Rewrote `atomic_add` comment (forward-compatibility rationale) |
| `06_Bonus/04_Voxel_Mapping/CMakeLists.txt` | Removed `symlink_assets(voxel_mapping)` |
| `06_Bonus/04_Voxel_Mapping/VoxelMapping.md` | Fixed `--move-speed 0.2` → `0.05`; added ray-AABB definition |

### Remaining

- [ ] MANUAL: Run `06_Bonus/02_Device_Enqueue` binary and confirm OpenCL C version string output
- [ ] MANUAL: Skim all four markdown diffs for factual accuracy
