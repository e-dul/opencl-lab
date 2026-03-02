# Task 008: Toolbox — LocalMemory Tool

## Context
- **Design Feature:** `workflow/design/07-toolbox.md`
- **Milestone:** Phase 3 — LocalMemory (`99_Toolbox/LocalMemory/`)
- **Relevant Files:**
  - `workflow/design/07-toolbox.md` — (read-only: architecture reference)
  - `.claude/rules/00_master_specs.md` — (read-only: inherited constraints)
  - `common/ocl_wrapper.hpp` — (read-only: `create_context()`, GPU env var)
  - `common/opencl_utils.hpp` — (read-only: `CL_CHECK` macro)
  - `common/common.cmake` — (read-only: CLI11 integration)
  - `99_Toolbox/LocalMemory/LocalMemory.md` — (read-only: user-facing description)
  - `99_Toolbox/LocalMemory/CMakeLists.txt` — (new file)
  - `99_Toolbox/LocalMemory/main.cpp` — (new file)
  - `99_Toolbox/LocalMemory/kernels/blur_kernel.cl` — (new file)

## Objective
Implement the LocalMemory tool: a standalone C++17 executable that benchmarks a box blur kernel in two variants — pure global memory reads vs. tile+halo pattern using `__local` memory — prints a structured before/after timing table, and writes `output.bmp` for the optimized path.

## Constraints & Rules
- **No Design Changes:** Do not modify `workflow/design/*.md`. If architecture is wrong, stop and ask.
- **Language/Standard:** C++17. No C++20.
- **OpenCL:** `cl.hpp` bindings only. `clCreateBuffer` / `clReleaseMemObject` are FORBIDDEN.
- **CLI Parsing:** CLI11 via `common/common.cmake`. Minimum flags: `--width` (default 1920), `--height` (default 1080), `--radius` (default 5).
- **GPU Selection:** `create_context()` from `common/ocl_wrapper.hpp`. Hard-coded device indices are FORBIDDEN.
- **Profiling:** All timing via `cl::Event` (`CL_QUEUE_PROFILING_ENABLE`). Wall-clock measurements do not satisfy the performance gate.
- **Output image:** `output.bmp` written via `stb_image_write`. Format: Grayscale (`uchar`, 1 channel). Both variants must produce byte-identical pixel output — verified in-process.
- **Error Handling:** `CL_CHECK()` for all OpenCL calls. Throw `std::runtime_error` on failure.
- **Standalone Build:** Must build via `cmake -B build && cmake --build build` from `99_Toolbox/LocalMemory/`. No shared CMake parent.
- **Kernel copy rule (CMakeLists.txt):**
  ```cmake
  add_custom_command(TARGET local_mem_demo POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/kernels
            $<TARGET_FILE_DIR:local_mem_demo>/kernels
    COMMENT "Copying kernels"
  )
  ```
- **Local memory limit guard:** Query `CL_DEVICE_LOCAL_MEM_SIZE` at startup. If the required tile (`(local_size + 2*radius)^2` bytes) exceeds 80% of the device limit, print a warning and reduce `--radius` automatically (print the adjusted value). Do not crash.

---

## Implementation

1. **Create `99_Toolbox/LocalMemory/CMakeLists.txt`**
   - `cmake_minimum_required(VERSION 3.18)`, `project(LocalMemory CXX)`, `set(CMAKE_CXX_STANDARD 17)`.
   - `find_package(OpenCL REQUIRED)`.
   - Include `common/common.cmake` (path: `${CMAKE_CURRENT_SOURCE_DIR}/../../common/common.cmake`).
   - Link `OpenCL::OpenCL`, `CLI11::CLI11`.
   - Add POST_BUILD kernel copy command as specified above.

2. **Create `99_Toolbox/LocalMemory/kernels/blur_kernel.cl`**
   - Two kernels, both accepting `(__global const uchar* input, __global uchar* output, int width, int height, int radius)`:
     - `blur_global`: work-item `(gx, gy)` iterates over the `(2*radius+1)^2` neighbourhood reading directly from `input[]` each time. Boundary-clamps with `clamp(gx+dx, 0, width-1)`.
     - `blur_local`: tile+halo pattern.
       - Work-group size is a compile-time constant `LOCAL_SIZE` (set by `-D LOCAL_SIZE=16` from host).
       - Declares `__local uchar tile[(LOCAL_SIZE + 2*MAX_RADIUS) * (LOCAL_SIZE + 2*MAX_RADIUS)]` where `MAX_RADIUS` is also passed via `-D`.
       - Phase 1: cooperatively load the tile including halo from global memory (boundary-clamped). Each work-item loads its own pixel plus contributes to halo fill using modulo-based distribution.
       - `barrier(CLK_LOCAL_MEM_FENCE)` after tile load.
       - Phase 2: compute box blur sum by reading only from `tile[]`.
       - Boundary guard: `if (gx >= width || gy >= height) return;` at entry.

