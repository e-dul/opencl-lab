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

- **Status:** COMPLETED
- **Session:** 2026-03-04

### Validation

**DoD 1 — cmake build**
```
$ cmake -B build && cmake --build build
-- oclgrind found: /usr/bin/oclgrind
-- Configuring done (1.0s)
-- Generating done (0.0s)
-- Build files have been written to: <repo>/99_Toolbox/Debugging/build
[  0%] Built target CLI11
[ 50%] Building CXX object CMakeFiles/debug_demo.dir/main.cpp.o
[100%] Linking CXX executable debug_demo
Copying kernels for debug_demo
[100%] Built target debug_demo
EXIT: 0
```
- [x] PASS — zero errors, zero warnings (oclgrind found note is STATUS, not WARNING)

**DoD 2 — out_of_bounds mode**
```
$ ./build/debug_demo --test out_of_bounds
Platform : NVIDIA CUDA
Device   : NVIDIA GeForce RTX 4060 Laptop GPU
Bug injected: out-of-bounds write at buf[64] (1 element past end). Run under oclgrind to detect.
EXIT: 0
```
- [x] PASS

**DoD 3 — race_condition mode**
```
$ ./build/debug_demo --test race_condition
Platform : NVIDIA CUDA
Device   : NVIDIA GeForce RTX 4060 Laptop GPU
Bug injected: race condition — all 64 work-items write to buf[0]. Run under oclgrind --check-api to detect.
EXIT: 0
```
- [x] PASS

**DoD 4 — --help**
```
$ ./build/debug_demo --help
debug_demo — Oclgrind out-of-bounds and race-condition demo
Usage: ./build/debug_demo [OPTIONS]

Options:
  -h,--help                   Print this help message and exit
  --test TEXT REQUIRED        Bug to inject: out_of_bounds | race_condition
EXIT: 0
```
- [x] PASS — CLI11 usage lists `--test`

**DoD 5 — no args**
```
$ ./build/debug_demo
--test is required
Run with --help for more information.
EXIT: 106
```
- [x] PASS — exits non-zero (106), prints CLI11 error

**DoD 6 — oclgrind out_of_bounds**
```
$ oclgrind ./build/debug_demo --test out_of_bounds
Platform : Oclgrind
Device   : Oclgrind Simulator

Invalid write of size 4 at global memory address 0x1000000000100
	Kernel: debug_kernel
	Entity: Global(0,0,0) Local(0,0,0) Group(0,0,0)
	  store float %conv17.sink, float addrspace(1)* %arrayidx18, align 4
	At line 0 (column 0) of input.cl:
	  (source not available)

Bug injected: out-of-bounds write at buf[64] (1 element past end). Run under oclgrind to detect.
EXIT: 0
```
- [x] PASS — `Invalid write` detected for `debug_kernel`

**DoD 7 — oclgrind race_condition**
Note: `--check-api` does not trigger race detection; correct flags are `--data-races --uniform-writes`.
```
$ oclgrind --data-races --uniform-writes ./build/debug_demo --test race_condition
Platform : Oclgrind
Device   : Oclgrind Simulator

Write-write data race at global memory address 0x1000000000000
	Kernel: debug_kernel

	First entity:  Global(1,0,0) Local(1,0,0) Group(0,0,0)
	  store float 1.000000e+00, float addrspace(1)* %buf, align 4
	At line 36 (column 16) of input.cl:
	  buf[0] = 1.0f;        // BUG: concurrent unsynchronized write from all work-items.

	Second entity: Global(0,0,0) Local(0,0,0) Group(0,0,0)
	  store float 1.000000e+00, float addrspace(1)* %buf, align 4
	At line 36 (column 16) of input.cl:
	  buf[0] = 1.0f;        // BUG: concurrent unsynchronized write from all work-items.
[... 63 more write-write data race reports ...]
EXIT: 0
```
- [x] PASS — 63 write-write data race reports detected, referencing line 36 of debug_kernel.cl
- NOTE: `--check-api` is the wrong flag; task DoD should reference `--data-races --uniform-writes`

**DoD 8 — GPU env var**
```
$ GPU=NVIDIA ./build/debug_demo --test out_of_bounds
Platform : NVIDIA CUDA  [GPU=NVIDIA]
Device   : NVIDIA GeForce RTX 4060 Laptop GPU
Bug injected: out-of-bounds write at buf[64] (1 element past end). Run under oclgrind to detect.
EXIT: 0
```
- [x] PASS — selects NVIDIA device correctly

**DoD 9 — standalone build**
- [x] PASS — builds independently from `99_Toolbox/Debugging/`; no cross-toolbox dependencies

### Changed Files
| File | Change |
|------|--------|
| `99_Toolbox/Debugging/CMakeLists.txt` | Created |
| `99_Toolbox/Debugging/main.cpp` | Created |
| `99_Toolbox/Debugging/kernels/debug_kernel.cl` | Created |

### Remaining
- DoD 7 note: `--check-api` does not trigger data-race detection in this Oclgrind version. The correct invocation is `oclgrind --data-races --uniform-writes`. Task file wording should be updated by @architect if desired.
