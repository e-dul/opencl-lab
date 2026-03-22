# Task 019: A1 — OpenCV Interop Benchmark

## Context
- **Design Feature:** `workflow/design/04-multimedia-projects.md`
- **Milestone:** Phase 1 — A1 OpenCV Interop
- **Relevant Files:**
  - `workflow/design/04-multimedia-projects.md` — (read-only: architecture, data flow §A1)
  - `.claude/rules/00_master_specs.md` — (read-only: global constraints)
  - `common/ocl_wrapper.hpp` — (read-only: `create_context()`)
  - `common/common.cmake` — (read-only: CMake shared config, CLI11)
  - `02_Projects/A_Multimedia/A1_OpenCV_Interop/CMakeLists.txt` — (new file)
  - `02_Projects/A_Multimedia/A1_OpenCV_Interop/main.cpp` — (new file)

## Objective

Implement a standalone transfer benchmark that measures and compares the GPU upload cost of two paths — `clEnqueueWriteBuffer` from a raw `cv::Mat` vs zero-copy via `cv::UMat` + `CL_MEM_USE_HOST_PTR` — and prints a structured timing table to console.

## Constraints & Rules

- All standard constraints from `00_master_specs.md` apply (C++17, `cl.hpp`, `CLI11`, `create_context()`, `cl::Event` profiling, `CL_CHECK`, standalone CMake, kernel copy rule).
- **No kernel file needed.** This is a pure host-side memory transfer benchmark. No `.cl` kernel is required.
- **No BMP output.** Console timing table is the sole required artifact (master_specs §3, Exception clause).
- **`cl::Event` profiling only.** Wall-clock measurements do NOT satisfy the performance gate (master_specs §6).
- **Input image must come from `--input` CLI arg.** Default may point to `assets/sample.bmp`. Hardcoded paths are forbidden.
- `create_context()` from `common/ocl_wrapper.hpp` must be used for device selection. Hard-coded platform/device indices are forbidden.
- **OpenCV**: `find_package(OpenCV REQUIRED)` in CMakeLists.txt. OpenCV 4.5+ assumed.
- The `UMat` zero-copy path must use `CL_MEM_USE_HOST_PTR` when constructing the `cl::Buffer` from the UMat data pointer. Add a `// WHY` comment explaining why this flag enables zero-copy on UMA/iGPU stacks.
- **Integer safety**: if `image.total() * image.elemSize() > INT_MAX`, throw `std::runtime_error`. Silent truncation is forbidden.

---

## Implementation

1. **Scaffold directory and CMakeLists.txt**
   - Create `02_Projects/A_Multimedia/A1_OpenCV_Interop/CMakeLists.txt`.
   - Standalone build: `cmake_minimum_required(VERSION 3.18)`, `project(A1_OpenCV_Interop CXX)`.
   - `set(CMAKE_CXX_STANDARD 17)` + `set(CMAKE_CXX_EXTENSIONS OFF)`.
   - `find_package(OpenCL REQUIRED)` and `find_package(OpenCV REQUIRED)`.
   - Include `common/common.cmake` (via `CMAKE_CURRENT_SOURCE_DIR/../../..`).
   - Link: `OpenCL::OpenCL`, `CLI11::CLI11`, `${OpenCV_LIBS}`.
   - `add_definitions(-DCL_HPP_ENABLE_EXCEPTIONS -DCL_HPP_TARGET_OPENCL_VERSION=120)`.
   - No kernel copy command needed (no kernel files).

2. **Implement `main.cpp`**

   **CLI args** (CLI11):
   - `--input` (`std::string`, required): path to input image.
   - `--iterations` (`int`, default `10`): number of benchmark repetitions per path (warm-up + average).

   **Load image**:
   - `cv::imread(input_path, cv::IMREAD_COLOR)` → `cv::Mat mat_bgr`.
   - Validate non-empty. Convert to RGBA: `cv::cvtColor(mat_bgr, mat_rgba, cv::COLOR_BGR2RGBA)`.
   - Compute `size_bytes = mat_rgba.total() * mat_rgba.elemSize()`. Throw if `> INT_MAX`.

   **OpenCL setup**:
   - `auto [ctx, queue] = create_context()` — enable profiling: `cl::CommandQueue(ctx, device, CL_QUEUE_PROFILING_ENABLE)`.
   - Create a destination `cl::Buffer` (device-side, `CL_MEM_READ_WRITE`, `size_bytes`).

   **Path 1 — Copy (`clEnqueueWriteBuffer`)**:
   - Loop `iterations` times. Each iteration:
     - `cl::Event ev;`
     - `CL_CHECK(queue.enqueueWriteBuffer(buf, CL_FALSE, 0, size_bytes, mat_rgba.data, nullptr, &ev));`
     - `CL_CHECK(queue.finish());`
     - Accumulate `ev.getProfilingInfo<CL_PROFILING_COMMAND_END>() - ev.getProfilingInfo<CL_PROFILING_COMMAND_START>()` (nanoseconds).
   - Compute average ms.

   **Path 2 — Zero-Copy (`CL_MEM_USE_HOST_PTR` via UMat)**:
   - Convert `mat_rgba` → `cv::UMat umat_rgba` via `mat_rgba.getUMat(cv::ACCESS_READ)`.
   - Obtain raw pointer: `void* ptr = umat_rgba.getMat(cv::ACCESS_READ).data;` (validate non-null).
   - Loop `iterations` times. Each iteration:
     - Construct `cl::Buffer host_buf(ctx, CL_MEM_USE_HOST_PTR | CL_MEM_READ_ONLY, size_bytes, ptr);`
       ```cpp
       // WHY: CL_MEM_USE_HOST_PTR tells the runtime to use `ptr` directly as the
       // backing store. On UMA (iGPU / CPU OCL), this is a true zero-copy path —
       // no PCIe transfer occurs. On discrete GPU, the driver pins and DMA-maps the
       // page-aligned host buffer, avoiding a redundant staging copy.
       ```
     - Enqueue a read-back of 1 byte to force the buffer to be materialized and record event time:
       ```cpp
       cl::Event ev;
       uint8_t dummy;
       CL_CHECK(queue.enqueueReadBuffer(host_buf, CL_FALSE, 0, 1, &dummy, nullptr, &ev));
       CL_CHECK(queue.finish());
       ```
     - Accumulate profiling time.
   - Compute average ms.

   **Print timing table**:
   ```
   === OpenCV Interop Transfer Benchmark ===
   Image: 1920x1080 RGBA (7.91 MB)   [or actual dims/size]
   Iterations: 10

   Path                         Avg Time (ms)
   ----------------------------------------
   1. clEnqueueWriteBuffer       X.XXX ms
   2. UMat zero-copy             X.XXX ms
   ----------------------------------------
   Speedup (zero-copy / copy):   X.XXX x
   Gate: PASS   [if zero-copy >= 20% faster]   OR   Gate: WAIVER (iGPU)
   ```
   - Gate check: `speedup = copy_ms / zerocopy_ms`. PASS if `speedup >= 1.20`. If not, print `WAIVER` with a note about iGPU/UMA topology.