3. **Create `99_Toolbox/LocalMemory/main.cpp`**
   - Parse CLI args with CLI11: `--width` (default 1920), `--height` (default 1080), `--radius` (default 5).
   - Call `create_context()`, create `cl::CommandQueue` with `CL_QUEUE_PROFILING_ENABLE`.
   - Query `CL_DEVICE_LOCAL_MEM_SIZE`; enforce the tile-size guard (see Constraints above).
   - Generate a synthetic grayscale input: `input[y * width + x] = static_cast<uchar>((x * 3 + y * 7) % 256)`.
   - Allocate `cl::Buffer` for input (`CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR`) and two separate output buffers (`CL_MEM_WRITE_ONLY`).
   - Build the kernel program with `-D LOCAL_SIZE=16 -D MAX_RADIUS=<radius>`.
   - Run `blur_global` with 2D NDRange (`global` padded to next multiple of 16). Capture `cl::Event`, record kernel time.
   - Run `blur_local` with 2D NDRange and `local = {16, 16}`. Capture `cl::Event`, record kernel time.
   - Print structured timing table:
     ```
     Variant                Kernel Time (ms)   Speedup
     ──────────────────────────────────────────────────
     Global memory blur     XX.XXX             1.00x
     Local memory blur      XX.XXX             X.XXx
     ```
   - Pixel correctness check: read back both output buffers. Any mismatch → print `[ERROR] output mismatch at pixel (x, y)` and `exit(1)`.
   - Write `output.bmp` from the `blur_local` output buffer via `stb_image_write` (grayscale, 1 channel).

4. **Handle `local_work_size` / NDRange padding**
   - For both kernels, if `width` or `height` is not divisible by 16, round up `global_work_size` components to the next multiple of 16. The kernel's boundary guard handles the extra work-items.

---

## Definition of Done (DoD)

- [ ] `cmake -B build && cmake --build build` succeeds from `99_Toolbox/LocalMemory/` with zero errors and zero warnings.
- [ ] Binary runs: `./build/local_mem_demo --radius 5 --width 1920 --height 1080` completes without error.
- [ ] Console output contains the two-row timing table with `Global memory blur` and `Local memory blur` rows, both with non-zero kernel times (not `0.000 ms`).
- [ ] `Local memory blur` kernel time is at least 2× faster than `Global memory blur` at radius 5 on a discrete GPU (performance gate: design doc specifies ≥3×; acceptable to note "integrated GPU" if below gate on iGPU).
- [ ] Pixel correctness check passes silently (no `[ERROR]` lines in normal output on a valid run).
- [ ] `output.bmp` is produced in the working directory and is a valid, non-corrupt grayscale BMP (visually shows a blurred gradient).
- [ ] `--help` prints CLI11-generated usage including `--width`, `--height`, and `--radius` flags.
- [ ] `GPU=NVIDIA ./build/local_mem_demo` (or equivalent vendor string) selects the correct device without crashing.
- [ ] `blur_local` kernel contains `barrier(CLK_LOCAL_MEM_FENCE)` between tile-load and tile-read phases.
- [ ] Running with `--radius 20` either completes successfully or prints the local-memory-limit warning and adjusts radius — it does NOT crash or produce a CL error.

---

## Execution Report
<!-- Filled by @coder after implementation. -->

- **Status:** PENDING
- **Session:** [YYYY-MM-DD]

### Validation
```
[output here]
```

### Changed Files
| File | Change |
|------|--------|
| `99_Toolbox/LocalMemory/CMakeLists.txt` | Created |
| `99_Toolbox/LocalMemory/main.cpp` | Created |
| `99_Toolbox/LocalMemory/kernels/blur_kernel.cl` | Created |

### Remaining
- [ ] All DoD items above
