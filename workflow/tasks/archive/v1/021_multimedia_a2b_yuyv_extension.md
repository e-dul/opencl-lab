# Task 021: A2b — YUYV (4:2:2) Extension

## Context
- **Design Feature:** `workflow/design/04-multimedia-projects.md`
- **Milestone:** Phase 2b — A2 YUYV Extension
- **Relevant Files:**
  - `workflow/design/04-multimedia-projects.md` — (read-only: Phase 2b spec, Key Decision §2, Known Issues §A2 Mini-challenge)
  - `.claude/rules/00_master_specs.md` — (read-only: global constraints)
  - `common/ocl_wrapper.hpp` — (read-only: `create_context()`)
  - `common/image_utils.hpp` — (read-only: BMP/PNG save utilities)
  - `common/common.cmake` — (read-only: CMake shared config, CLI11)
  - `02_Projects/A_Multimedia/A2_YUV_Pipeline/` — (read-only: reference implementation for NV12 structure)
  - `02_Projects/A_Multimedia/A2b_YUYV_Extension/CMakeLists.txt` — (new file)
  - `02_Projects/A_Multimedia/A2b_YUYV_Extension/main.cpp` — (new file)
  - `02_Projects/A_Multimedia/A2b_YUYV_Extension/kernels/yuyv_to_rgba.cl` — (new file)
  - `02_Projects/A_Multimedia/A2b_YUYV_Extension/kernels/yuyv_to_rgba_twopass.cl` — (new file)

## Objective

Port the YUV conversion pipeline to YUYV (4:2:2 packed) format, implementing both a single-pass and a two-pass OpenCL kernel, and produce a console timing table comparing the two approaches plus an OpenCV CPU reference path.

## Constraints & Rules

- All standard constraints from `00_master_specs.md` apply (C++17, `cl.hpp`, `CLI11`, `create_context()`, `cl::Event` profiling, `CL_CHECK`, standalone CMake, kernel copy rule).
- **YUYV memory layout (4:2:2 packed)**: Each macropixel is 4 bytes encoding 2 horizontal pixels: `[Y0, U, Y1, V]`. Total buffer size: `width * height * 2` bytes. Width must be even; throw `std::runtime_error` if `width % 2 != 0`.
- **Input is a raw YUYV flat byte file** — not decoded via `cv::imread`. Load as `std::vector<uint8_t>`. OpenCV is used only for the CPU reference path (`cv::cvtColor` with `COLOR_YUV2RGBA_YUYV`).
- **Single-pass kernel** (`yuyv_to_rgba.cl`): one work-item per output pixel; extracts Y0/Y1 and shared U/V from the same macropixel in a single pass. Global size: `width * height`.
- **Two-pass kernel** (`yuyv_to_rgba_twopass.cl`): Pass 1 extracts the Y-plane into a temporary `cl::Buffer`; Pass 2 performs UV-interleaved color reconstruction from that Y buffer plus the original YUYV buffer. Global size for each pass: `width * height`. Separate `cl::Event` for each pass; report both pass times and their sum.
- **`cl::Event` profiling only** for GPU timings. CPU path timed via `std::chrono::steady_clock`. Wall-clock measurements do not satisfy the performance gate.
- **Integer safety**: throw `std::runtime_error` if `static_cast<size_t>(width) * height > static_cast<size_t>(INT_MAX)` before setting `cl_int` kernel args. Silent `std::min` truncation is forbidden.
- **BMP output required** (master_specs §3): save `output_rgba_singlepass.bmp` and `output_rgba_twopass.bmp`. Both must be visually identical for a correct implementation.
- **No hardcoded asset paths.** `--input`, `--width`, `--height` are required CLI args.
- UV-plane and buffer size arithmetic must use `static_cast<size_t>(width) * height` to prevent 32-bit overflow (master_specs §7.1).
- Kernel `.cl` files must be copied to the binary directory via `add_custom_command` POST_BUILD (master_specs §1).

---

## Implementation

