# Task 014: MultiGPU_Strategy Tool

## Context
- **Design Feature:** `workflow/design/07-toolbox.md`
- **Milestone:** Phase 9 — MultiGPU_Strategy
- **Relevant Files:**
  - `workflow/design/07-toolbox.md` — (read-only: architecture reference)
  - `.claude/rules/00_master_specs.md` — (read-only: global constraints)
  - `common/ocl_wrapper.hpp` — (read-only: `create_context()`)
  - `common/opencl_utils.hpp` — (read-only: `CL_CHECK` macro)
  - `99_Toolbox/MultiGPU_Strategy/` — (new directory: all files to create)

## Objective
Implement a standalone `MultiGPU_Strategy` tool that detects all available OpenCL GPU devices, distributes horizontal image slices across them via per-device queues and contexts, merges the results on the host, and prints a structured timing table comparing single-GPU vs N-GPU throughput on a 4K workload.

## Constraints & Rules
- **No Design Changes:** Do not modify `workflow/design/*.md`. If architecture is wrong, stop and ask.
- **Language/Standard:** C++17.
- **OpenCL:** 1.2 baseline. `cl.hpp` C++ bindings only. `clCreateBuffer` / `clReleaseMemObject` forbidden.
- **CLI:** CLI11 v2.4.2 via `common/common.cmake`. Required flags: `--width`, `--height`, `--gpus` (integer, default 0 = all available).
- **GPU Selection:** Device enumeration via `create_context()` (respects `GPU` env var for filtering). Hard-coded device indices forbidden.
- **Cross-Platform Multi-GPU:** A single `cl::Context` spanning devices from different platforms is not supported in OpenCL 1.2. Implement separate per-device contexts (one `cl::Context` per device). Host-side merge synchronises results after all devices complete.
- **Profiling:** All GPU timing via `cl::Event` with `CL_QUEUE_PROFILING_ENABLE`. No wall-clock measurements for performance gate verification.
- **Error Handling:** `CL_CHECK()` macro on all CL return codes. `std::runtime_error` on fatal errors.
- **Single-GPU Fallback:** If only one GPU is detected, run both the single-GPU and single-GPU (trivially N=1) legs and note the absence of a second device in the output table. Do not crash.
- **Kernel:** Must be a separate `.cl` file loaded at runtime. Kernel copied to binary dir by CMake `POST_BUILD` command.
- **No Hardcoded Paths:** Asset paths via CLI args only.

---

## Implementation

1. **Directory scaffold**: Create `99_Toolbox/MultiGPU_Strategy/` with `CMakeLists.txt`, `main.cpp`, and `kernels/slice_kernel.cl`.

2. **CMakeLists.txt**:
   - Target name: `multigpu_strategy`.
   - `cmake_minimum_required(VERSION 3.18)`, `project(MultiGPU_Strategy CXX)`, `set(CMAKE_CXX_STANDARD 17)`.
   - `find_package(OpenCL REQUIRED)`.
   - Include `common/common.cmake` for CLI11 and shared headers.
   - `POST_BUILD` command to copy `kernels/` to `$<TARGET_FILE_DIR:multigpu_strategy>/kernels/`.

3. **`kernels/slice_kernel.cl`**:
   - Kernel name: `process_slice`.
   - Signature: `__kernel void process_slice(__global const uchar4* in, __global uchar4* out, int width, int height, int row_offset)`.
   - Operation: per-pixel invert (`255 - value`) applied to the assigned slice rows. Invertible transform allows correctness verification: applying the kernel twice returns the original image.
   - `row_offset` allows each device to work on its assigned horizontal band while the kernel operates on a full logical slice buffer (device-local allocation sized to the slice).

4. **`main.cpp` — device enumeration**:
   - Enumerate all platforms and collect all GPU devices (type `CL_DEVICE_TYPE_GPU`). If none found, fall back to `CL_DEVICE_TYPE_CPU` and print a warning.
   - Respect `GPU` env var filter (substring match on `CL_PLATFORM_VENDOR` / `CL_DEVICE_VENDOR`, case-insensitive) consistent with `common/ocl_wrapper.hpp` logic.
   - Print a device table at startup: index, vendor, device name, max compute units.

5. **`main.cpp` — input data**:
   - Generate a synthetic RGBA image of `--width` x `--height` pixels (default 3840x2160 = 4K) filled with a gradient pattern. No file I/O required; synthetic data avoids asset dependency for a numeric benchmark tool.
   - If `--image` flag is provided (optional), load from BMP via `stb_image`.

