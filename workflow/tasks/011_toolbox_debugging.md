# Task 011: Toolbox — Debugging

## Context
- **Design Feature:** `workflow/design/07-toolbox.md`
- **Milestone:** Phase 6 — Debugging
- **Relevant Files:**
  - `workflow/design/07-toolbox.md` — (read-only: architecture & verification standard)
  - `.claude/rules/00_master_specs.md` — (read-only: global constraints)
  - `99_Toolbox/Debugging/Debugging.md` — (read-only: user-facing README, defines expected Oclgrind output format and CLI flags)
  - `common/ocl_wrapper.hpp` — (read-only: `create_context()`, GPU env var)
  - `common/opencl_utils.hpp` — (read-only: `CL_CHECK` macro)
  - `99_Toolbox/Debugging/CMakeLists.txt` — (new file)
  - `99_Toolbox/Debugging/main.cpp` — (new file)
  - `99_Toolbox/Debugging/kernels/debug_kernel.cl` — (new file)

## Objective

Implement the Debugging tool: a binary with two injected-bug modes (`--test out_of_bounds` and `--test race_condition`) designed to be run under Oclgrind, where the Oclgrind error report is the verification artifact — not visual output and not a timing table.

## Constraints & Rules
- **No Design Changes:** Do not modify `workflow/design/*.md`. If architecture is wrong, stop and ask.
- **Language/Standard:** C++17 strict.
- **OpenCL:** 1.2 baseline; `cl.hpp` C++ bindings only. Raw `clCreate*` / `clRelease*` calls are FORBIDDEN.
- **CLI:** CLI11 (v2.4.2, FetchContent). Required flag: `--test <out_of_bounds|race_condition>` (string, required; no default — must be specified by the user).
- **No BMP output, no timing table:** The artifact is the Oclgrind error report. This is explicitly noted as the exception in design doc §Specifications.
- **Oclgrind Optional at Build Time:** CMake must NOT fail if Oclgrind is absent. Emit `message(WARNING ...)` only. The binary builds and runs on real hardware; Oclgrind is only required at runtime for verification.
- **Injected Bugs Must Be Real and Detectable:** The out-of-bounds write must exceed the allocated buffer by at least one element. The race condition must involve concurrent, unsynchronized writes from different work-items to the same address.
- **No `printf` in kernels** (master specs §4): injected bugs must manifest as memory errors only, not via kernel-side printing.
- **GPU Selection:** `common/ocl_wrapper.hpp → create_context()` only. Hard-coded device indices are FORBIDDEN.
- **Error Handling:** `CL_CHECK()` on every OpenCL call; throw `std::runtime_error` on host-side CL failures.
- **Standalone Build:** The tool must build independently via `cmake -B build && cmake --build build` from `99_Toolbox/Debugging/`.

---

## Implementation

1. **Kernel (`kernels/debug_kernel.cl`)**
   - Write a single kernel `debug_kernel(__global float* buf, int n, int mode)`:
     - `mode == 0` (out-of-bounds): work-item 0 writes to `buf[n]` — one element past the end of the allocation.
     - `mode == 1` (race condition): all work-items write the value `1.0f` to `buf[0]` without synchronization (unsynchronized concurrent write).
   - All other work-items perform a valid write (`buf[id] = (float)id`) when not triggering a bug.
   - The bugs must be live (not dead code): the `mode` argument is passed at runtime, not compile-time, so the compiler cannot eliminate either path.

2. **`CMakeLists.txt`**
   - `cmake_minimum_required(VERSION 3.18)`, `project(debug_demo CXX)`.
   - `set(CMAKE_CXX_STANDARD 17)`.
   - `find_package(OpenCL REQUIRED)`.
   - Include `common/common.cmake` (path: `${CMAKE_CURRENT_SOURCE_DIR}/../../common/common.cmake`).
   - FetchContent for CLI11 v2.4.2 (header-only).
   - Link: `OpenCL::OpenCL`, `CLI11::CLI11`.
   - Post-build kernel copy command (copies `kernels/` to binary dir) per master specs.
   - Optional Oclgrind detection: `find_program(OCLGRIND_EXECUTABLE oclgrind)`. If not found, emit `message(WARNING "oclgrind not found — runtime verification unavailable. Install with: sudo apt install oclgrind")`. Do NOT use `REQUIRED`.

3. **`main.cpp` — host code**
   - Parse CLI args: `--test` (string, required; allowed values: `out_of_bounds`, `race_condition`).
   - If `--test` value is not one of the two allowed values, print an error message and exit non-zero.
   - Create context + queue via `create_context()`. Do NOT enable `CL_QUEUE_PROFILING_ENABLE` (this tool has no timing requirement).
   - Allocate `cl::Buffer` for `float[64]` (small fixed size — Oclgrind instruments all accesses regardless of size; 1080p is wasteful here).
   - Load and build `kernels/debug_kernel.cl`.
   - Set kernel arg `mode`: `0` for `out_of_bounds`, `1` for `race_condition`.
   - Enqueue kernel with `global_work_size = 64`, `local_work_size = 64` (single work-group; required for the race condition to be within one group).
   - After `queue.finish()`, print a single line to stdout:
     - `out_of_bounds`: `"Bug injected: out-of-bounds write at buf[64] (1 element past end). Run under oclgrind to detect."`
     - `race_condition`: `"Bug injected: race condition — all 64 work-items write to buf[0]. Run under oclgrind --check-api to detect."`
   - The program exits with code 0 in all cases (the bugs are memory-level; the host does not crash on real hardware — only Oclgrind catches them).

---

## Definition of Done (DoD)

- [ ] `cmake -B build && cmake --build build` succeeds without errors from `99_Toolbox/Debugging/`. A warning about Oclgrind being absent is acceptable; an error is not.
- [ ] `./build/debug_demo --test out_of_bounds` runs to completion (exit 0) on real hardware and prints the expected injection confirmation line.
- [ ] `./build/debug_demo --test race_condition` runs to completion (exit 0) on real hardware and prints the expected injection confirmation line.
- [ ] `./build/debug_demo --help` prints CLI11-generated usage listing `--test`.
- [ ] `./build/debug_demo` (no args) exits non-zero and prints a CLI11 error indicating `--test` is required.
- [ ] `oclgrind ./build/debug_demo --test out_of_bounds` reports at least one `Invalid write` error referencing `debug_kernel.cl`.
- [ ] `oclgrind --check-api ./build/debug_demo --test race_condition` reports at least one data-race or concurrent-write error.
- [ ] `GPU` env var is respected: `GPU=NVIDIA ./build/debug_demo --test out_of_bounds` selects the Nvidia device if present.
- [ ] The binary is standalone — no dependency on other Toolbox tool directories.

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
| `99_Toolbox/Debugging/CMakeLists.txt` | Created |
| `99_Toolbox/Debugging/main.cpp` | Created |
| `99_Toolbox/Debugging/kernels/debug_kernel.cl` | Created |

### Remaining
- [ ] [Remaining item]