3. **Validate build**
   - `cmake -B build && cmake --build build` from `A1_OpenCV_Interop/` — zero errors, zero warnings.
   - Run `./build/A1_OpenCV_Interop --input ../../../assets/sample.bmp` — prints timing table.
   - `./build/A1_OpenCV_Interop --help` — CLI11 usage shown.

---

## Definition of Done (DoD)

Standard items (master_specs §8):
- [x] `cmake -B build && cmake --build build` succeeds with zero errors and zero warnings.
- [x] Binary runs without error: `./build/A1_OpenCV_Interop --input ../../../assets/sample.bmp`.
- [x] `--help` prints CLI11-generated usage including `--input` and `--iterations`.
- [x] `GPU=<vendor> ./build/A1_OpenCV_Interop --input ...` selects the correct device without crashing.

Task-specific outcomes:
- [x] Console output contains a structured timing table with both path timings in ms (3 decimal places).
- [x] Both timings are sourced from `cl::Event` profiling, not `std::chrono`.
- [x] `CL_MEM_USE_HOST_PTR` buffer construction includes a `// WHY` comment explaining zero-copy semantics.
- [x] `size_bytes > INT_MAX` check throws `std::runtime_error` (not silent truncation).
- [x] Speedup gate printed: PASS (`>= 1.20x`) or WAIVER with topology note.
- [x] No hardcoded asset paths; `--input` is required CLI arg.

---

## Execution Report

- **Status:** COMPLETE
- **Session:** 2026-03-07 — Device: NVIDIA GeForce RTX 4060 Laptop GPU

### Validation
```
# 1. cmake -B build && cmake --build build
-- Configuring done (1.2s)
-- Generating done (0.0s)
-- Build files have been written to: .../A1_OpenCV_Interop/build
[  0%] Built target CLI11
[100%] Built target A1_OpenCV_Interop
# Zero errors, zero warnings.

# 2. ./build/A1_OpenCV_Interop --input /home/emil/Projects/opencl-lab/01_Host_API/01_Visual_Kernel/gradient_input.bmp
pci id for fd 10: 10de:28e0, driver (null)
pci id for fd 11: 10de:28e0, driver (null)
Platform : NVIDIA CUDA
Device   : NVIDIA GeForce RTX 4060 Laptop GPU

=== OpenCV Interop Transfer Benchmark ===
Image: 256x256 RGBA (0.25 MB)
Iterations: 10

Path                            Avg Time (ms)
------------------------------------------------
1. clEnqueueWriteBuffer         0.023 ms
2. UMat zero-copy               0.001 ms
------------------------------------------------
Speedup (zero-copy / copy):   21.695 x
Gate: PASS

# 3. ./build/A1_OpenCV_Interop --help
OpenCV Interop Transfer Benchmark
Usage: ./build/A1_OpenCV_Interop [OPTIONS]

Options:
  -h,--help                   Print this help message and exit
  --input TEXT REQUIRED       Path to input image
  --iterations INT [10]       Benchmark repetitions per path

# 4. GPU=NVIDIA ./build/A1_OpenCV_Interop --input ...
pci id for fd 10: 10de:28e0, driver (null)
pci id for fd 11: 10de:28e0, driver (null)
Platform : NVIDIA CUDA  [GPU=NVIDIA]
Device   : NVIDIA GeForce RTX 4060 Laptop GPU

=== OpenCV Interop Transfer Benchmark ===
Image: 256x256 RGBA (0.25 MB)
Iterations: 10

Path                            Avg Time (ms)
------------------------------------------------
1. clEnqueueWriteBuffer         0.024 ms
2. UMat zero-copy               0.001 ms
------------------------------------------------
Speedup (zero-copy / copy):   21.115 x
Gate: PASS
```

### Notes
- `assets/sample.bmp` does not exist at the repo root; validation used an equivalent BMP from `01_Host_API/01_Visual_Kernel/gradient_input.bmp` (256x256). The `--input` path is a CLI arg; no hardcoded path exists in source.
- Speedup of ~21x (well above 1.20x gate) on NVIDIA RTX 4060 Laptop GPU — Gate: PASS.

### Changed Files
| File | Change |
|------|--------|
| `02_Projects/A_Multimedia/A1_OpenCV_Interop/CMakeLists.txt` | Created |
| `02_Projects/A_Multimedia/A1_OpenCV_Interop/main.cpp` | Created |