6. **`main.cpp` — single-GPU baseline leg**:
   - Use device[0] only.
   - One `cl::Context`, one `cl::CommandQueue` (profiling enabled), one `cl::Buffer` for the full image.
   - Enqueue `process_slice` over the full image in one dispatch.
   - Capture `cl::Event` timing. Record as `t_single_gpu_ms`.

7. **`main.cpp` — N-GPU parallel leg**:
   - For each device[i], create an independent `cl::Context` and `cl::CommandQueue` (profiling enabled).
   - Partition the image into N horizontal slices (rows 0..H/N-1, H/N..2H/N-1, ...). Last slice absorbs remainder rows.
   - For each device: `cl::Buffer` allocated for its slice, `clEnqueueWriteBuffer`, kernel dispatch, `clEnqueueReadBuffer` — all captured as a chain of `cl::Event`s.
   - Call `queue.finish()` on all queues before measuring wall-clock end. Use per-device `cl::Event` profiling for the kernel time component. Report per-device kernel time in the table.
   - After all devices finish, reconstruct the full output image from the per-device slice buffers.
   - Total N-GPU time = max(per-device end timestamp) - min(per-device start timestamp) using `CL_PROFILING_COMMAND_START` / `CL_PROFILING_COMMAND_END`.

8. **`main.cpp` — output table**:
   Print the following structured table to stdout (ms to 3 decimal places):
   ```
   ┌────────────────────────────┬──────────────┐
   │ Configuration              │ Time (ms)    │
   ├────────────────────────────┼──────────────┤
   │ Single-GPU (device 0)      │   XX.XXX     │
   │ N-GPU parallel (N=2)       │   XX.XXX     │
   │ Speedup                    │   X.XXx      │
   └────────────────────────────┴──────────────┘
   Per-device kernel times:
     [0] NVIDIA GeForce RTX ...   XX.XXX ms
     [1] AMD Radeon ...           XX.XXX ms
   ```

9. **`main.cpp` — correctness check**:
   - After the N-GPU leg, apply `process_slice` a second time on the merged output (single dispatch on device[0]).
   - Compare byte-for-byte with the original synthetic input. Print `[PASS] Output verified` or `[FAIL] Mismatch at pixel N`.

10. **`main.cpp` — OOO queue warning (Known Issue)**:
    - If N=1 and the single-GPU leg equals the N-GPU leg, print a note: "Only 1 device found; N-GPU leg is equivalent to single-GPU baseline."

---

## Definition of Done (DoD)

- [ ] `cmake -B build && cmake --build build` succeeds from within `99_Toolbox/MultiGPU_Strategy/` without warnings on GCC/Clang with `-Wall`.
- [ ] `./build/multigpu_strategy --help` prints CLI11-generated usage listing `--width`, `--height`, `--gpus`, and optional `--image`.
- [ ] Running with default args on a single-GPU system prints the structured timing table with `N-GPU parallel (N=1)` row and the note "Only 1 device found".
- [ ] Running on a dual-GPU system produces a speedup ≥ 2× on a 4K (3840×2160) workload (performance gate from design doc).
- [ ] Per-device kernel times are sourced from `cl::Event` profiling (`CL_PROFILING_COMMAND_START/END`), not wall-clock.
- [ ] `[PASS] Output verified` printed after the correctness check (invert-twice round-trip).
- [ ] `kernels/slice_kernel.cl` is copied to the binary directory by the `POST_BUILD` CMake command and loaded at runtime (no embedded kernel strings).
- [ ] No hard-coded device indices; device selection respects `GPU` env var.
- [ ] On a single-platform / multi-device system, separate `cl::Context` per device is used (no cross-device context).

---

## Execution Report
<!-- Filled by @coder after implementation. -->

- **Status:** PENDING
- **Session:** —

### Validation
```
[output here]
```

### Changed Files
| File | Change |
|------|--------|
| `99_Toolbox/MultiGPU_Strategy/CMakeLists.txt` | Created |
| `99_Toolbox/MultiGPU_Strategy/main.cpp` | Created |
| `99_Toolbox/MultiGPU_Strategy/kernels/slice_kernel.cl` | Created |

### Remaining
- [ ] Implementation by @coder