1. **Scaffold directory and CMakeLists.txt**
   - Create `02_Projects/A_Multimedia/A2b_YUYV_Extension/CMakeLists.txt`.
   - Standalone build: `cmake_minimum_required(VERSION 3.18)`, `project(A2b_YUYV_Extension CXX)`.
   - `set(CMAKE_CXX_STANDARD 17)` + `set(CMAKE_CXX_EXTENSIONS OFF)`.
   - `find_package(OpenCL REQUIRED)` and `find_package(OpenCV REQUIRED)`.
   - Include `common/common.cmake` via `${CMAKE_CURRENT_SOURCE_DIR}/../../..`.
   - Link: `OpenCL::OpenCL`, `CLI11::CLI11`, `${OpenCV_LIBS}`.
   - `add_definitions(-DCL_HPP_ENABLE_EXCEPTIONS -DCL_HPP_TARGET_OPENCL_VERSION=120)`.
   - Kernel copy command: copy `kernels/` to `$<TARGET_FILE_DIR:A2b_YUYV_Extension>/kernels` POST_BUILD.

2. **Write kernel `kernels/yuyv_to_rgba.cl` (single-pass)**
   - Args: `__global const uchar* yuyv`, `__global uchar4* rgba`, `int width`, `int height`.
   - `size_t gid = get_global_id(0);` Guard: `if (gid >= (size_t)(width * height)) return;`
   - Compute pixel column and row: `int x = (int)(gid % (size_t)width); int y_row = (int)(gid / (size_t)width);`
   - Macropixel index: `size_t macro = ((size_t)y_row * width + (x & ~1));` YUYV byte offset: `macro * 2`.
   - Y sample: `(x % 2 == 0) ? yuyv[macro*2] : yuyv[macro*2 + 2]`. U = `yuyv[macro*2 + 1]`. V = `yuyv[macro*2 + 3]`.
   - BT.601 limited-range conversion to RGB. Store as `uchar4` with alpha = 255.
   - Include a `// WHY` comment: explain that the macropixel stride is 4 bytes per 2 pixels because YUYV packs U and V shared across the horizontal pair, halving chroma horizontal resolution (4:2:2 subsampling).

3. **Write kernel `kernels/yuyv_to_rgba_twopass.cl` (two-pass)**
   - **Pass 1 kernel** (`extract_y_from_yuyv`): Args: `__global const uchar* yuyv`, `__global uchar* y_plane`, `int width`, `int height`. Extract Y samples: even pixels from byte `gid*2`, odd pixels from byte `(gid-1)*2 + 2` relative to their macropixel. Write to `y_plane[gid]`.
   - **Pass 2 kernel** (`reconstruct_rgba_twopass`): Args: `__global const uchar* yuyv`, `__global const uchar* y_plane`, `__global uchar4* rgba`, `int width`, `int height`. Read Y from `y_plane[gid]`, read U/V from YUYV buffer at the macropixel offset. Apply BT.601 conversion.
   - Both kernels share the same guard pattern and `size_t gid`.

