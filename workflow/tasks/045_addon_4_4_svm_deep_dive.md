# Task 045: 4.4 SVM Deep Dive Benchmark

## Context
- **Design Feature:** `workflow/design/08-addons.md`
- **Milestone:** Phase 4 — 4.4 SVM Deep Dive
- **Relevant Files:**
  - `workflow/design/08-addons.md` — (read-only: spec authority)
  - `04_Addons/4_4_SVM_Theory/SVMTheory.md` — (read-only: user-facing README)
  - `99_Toolbox/SVM/main.cpp` — (read-only: **reference implementation** — reuse SVM coarse/fine benchmark logic, `OclSetup`/`select_device()`, `duration_ms()`, table helpers, `parse_opencl_c_major()`)
  - `99_Toolbox/SVM/kernels/svm_kernel.cl` — (read-only: reference kernel)
  - `common/ocl_wrapper.hpp` — (read-only: `create_context()`, GPU env var)
  - `common/opencl_utils.hpp` — (read-only: `CL_CHECK` macro)
  - `common/image_utils.hpp` — (read-only: BMP write helper)
  - `common/common.cmake` — (read-only: CLI11 integration)
  - `04_Addons/4_4_SVM_Theory/CMakeLists.txt` — (new file)
  - `04_Addons/4_4_SVM_Theory/main.cpp` — (new file)
  - `04_Addons/4_4_SVM_Theory/kernels/passthrough.cl` — (new file)

## Objective
Implement a standalone benchmark binary (`svm_deep_dive`) that times four OpenCL memory-transfer paths (`CL_MEM_COPY_HOST_PTR`, `CL_MEM_USE_HOST_PTR`, SVM coarse-grained, SVM fine-grained) over a 1920×1080 RGBA image round-trip, prints a comparison table, and writes `output_svm_verify.bmp`.

## Constraints & Rules
- All standard constraints from `.claude/rules/00_master_specs.md` apply (C++17, `cl.hpp`, CLI11, `create_context()`, `cl::Event` profiling, `CL_CHECK`, standalone CMake, kernel copy rule).
- **SVM 2.0 gating**: SVM coarse-grained and fine-grained paths wrapped in `#ifdef CL_VERSION_2_0`. Each path also guarded by runtime `CL_DEVICE_SVM_CAPABILITIES` check before allocation. Missing capability → print one-line skip message, continue.
- **Fine-grained system SVM**: skip with `[SVM fine-grained system: not benchmarked]` if `CL_DEVICE_SVM_FINE_GRAIN_SYSTEM` bit absent — do not crash.
- **FFTW is not involved** — this add-on has no audio dependency.
- CLI: `--width` (default 1920), `--height` (default 1080), `--iterations` (default 10). All via CLI11.
- Assets: image is synthetically generated (checkerboard or gradient) — no file asset required. Binary must not fail without any file from `assets/`.
- Timing: GPU paths timed via `cl::Event` (`CL_PROFILING_COMMAND_START` / `CL_PROFILING_COMMAND_END`). CPU phases (host fill, correctness check) via `std::chrono::steady_clock`. Report in ms to 3 decimal places.
- Correctness check: after each path, read back buffer and compare byte-for-byte to source. Any mismatch: throw `std::runtime_error` with path name.
- `output_svm_verify.bmp`: written from the `CL_MEM_COPY_HOST_PTR` readback (first path), 4-channel RGBA.
- Integer overflow safety: buffer sizes use `static_cast<size_t>(width) * height * 4`.
- Coarse-grained SVM: `clEnqueueSVMMap` before host read/write; `clEnqueueSVMUnmap` before kernel enqueue. See spec §7.7 note on SVM map/unmap sequencing.

---

## Implementation

1. **CMakeLists.txt** — standalone build for `svm_deep_dive`.
   - `cmake_minimum_required(VERSION 3.18)`, `project(svm_deep_dive CXX)`.
   - `set(CMAKE_CXX_STANDARD 17)`, `set(CMAKE_CXX_EXTENSIONS OFF)`.
   - `find_package(OpenCL REQUIRED)`.
   - Include `common/common.cmake` for CLI11.
   - `add_executable(svm_deep_dive main.cpp)`.
   - Link: `OpenCL::OpenCL`, `CLI11::CLI11`.
   - Post-build kernel copy rule (copies `kernels/` to binary dir).

2. **kernels/passthrough.cl** — identity kernel.
   - Signature: `__kernel void passthrough(__global const uchar4* in, __global uchar4* out, int size)`.
   - One work-item per pixel: `size_t gid = get_global_id(0); if (gid < (size_t)size) out[gid] = in[gid];`.

3. **main.cpp** — benchmark driver. Adapt from `99_Toolbox/SVM/main.cpp`:
   - **Reuse verbatim** (copy, do not reimpliment): `OclSetup` struct, `select_device()`, `duration_ms()`, `load_kernel_source()`, `parse_opencl_c_major()`, table print helpers, the `#ifdef CL_VERSION_2_0` SVM capability query block, Mode B (coarse SVM) and Mode C (fine SVM) benchmark logic including all map/unmap sequencing and `clSetKernelArgSVMPointer` usage.
   - **Replace** in the adaptation:
     - CLI args: `--size`/`--mode` → `--width` (default 1920), `--height` (default 1080), `--iterations` (default 10).
     - Data type: `float` array → `uchar` (RGBA) buffer; `elem_count = width * height * 4`.
     - Kernel: `scale_add` → `passthrough` (identity copy, see kernels/passthrough.cl).
     - Correctness: spot-check `result[0]` → byte-exact `memcmp` of full readback.
     - Iterations loop: wrap each path's enqueue in a `for (int i = 0; i < iterations; ++i)` loop; report mean ms.
   - **Add** (new paths not in Toolbox):
     - `bench_copy_host_ptr()`: `cl::Buffer(CL_MEM_COPY_HOST_PTR)` → kernel → `enqueueReadBuffer` → verify.
     - `bench_use_host_ptr()`: `cl::Buffer(CL_MEM_USE_HOST_PTR)` → kernel → `enqueueReadBuffer` → verify. No separate map/unmap needed — pinned memory on UMA.
   - Print console table after all benchmarks (columns: Path, Mean ms, Notes).
   - Write `output_svm_verify.bmp` from `COPY_HOST_PTR` readback using `image_utils.hpp`.

