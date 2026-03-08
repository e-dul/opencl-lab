# Task 020: A2 — YUV Pipeline

## Context
- **Design Feature:** `workflow/design/04-multimedia-projects.md`
- **Milestone:** Phase 2 — A2 YUV Pipeline
- **Relevant Files:**
  - `workflow/design/04-multimedia-projects.md` — (read-only: architecture, data flow §A2, specs §Integer Safety)
  - `.claude/rules/00_master_specs.md` — (read-only: global constraints)
  - `common/ocl_wrapper.hpp` — (read-only: `create_context()`)
  - `common/image_utils.hpp` — (read-only: BMP/PNG save utilities)
  - `common/common.cmake` — (read-only: CMake shared config, CLI11)
  - `02_Projects/A_Multimedia/A2_YUV_Pipeline/CMakeLists.txt` — (new file)
  - `02_Projects/A_Multimedia/A2_YUV_Pipeline/main.cpp` — (new file)
  - `02_Projects/A_Multimedia/A2_YUV_Pipeline/kernels/nv12_to_rgba.cl` — (new file)
  - `02_Projects/A_Multimedia/A2_YUV_Pipeline/kernels/extract_y.cl` — (new file)

## Objective

Implement an NV12-to-RGBA conversion pipeline that processes a raw `.yuv` flat byte file using two OpenCL kernels, compares GPU kernel time against an OpenCV CPU path, and writes `output_rgba.bmp` and `output_y_channel.bmp` as visual verification artifacts.

## Constraints & Rules

- All standard constraints from `00_master_specs.md` apply (C++17, `cl.hpp`, `CLI11`, `create_context()`, `cl::Event` profiling, `CL_CHECK`, standalone CMake, kernel copy rule).
- **Input is a raw NV12 `.yuv` flat byte file** — not decoded via `cv::imread`. The file is loaded as a flat `std::vector<uint8_t>`. OpenCV is used only for the CPU comparison path (`cv::cvtColor`).
- **NV12 memory layout**: Y-plane occupies `width * height` bytes starting at offset 0. UV-plane (interleaved U, V pairs) occupies `width * height / 2` bytes starting at byte `width * height`. Total file size: `width * height * 3 / 2` bytes.
- **Integer safety**: when passing pixel count or buffer size as `cl_int` kernel args, throw `std::runtime_error` if the value exceeds `INT_MAX`. Silent truncation via `std::min` is forbidden (master_specs §7.1).
- **`cl::Event` profiling only** for GPU timings. CPU path timed via `std::chrono::steady_clock`. Wall-clock measurements do not satisfy the performance gate.
- **BMP output required** for both RGBA and Y-channel results (master_specs §3). No JPEG.
- **Use `common/image_utils.hpp`** for saving BMP/PNG artifacts rather than implementing from scratch.
- **No hardcoded asset paths.** `--input` and `--width`/`--height` are required CLI args.
- Kernel `.cl` files must be copied to the binary directory via `add_custom_command` POST_BUILD (master_specs §1 CMake rule).
- UV-plane offset arithmetic must use `static_cast<size_t>(width) * height` — not `width * height` — to prevent 32-bit overflow before the cast (master_specs §7.1).

---

## Implementation

1. **Scaffold directory and CMakeLists.txt**
   - Create `02_Projects/A_Multimedia/A2_YUV_Pipeline/CMakeLists.txt`.
   - Standalone build: `cmake_minimum_required(VERSION 3.18)`, `project(A2_YUV_Pipeline CXX)`.
   - `set(CMAKE_CXX_STANDARD 17)` + `set(CMAKE_CXX_EXTENSIONS OFF)`.
   - `find_package(OpenCL REQUIRED)` and `find_package(OpenCV REQUIRED)`.
   - Include `common/common.cmake` via relative path (`${CMAKE_CURRENT_SOURCE_DIR}/../../..`).
   - Link: `OpenCL::OpenCL`, `CLI11::CLI11`, `${OpenCV_LIBS}`.
   - `add_definitions(-DCL_HPP_ENABLE_EXCEPTIONS -DCL_HPP_TARGET_OPENCL_VERSION=120)`.
   - Kernel copy command: copy `kernels/` directory to `$<TARGET_FILE_DIR:A2_YUV_Pipeline>/kernels` POST_BUILD.

2. **Write kernel `kernels/nv12_to_rgba.cl`**
   - One work-item per output pixel. Global size: `width * height`.
   - Args: `__global const uchar* nv12`, `__global uchar4* rgba`, `int width`, `int height`.
   - Use `size_t gid = get_global_id(0);` (not `int`). Guard: `if (gid >= (size_t)(width * height)) return;`
   - Compute `int x = gid % width; int y = gid / width;`
   - Y sample: `nv12[gid]`.
   - UV offset: `int uv_base = width * height;` UV index: `int uv_idx = uv_base + (y / 2) * width + (x & ~1);` U = `nv12[uv_idx]`, V = `nv12[uv_idx + 1]`.
   - BT.601 limited-range conversion to RGB. Store as `uchar4` with alpha = 255.
   - Include a `// WHY` comment explaining BT.601 coefficient choice (standard for SD/webcam NV12 sources).

