# Task 010: Toolbox — WorkGroupSizing

## Context
- **Design Feature:** `workflow/design/07-toolbox.md`
- **Milestone:** Phase 5 — WorkGroupSizing
- **Relevant Files:**
  - `workflow/design/07-toolbox.md` — (read-only: architecture & performance gate)
  - `.claude/rules/00_master_specs.md` — (read-only: global constraints)
  - `99_Toolbox/WorkGroupSizing/WorkGroupSizing.md` — (read-only: user-facing README, defines expected console output format)
  - `common/ocl_wrapper.hpp` — (read-only: `create_context()`, GPU env var)
  - `common/opencl_utils.hpp` — (read-only: `CL_CHECK` macro)
  - `99_Toolbox/WorkGroupSizing/CMakeLists.txt` — (new file)
  - `99_Toolbox/WorkGroupSizing/main.cpp` — (new file)
  - `99_Toolbox/WorkGroupSizing/kernels/mad_kernel.cl` — (new file)

## Objective

Implement the WorkGroupSizing tool: an automated sweep of `local_work_size` values for a MAD kernel over a 1D buffer, reporting kernel time (ms, 3 d.p.) and a software occupancy estimate at each step, with at least a 2× gap visible between the worst and best work-group sizes.

## Constraints & Rules
- **No Design Changes:** Do not modify `workflow/design/*.md`. If architecture is wrong, stop and ask.
- **Language/Standard:** C++17 strict.
- **OpenCL:** 1.2 baseline; `cl.hpp` C++ bindings only. Raw `clCreate*` / `clRelease*` calls are FORBIDDEN.
- **CLI:** CLI11 (v2.4.2, FetchContent). Required flags: `--width` (default 1920), `--height` (default 1080). Optional: `--kernel <mad|blur_r5>` (default `mad`).
- **Profiling:** `CL_QUEUE_PROFILING_ENABLE` + `cl::Event` timing exclusively. Wall-clock measurements do not satisfy the performance gate.
- **Global Work Size Padding:** The sweep loop must pad `global_work_size` to the next multiple of the tested `local_work_size` to avoid `CL_INVALID_WORK_GROUP_SIZE`. Do not hard-code padding.
- **Device Query:** Read `CL_DEVICE_MAX_WORK_GROUP_SIZE` at runtime. The sweep must not test sizes exceeding this limit.
- **GPU Selection:** `common/ocl_wrapper.hpp → create_context()` only. Hard-coded device indices are FORBIDDEN.
- **Error Handling:** `CL_CHECK()` on every OpenCL call; throw `std::runtime_error` on failure.
- **No BMP Output:** This is a numeric tool. The artifact is the structured console timing table.
- **Standalone Build:** The tool must build independently via `cmake -B build && cmake --build build` from `99_Toolbox/WorkGroupSizing/`.

---

## Implementation

1. **Kernel (`kernels/mad_kernel.cl`)**
   - Write a single kernel `mad_kernel(__global float* buf, int n)` that performs a fused multiply-add (`a * b + c`) on every element of `buf`.
   - The kernel must be memory-latency-bound (not compute-bound) so that occupancy tuning is visible: perform 1 read + 1 write per work-item, no loop unrolling.

2. **`CMakeLists.txt`**
   - `cmake_minimum_required(VERSION 3.18)`, `project(occupancy_demo CXX)`.
   - `set(CMAKE_CXX_STANDARD 17)`.
   - `find_package(OpenCL REQUIRED)`.
   - Include `common/common.cmake` (path relative to repo root via `${CMAKE_CURRENT_SOURCE_DIR}/../../common/common.cmake`).
   - FetchContent for CLI11 v2.4.2 (header-only).
   - Link: `OpenCL::OpenCL`, `CLI11::CLI11`.
   - Post-build kernel copy command (copies `kernels/` to binary dir) per master specs.

3. **`main.cpp` — host code**
   - Parse CLI args: `--width` (int, default 1920), `--height` (int, default 1080), `--kernel` (string, default `"mad"`).
   - Compute `n = width * height` (total elements, 1D workload).
   - Create context + queue via `create_context()`. Enable `CL_QUEUE_PROFILING_ENABLE`.
   - Allocate `cl::Buffer` (RAII) for `float` array of size `n`.
   - Load and build `kernels/mad_kernel.cl`.
   - Query `CL_DEVICE_MAX_WORK_GROUP_SIZE` → cap sweep at that limit.
   - Sweep `local_work_size` over `{8, 16, 32, 64, 128, 256}` (skip sizes > device max):
     - Compute padded global size = `ceil(n / lws) * lws`.
     - Enqueue kernel with `cl::Event`; call `queue.finish()`.
     - Extract `CL_PROFILING_COMMAND_START` / `CL_PROFILING_COMMAND_END` → ms (3 d.p.).
     - Compute software occupancy estimate = `(lws / device_max_wgs) * 100.0` (percent).
   - Print structured table to stdout matching the format in `WorkGroupSizing.md`:
     ```
     local_work_size=  8: XX.XXX ms  occupancy: XX.X%
     ```
   - After the sweep, print the line: `Sweet spot: local_work_size=<N> at <T> ms`.

---

## Definition of Done (DoD)

- [ ] `cmake -B build && cmake --build build` succeeds without warnings from `99_Toolbox/WorkGroupSizing/`.
- [ ] `./build/occupancy_demo` runs without flags (uses 1920×1080 defaults) and prints a timing table with one row per tested `local_work_size`.
- [ ] `./build/occupancy_demo --help` prints CLI11-generated usage listing `--width`, `--height`, `--kernel`.
- [ ] The timing table shows at least a 2× gap between the slowest and fastest `local_work_size` on any OpenCL-capable device (gate from design doc).
- [ ] All timing values are from `cl::Event` profiling (not `std::chrono`); the table reports ms to 3 decimal places.
- [ ] No `local_work_size` exceeding `CL_DEVICE_MAX_WORK_GROUP_SIZE` is tested; no `CL_INVALID_WORK_GROUP_SIZE` error occurs.
- [ ] `GPU` env var is respected: `GPU=NVIDIA ./build/occupancy_demo` selects the Nvidia device if present.
- [ ] The binary is a standalone executable — no dependency on other Toolbox tool directories.

---

## Execution Report
<!-- Filled by @coder after implementation. -->

- **Status:** PENDING
  <!-- PENDING → IN PROGRESS → COMPLETED -->
- **Session:** [YYYY-MM-DD]

### Validation
<!-- Paste terminal output, test results, or observable evidence. -->
```
[output here]
```

### Changed Files
| File | Change |
|------|--------|
| `99_Toolbox/WorkGroupSizing/CMakeLists.txt` | Created |
| `99_Toolbox/WorkGroupSizing/main.cpp` | Created |
| `99_Toolbox/WorkGroupSizing/kernels/mad_kernel.cl` | Created |

### Remaining
- [ ] [Remaining item]