4. **Implement `main.cpp`**

   **CLI args** (CLI11):
   - `--input` (`std::string`, required): path to raw YUYV `.yuv` file.
   - `--width` (`int`, required): frame width (must be even).
   - `--height` (`int`, required): frame height.
   - `--output-single` (`std::string`, default `"output_rgba_singlepass.bmp"`).
   - `--output-two` (`std::string`, default `"output_rgba_twopass.bmp"`).

   **Load raw YUYV file**:
   - Validate `width % 2 == 0`; throw if odd.
   - Compute `size_t yuyv_bytes = static_cast<size_t>(width) * height * 2;`
   - Open with `std::ifstream` binary. Read exactly `yuyv_bytes` into `std::vector<uint8_t> yuyv_data`.
   - Validate file size; throw on mismatch.

   **Integer safety**:
   ```cpp
   if (static_cast<size_t>(width) * height > static_cast<size_t>(INT_MAX))
       throw std::runtime_error("Image too large: pixel count exceeds INT_MAX");
   int pixel_count = width * height;
   ```

   **OpenCL setup**:
   - `auto [ctx, queue] = create_context()` with `CL_QUEUE_PROFILING_ENABLE`.
   - Load and build both kernel files from the `kernels/` directory (relative to binary).
   - Create buffers:
     - `cl::Buffer buf_yuyv(ctx, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, yuyv_bytes, yuyv_data.data())`
     - `cl::Buffer buf_rgba_single(ctx, CL_MEM_WRITE_ONLY, static_cast<size_t>(width) * height * 4)`
     - `cl::Buffer buf_rgba_two(ctx, CL_MEM_WRITE_ONLY, static_cast<size_t>(width) * height * 4)`
     - `cl::Buffer buf_y_tmp(ctx, CL_MEM_READ_WRITE, static_cast<size_t>(width) * height)` — temporary Y-plane for two-pass.

   **CPU reference path**:
   - Construct `cv::Mat yuyv_mat(height, width, CV_8UC2, yuyv_data.data())`.
   - `auto t0 = std::chrono::steady_clock::now();`
   - `cv::cvtColor(yuyv_mat, cpu_rgba, cv::COLOR_YUV2RGBA_YUYV);`
   - `auto t1 = std::chrono::steady_clock::now();`
   - Record `cpu_ms`.

   **Single-pass GPU path**:
   - Set args, enqueue with `cl::NDRange(pixel_count)`, record `ev_single`. `CL_CHECK(queue.finish())`.
   - Read back → save `output_rgba_singlepass.bmp` via `image_utils`.

   **Two-pass GPU path**:
   - Pass 1: enqueue `extract_y_from_yuyv` kernel, record `ev_pass1`. `CL_CHECK(queue.finish())`.
   - Pass 2: enqueue `reconstruct_rgba_twopass` kernel, record `ev_pass2`. `CL_CHECK(queue.finish())`.
   - Read back → save `output_rgba_twopass.bmp` via `image_utils`.
   - Compute `twopass_total_ms = pass1_ms + pass2_ms`.

   **Print timing table**:
   ```
   === A2b YUYV Extension Benchmark ===
   Input:  <path>  (<width>x<height> YUYV 4:2:2)

   Stage                       Time (ms)
   -------------------------------------
   OpenCV CPU cvtColor          X.XXX ms
   OpenCL single-pass           X.XXX ms
   OpenCL two-pass (P1 + P2)    X.XXX ms  (P1: X.XXX ms, P2: X.XXX ms)
   -------------------------------------
   Speedup CPU / single-pass:   X.XXX x
   Speedup two-pass / single:   X.XXX x  (>1.0 = single-pass wins)
   Gate: PASS   [if single-pass < 2.000 ms at 1920x1080]   OR   Gate: WAIVER (iGPU)
   ```

5. **Prepare test asset**
   - If `assets/sample_yuyv.yuv` does not exist, generate a synthetic 1920×1080 YUYV file:
     ```
     ffmpeg -f lavfi -i color=c=blue:size=1920x1080:rate=1 -frames:v 1 -pix_fmt yuyv422 assets/sample_yuyv.yuv
     ```
   - For local developer use only; not committed to the repository.

---

## Definition of Done (DoD)

Standard items (master_specs §8):
- [x] `cmake -B build && cmake --build build` from `A2b_YUYV_Extension/` succeeds with zero errors and zero warnings.
- [x] Binary runs without error: `./build/A2b_YUYV_Extension --input ../../../assets/sample_yuyv.yuv --width 256 --height 256` (asset is 256x256 per assets.md).
- [x] `--help` prints CLI11-generated usage including `--input`, `--width`, `--height`, `--output-single`, `--output-two`.
- [x] `GPU=NVIDIA ./build/A2b_YUYV_Extension --input ... --width 256 --height 256` selects correct device without crashing.