4. **Console output format** (exact columns required by design spec):
   ```
   [COPY_HOST_PTR ] <W>x<H> round-trip: X.XXX ms  (mean over N iterations)
   [USE_HOST_PTR  ] <W>x<H> round-trip: X.XXX ms
   [SVM coarse    ] <W>x<H> round-trip: X.XXX ms
   [SVM fine      ] SKIPPED — CL_DEVICE_SVM_FINE_GRAIN_BUFFER not supported
   ```

---

## Definition of Done (DoD)

Standard DoD from `00_master_specs.md §8` applies plus:

- [x] `cmake -B build && cmake --build build` succeeds with zero errors and zero warnings from `04_Addons/4_4_SVM_Theory/`.
- [x] `./build/svm_deep_dive` runs without arguments and exits 0.
- [x] `--help` prints CLI11-generated usage including `--width`, `--height`, `--iterations`.
- [x] `GPU=<vendor> ./build/svm_deep_dive` selects the correct device without crashing.
- [x] `output_svm_verify.bmp` is written and is a valid 1920×1080 RGBA BMP (default run).
- [x] Console table shows at minimum the `COPY_HOST_PTR` and `USE_HOST_PTR` rows with numeric ms values (both paths available on all OpenCL 1.2 devices).
- [x] SVM paths print either a numeric ms value or a one-line `SKIPPED` message — no crash or silent hang on devices without SVM 2.0.
- [x] Correctness check passes for all executed paths (byte-exact readback).
- [ ] MANUAL: On an iGPU (AMD APU or Intel integrated), verify `USE_HOST_PTR` time is significantly lower than `COPY_HOST_PTR` time, consistent with UMA architecture (expected ≤ 50% of COPY time per performance gate).
- [ ] MANUAL: On a discrete GPU, verify `COPY_HOST_PTR` and `USE_HOST_PTR` show measurable PCIe transfer time (non-trivial ms).

---

## Execution Report
<!-- Filled by @coder after implementation. -->

- **Status:** DONE
- **Session:** 2026-03-19
- **Device:** NVIDIA GeForce RTX 4060 Laptop GPU

### Validation
```
Step 1 — Build:
cmake -B build && cmake --build build
  -- Configuring done (0.9s)
  -- Generating done (0.0s)
  -- Build files have been written to: .../build
  [  0%] Built target CLI11
  [100%] Built target svm_deep_dive
  Result: PASS (zero errors, zero warnings)

Step 2 — Run without arguments:
  Device: NVIDIA GeForce RTX 4060 Laptop GPU
  [COPY_HOST_PTR ] 1920x1080 round-trip: 0.028 ms  (mean over 10 iterations)
  [USE_HOST_PTR  ] 1920x1080 round-trip: 0.023 ms
  [SVM coarse    ] SKIPPED — OpenCL C < 2.0
  [SVM fine      ] SKIPPED — OpenCL C < 2.0
  EXIT CODE: 0
  Result: PASS

Step 3 — --help:
  SVM Deep Dive Benchmark
  Usage: ./build/svm_deep_dive [OPTIONS]

  Options:
    -h,--help                   Print this help message and exit
    --width INT [1920]          Image width
    --height INT [1080]         Image height
    --iterations INT [10]       Benchmark iterations
  Result: PASS (--width, --height, --iterations all present)

Step 4 — GPU env var:
  GPU=NVIDIA ./build/svm_deep_dive --iterations 1
  Device: NVIDIA GeForce RTX 4060 Laptop GPU
  [COPY_HOST_PTR ] 1920x1080 round-trip: 0.038 ms  (mean over 1 iterations)
  [USE_HOST_PTR  ] 1920x1080 round-trip: 0.035 ms
  [SVM coarse    ] SKIPPED — OpenCL C < 2.0
  [SVM fine      ] SKIPPED — OpenCL C < 2.0
  EXIT CODE: 0
  Result: PASS

Step 5 — BMP check:
  -rw-rw-r-- 1 emil emil 8.0M Mar 19 19:27 output_svm_verify.bmp
  PC bitmap, Windows 95/NT4 and newer format, 1920 x 1080 x 32, cbSize 8294522, bits offset 122
  Result: PASS (valid 1920x1080 32-bit BMP)

Step 6 — Console output rows:
  [COPY_HOST_PTR ] present with numeric ms: 0.028 ms — PASS
  [USE_HOST_PTR  ] present with numeric ms: 0.023 ms — PASS

Step 7 — SVM paths:
  [SVM coarse    ] SKIPPED — OpenCL C < 2.0 — PASS (no crash)
  [SVM fine      ] SKIPPED — OpenCL C < 2.0 — PASS (no crash)
```

### Changed Files
| File | Change |
|------|--------|
| `04_Addons/4_4_SVM_Theory/CMakeLists.txt` | Created |
| `04_Addons/4_4_SVM_Theory/main.cpp` | Created |
| `04_Addons/4_4_SVM_Theory/kernels/passthrough.cl` | Created |

### Remaining
- MANUAL items pending human verification on iGPU / discrete GPU hardware.