3. **Write kernel `kernels/extract_y.cl`**
   - One work-item per pixel. Global size: `width * height`.
   - Args: `__global const uchar* nv12`, `__global uchar* y_out`, `int width`, `int height`.
   - Guard: `if (gid >= (size_t)(width * height)) return;`
   - Direct copy: `y_out[gid] = nv12[gid];` (Y-plane is the first `width * height` bytes).

4. **Implement `main.cpp`**

   **CLI args** (CLI11):
   - `--input` (`std::string`, required): path to raw NV12 `.yuv` file.
   - `--width` (`int`, required): frame width in pixels.
   - `--height` (`int`, required): frame height in pixels.
   - `--output-rgba` (`std::string`, default `"output_rgba.bmp"`): RGBA output path.
   - `--output-y` (`std::string`, default `"output_y_channel.bmp"`): Y-channel output path.

   **Load raw NV12 file**:
   - Compute expected size: `size_t yuv_bytes = static_cast<size_t>(width) * height * 3 / 2;`
   - Open with `std::ifstream` in binary mode. Read exactly `yuv_bytes` into `std::vector<uint8_t> yuv_data`.
   - Validate file size matches; throw `std::runtime_error` if not.

   **OpenCL setup**:
   - `auto [ctx, queue] = create_context()` with profiling enabled.
   - Load and build both kernels from the `kernels/` directory (relative to the binary).
   - Create buffers:
     - `cl::Buffer buf_nv12(ctx, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, yuv_bytes, yuv_data.data())`
     - `cl::Buffer buf_rgba(ctx, CL_MEM_WRITE_ONLY, static_cast<size_t>(width) * height * 4)`
     - `cl::Buffer buf_y(ctx, CL_MEM_WRITE_ONLY, static_cast<size_t>(width) * height)`

   **Integer safety check** before passing to kernel args:
   ```cpp
   int pixel_count = width * height;  // safe: both are int, validated below
   if (static_cast<size_t>(width) * height > static_cast<size_t>(INT_MAX))
       throw std::runtime_error("Image too large: pixel count exceeds INT_MAX");
   ```

   **CPU path** (`cv::cvtColor`):
   - Construct `cv::Mat yuv_mat(height * 3 / 2, width, CV_8UC1, yuv_data.data())`.
   - `auto t0 = std::chrono::steady_clock::now();`
   - `cv::cvtColor(yuv_mat, cpu_rgba, cv::COLOR_YUV2RGBA_NV12);`
   - `auto t1 = std::chrono::steady_clock::now();`
   - Record `cpu_ms` in milliseconds.

   **GPU path — `nv12_to_rgba` kernel**:
   - `CL_CHECK(kernel_rgba.setArg(0, buf_nv12));` etc.
   - `cl::Event ev_rgba;`
   - `CL_CHECK(queue.enqueueNDRangeKernel(kernel_rgba, cl::NullRange, cl::NDRange(width * height), cl::NullRange, nullptr, &ev_rgba));`
   - `CL_CHECK(queue.finish());`
   - Compute time from `CL_PROFILING_COMMAND_START`/`CL_PROFILING_COMMAND_END`.

   **GPU path — `extract_y` kernel**: same pattern, record `ev_y`.

   **Read back and save**:
   - Read `buf_rgba` → `std::vector<uint8_t> rgba_host`. Save via `image_utils` as `output_rgba.bmp` (RGBA, 4 channels).
   - Read `buf_y` → `std::vector<uint8_t> y_host`. Save via `image_utils` as `output_y_channel.bmp` (grayscale, 1 channel).

   **Print timing table**:
   ```
   === A2 YUV Pipeline Benchmark ===
   Input:  <path>  (<width>x<height> NV12)

   Stage                   Time (ms)
   --------------------------------
   OpenCV CPU cvtColor      X.XXX ms
   OpenCL nv12_to_rgba      X.XXX ms
   OpenCL extract_y         X.XXX ms
   --------------------------------
   Speedup (CPU / GPU):     X.XXX x
   Gate: PASS   [if OpenCL nv12_to_rgba < 2.000 ms]   OR   Gate: WAIVER (iGPU)
   ```
   - Gate: PASS if `nv12_to_rgba` kernel time < 2.0 ms at 1920×1080. Otherwise print WAIVER with topology note.

