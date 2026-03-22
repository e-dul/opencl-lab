# Task 006: Toolbox — ZeroCopy Tool

## Context
- **Design Feature:** `workflow/design/07-toolbox.md`
- **Milestone:** Phase 1 — ZeroCopy (`99_Toolbox/ZeroCopy/`)
- **Relevant Files:**
  - `workflow/design/07-toolbox.md` — (read-only: architecture reference)
  - `.claude/rules/00_master_specs.md` — (read-only: inherited constraints)
  - `common/ocl_wrapper.hpp` — (read-only: `create_context()`, GPU env var)
  - `common/opencl_utils.hpp` — (read-only: `CL_CHECK` macro)
  - `common/common.cmake` — (read-only: CLI11 integration)
  - `99_Toolbox/ZeroCopy/ZeroCopy.md` — (read-only: user-facing description)
  - `99_Toolbox/ZeroCopy/CMakeLists.txt` — (new file)
  - `99_Toolbox/ZeroCopy/main.cpp` — (new file)
  - `99_Toolbox/ZeroCopy/kernels/zero_copy_kernel.cl` — (new file)

## Objective
Implement the ZeroCopy tool: a standalone C++17 executable that benchmarks three OpenCL buffer strategies (`CL_MEM_COPY_HOST_PTR`, `CL_MEM_ALLOC_HOST_PTR`, `CL_MEM_USE_HOST_PTR`) on the same image upload workload, prints a structured before/after timing table, and writes `output.bmp` to confirm correctness.

## Constraints & Rules
- **No Design Changes:** Do not modify `workflow/design/*.md`. If architecture is wrong, stop and ask.
- **Language/Standard:** C++17. No C++20.
- **OpenCL:** `cl.hpp` bindings only. `clCreateBuffer` / `clReleaseMemObject` are FORBIDDEN.
- **CLI Parsing:** CLI11 via `common/common.cmake`. Minimum flags: `--width`, `--height`, `--image` (optional input BMP path).
- **GPU Selection:** `create_context()` from `common/ocl_wrapper.hpp`. Hard-coded device indices are FORBIDDEN.
- **Profiling:** All timing via `cl::Event` (`CL_QUEUE_PROFILING_ENABLE`). Wall-clock measurements do not satisfy the performance gate.
- **Output image:** `output.bmp` written via `stb_image_write`. Format: RGBA or Grayscale.
- **Error Handling:** `CL_CHECK()` for all OpenCL calls. Throw `std::runtime_error` on failure.
- **Standalone Build:** Must build via `cmake -B build && cmake --build build` from `99_Toolbox/ZeroCopy/`. No shared CMake parent.
- **Kernel copy rule (CMakeLists.txt):**
  ```cmake
  add_custom_command(TARGET zero_copy POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/kernels
            $<TARGET_FILE_DIR:zero_copy>/kernels
    COMMENT "Copying kernels"
  )
  ```

---

## Implementation

1. **Create `99_Toolbox/ZeroCopy/CMakeLists.txt`**
   - `cmake_minimum_required(VERSION 3.18)`, `project(ZeroCopy CXX)`, `set(CMAKE_CXX_STANDARD 17)`.
   - `find_package(OpenCL REQUIRED)`.
   - Include `common/common.cmake` (path relative to repo root via `${CMAKE_SOURCE_DIR}/../../common/common.cmake` or equivalent discoverable path).
   - Link `OpenCL::OpenCL`, `CLI11::CLI11`.
   - Add POST_BUILD kernel copy command as specified above.

2. **Create `99_Toolbox/ZeroCopy/kernels/zero_copy_kernel.cl`**
   - Single kernel: `passthrough` — reads one input buffer, writes identical data to an output buffer. This confirms that each buffer strategy produces correct pixel data.
   - Signature: `__kernel void passthrough(__global const uchar* in, __global uchar* out, int size)`.

3. **Create `99_Toolbox/ZeroCopy/main.cpp`**
   - Parse CLI args with CLI11: `--width` (default 1920), `--height` (default 1080), `--image` (optional BMP path; if absent, generate a synthetic gradient buffer).
   - Call `create_context()` from `common/ocl_wrapper.hpp`.
   - Show `CL_DEVICE_HOST_UNIFIED_MEMORY` capability status to give hint about expected results.
   - Create `cl::CommandQueue` with `CL_QUEUE_PROFILING_ENABLE`.
   - Allocate host-side pixel data (RGBA, `uchar4` equivalent: `std::vector<cl_uchar>`).
   - Run three benchmark variants in sequence. For each variant:
     a. Create `cl::Buffer` with the respective flag (`CL_MEM_COPY_HOST_PTR`, `CL_MEM_ALLOC_HOST_PTR`, `CL_MEM_USE_HOST_PTR`).
     b. Enqueue the `passthrough` kernel with a `cl::Event`.
     c. Call `queue.finish()`.
     d. Read back output buffer and verify it matches input (byte comparison on first 64 bytes is sufficient).
     e. Record kernel time via `CL_PROFILING_COMMAND_START` / `CL_PROFILING_COMMAND_END`. Report in ms to 3 decimal places.
   - Print structured timing table:
     ```
     Strategy              Kernel Time (ms)
     ─────────────────────────────────────
     COPY_HOST_PTR         X.XXX
     ALLOC_HOST_PTR        X.XXX
     USE_HOST_PTR          X.XXX
     ```
   - Write the output of the ALLOC_HOST_PTR passthrough to `output.bmp` via `stb_image_write`.

4. **Verification step in `main.cpp`**
   - After each variant, assert output matches input. On mismatch, print `"[ERROR] Buffer strategy <name> produced incorrect output"` and `exit(1)`.

---

## Definition of Done (DoD)

- [x] `cmake -B build && cmake --build build` succeeds from `99_Toolbox/ZeroCopy/` with zero errors and zero warnings.
- [x] Binary runs without arguments and completes without error: `./build/zero_copy`.
- [x] `output.bmp` is produced in the working directory and is a valid, non-corrupt BMP file (open-able in any image viewer).
- [x] Console output contains the three-row timing table with `COPY_HOST_PTR`, `ALLOC_HOST_PTR`, and `USE_HOST_PTR` rows.
- [x] All three buffer strategies print a kernel time in ms (not 0.000).
- [x] `--help` prints CLI11-generated usage including `--width`, `--height`, and `--image` flags.
- [x] Pixel verification passes silently (no `[ERROR]` lines in normal output).
- [x] `GPU=NVIDIA ./build/zero_copy` (or equivalent vendor string) correctly selects the specified device without crashing.

---

## Execution Report

- **Status:** COMPLETE
- **Session:** 2026-03-03

### Validation
```
Image size : 1920x1080 (8294400 bytes RGBA)
Platform : NVIDIA CUDA
Device   : NVIDIA GeForce RTX 4060 Laptop GPU
Host Unified Memory: NO

Strategy              Kernel Time (ms)
--------------------------------------
COPY_HOST_PTR         0.078
ALLOC_HOST_PTR        0.076
USE_HOST_PTR          0.071

Output written to output.bmp
```

### Changed Files
| File | Change |
|------|--------|
| `99_Toolbox/ZeroCopy/CMakeLists.txt` | Created |
| `99_Toolbox/ZeroCopy/main.cpp` | Created |
| `99_Toolbox/ZeroCopy/kernels/zero_copy_kernel.cl` | Created |

### Remaining
- All DoD items passed.