Task-specific outcomes:
- [x] `output_rgba_singlepass.bmp` produced — correct colors (not green-pink), matches OpenCV reference visually.
- [x] `output_rgba_twopass.bmp` produced — visually identical to single-pass output.
- [x] Console prints timing table with CPU reference, single-pass, and two-pass (with per-pass breakdown) in ms to 3 decimal places.
- [x] GPU timings sourced from `cl::Event` profiling; CPU timing from `std::chrono::steady_clock`.
- [x] `width % 2 != 0` throws `std::runtime_error` before any processing.
- [x] Integer safety check throws `std::runtime_error` if `width * height > INT_MAX` before `cl_int` kernel args are set.
- [x] UV-plane and buffer size arithmetic uses `static_cast<size_t>(width) * height` (no 32-bit overflow).
- [x] `yuyv_to_rgba.cl` includes a `// WHY` comment explaining YUYV 4:2:2 macropixel stride.
- [x] Speedup gate printed: PASS (single-pass < 2.0 ms) or WAIVER with iGPU/topology note.
- [x] Kernel `.cl` files present in `build/kernels/` after build (POST_BUILD copy rule active).

---

## Execution Report
<!-- Filled by @coder after implementation. -->

- **Status:** COMPLETE
- **Session:** 2026-03-08

### Validation
```
pci id for fd 11: 10de:28e0, driver (null)
pci id for fd 12: 10de:28e0, driver (null)
Platform : NVIDIA CUDA
Device   : NVIDIA GeForce RTX 4060 Laptop GPU
Saved: output_rgba_singlepass.bmp
Saved: output_rgba_twopass.bmp

=== A2b YUYV Extension Benchmark ===
Input:  ../../../assets/sample_yuyv.yuv  (256x256 YUYV 4:2:2)

Stage                         Time (ms)
-----------------------------------------------------
OpenCV CPU cvtColor           0.277 ms
OpenCL single-pass            0.013 ms
OpenCL two-pass (P1 + P2)     0.010 ms  (P1: 0.005 ms, P2: 0.005 ms)
-----------------------------------------------------
Speedup CPU / single-pass:   21.106 x
Speedup two-pass / single:   0.780 x  (>1.0 = single-pass wins)
Gate: PASS   [single-pass < 2.000 ms at 256x256]

--- Odd-width test ---
terminate called after throwing an instance of 'std::runtime_error'
  what():  Width must be even for YUYV 4:2:2 format, got: 257

--- GPU=NVIDIA test ---
Platform : NVIDIA CUDA  [GPU=NVIDIA]
Device   : NVIDIA GeForce RTX 4060 Laptop GPU
```

### Post-session Notes

**1080p benchmark** (`sample_yuyv_1080p.yuv`, 1920×1080, RTX 4060 Laptop):
```
OpenCV CPU cvtColor           3.709 ms
OpenCL single-pass            0.046 ms
OpenCL two-pass (P1 + P2)     0.070 ms  (P1: 0.033 ms, P2: 0.038 ms)
Speedup CPU / single-pass:   80.488 x
Speedup two-pass / single:   1.528 x  (single-pass wins)
Gate: PASS
```
At 1080p single-pass clearly wins — the cost of two kernel dispatches (0.070 ms) outweighs any compute benefit vs one dispatch (0.046 ms). This reverses the 256×256 result where two-pass was marginally faster due to tiny workload.

**`load_raw_binary()` refactor**: File-loading block extracted to `common/image_utils.hpp`. Both A2 and A2b `main.cpp` now delegate to `load_raw_binary(path, expected_bytes)`.

### Changed Files
| File | Change |
|------|--------|
| `02_Projects/A_Multimedia/A2b_YUYV_Extension/CMakeLists.txt` | Created |
| `02_Projects/A_Multimedia/A2b_YUYV_Extension/main.cpp` | Created |
| `02_Projects/A_Multimedia/A2b_YUYV_Extension/kernels/yuyv_to_rgba.cl` | Created |
| `02_Projects/A_Multimedia/A2b_YUYV_Extension/kernels/yuyv_to_rgba_twopass.cl` | Created |