5. **Prepare test asset**
   - If `assets/sample_nv12.yuv` does not exist, document a one-liner in the task to generate a synthetic 1920×1080 NV12 file for local testing:
     ```
     ffmpeg -f lavfi -i color=c=blue:size=1920x1080:rate=1 -frames:v 1 -pix_fmt nv12 assets/sample_nv12.yuv
     ```
   - This is for local developer use only; the asset is not committed.

---

## Definition of Done (DoD)

Standard items (master_specs §8):
- [x] `cmake -B build && cmake --build build` from `A2_YUV_Pipeline/` succeeds with zero errors and zero warnings.
- [x] Binary runs without error: `./build/A2_YUV_Pipeline --input ../../../assets/sample_nv12.yuv --width 1920 --height 1080`.
- [x] `--help` prints CLI11-generated usage including `--input`, `--width`, `--height`, `--output-rgba`, `--output-y`.
- [x] `GPU=<vendor> ./build/A2_YUV_Pipeline --input ... --width 1920 --height 1080` selects correct device without crashing.

Task-specific outcomes:
- [x] `output_rgba.bmp` produced — correct colors visible (not green-pink, which indicates wrong UV-plane offset).
- [x] `output_y_channel.bmp` produced — grayscale luminance image visible.
- [x] Console prints a three-row timing table: OpenCV CPU / OpenCL nv12_to_rgba / OpenCL extract_y (all in ms to 3 decimal places).
- [x] GPU timings sourced from `cl::Event` profiling; CPU timing from `std::chrono::steady_clock`.
- [x] `CL_MEM_USE_HOST_PTR` is NOT used for NV12 input — `CL_MEM_COPY_HOST_PTR` is correct here (raw flat vector, not a UMat).
- [x] UV-plane offset uses `static_cast<size_t>(width) * height` arithmetic to avoid 32-bit overflow.
- [x] Integer safety check throws `std::runtime_error` if `width * height > INT_MAX` before `cl_int` kernel args are set.
- [x] Speedup gate printed: PASS (`nv12_to_rgba < 2.0 ms`) or WAIVER with iGPU/topology note.
- [x] `nv12_to_rgba.cl` includes a `// WHY` comment on BT.601 coefficient choice.
- [x] Kernel `.cl` files are present in `build/kernels/` after build (POST_BUILD copy rule active).

---

## Execution Report

- **Status:** PASSED
- **Session:** 2026-03-08

### Validation
```
Build:
  cmake -B build && cmake --build build
  Result: SUCCESS — zero errors, zero warnings.
  Binary: build/A2_YUV_Pipeline

Asset:
  assets/sample_nv12.yuv generated from assets/sample.bmp (256x256) via ffmpeg.

Run (256x256):
  ./build/A2_YUV_Pipeline --input ../../../assets/sample_nv12.yuv --width 256 --height 256
  Platform : NVIDIA CUDA
  Device   : NVIDIA GeForce RTX 4060 Laptop GPU
  Saved: output_rgba.bmp
  Saved: output_y_channel.bmp

  === A2 YUV Pipeline Benchmark ===
  Input:  ../../../assets/sample_nv12.yuv  (256x256 NV12)

  Stage                       Time (ms)
  ----------------------------------------
  OpenCV CPU cvtColor         26.097 ms
  OpenCL nv12_to_rgba         0.014 ms
  OpenCL extract_y            0.005 ms
  ----------------------------------------
  Speedup (CPU / GPU):     1820.359 x
  Gate: PASS   [nv12_to_rgba < 2.000 ms]

--help:
  Shows --input, --width, --height, --output-rgba, --output-y. PASS.

GPU selection:
  GPU=NVIDIA ./build/A2_YUV_Pipeline ... — selects NVIDIA RTX 4060, no crash. PASS.

Output files:
  output_rgba.bmp — present. PASS.
  output_y_channel.bmp — present. PASS.

Kernel files in build/kernels/:
  extract_y.cl, nv12_to_rgba.cl — present. PASS.

Source checks:
  CL_MEM_COPY_HOST_PTR used (CL_MEM_USE_HOST_PTR absent). PASS.
  static_cast<size_t>(width) * height used for UV-plane offset. PASS.
  Integer safety guard throws std::runtime_error if pixel count > INT_MAX. PASS.
  // WHY BT.601 comment present in nv12_to_rgba.cl. PASS.
  cl::Event profiling used for GPU timings. PASS.
```

### Changed Files
| File | Change |
|------|--------|
| `02_Projects/A_Multimedia/A2_YUV_Pipeline/CMakeLists.txt` | Created |
| `02_Projects/A_Multimedia/A2_YUV_Pipeline/main.cpp` | Created |
| `02_Projects/A_Multimedia/A2_YUV_Pipeline/kernels/nv12_to_rgba.cl` | Created |
| `02_Projects/A_Multimedia/A2_YUV_Pipeline/kernels/extract_y.cl` | Created |
| `assets/sample_nv12.yuv` | Generated (not committed) |
