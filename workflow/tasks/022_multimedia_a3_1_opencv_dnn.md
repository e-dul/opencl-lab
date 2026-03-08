# Task 022: A3_1 — OpenCV DNN T-API Inference + Bokeh Blur

## Context
- **Design Feature:** `workflow/design/04-multimedia-projects.md`
- **Milestone:** Phase 3 — A3_1 OpenCV DNN (T-API): High-level inference, UMat stays on GPU.
- **Relevant Files:**
  - `workflow/design/04-multimedia-projects.md` — (read-only: architecture, data flow, known risks)
  - `.claude/rules/00_master_specs.md` — (read-only: global constraints)
  - `common/ocl_wrapper.hpp` — (read-only: `create_context()`)
  - `common/image_utils.hpp` — (read-only: image IO utilities)
  - `common/common.cmake` — (read-only: CLI11 integration)
  - `02_Projects/A_Multimedia/A3_1_OpenCV_DNN/` — (new directory: all files to create)

## Objective

Implement `A3_1_OpenCV_DNN`: load a BGRA image into a `cv::UMat`, run selfie segmentation via OpenCV DNN with `DNN_TARGET_OPENCL`, extract the resulting mask `cl_mem` handle without a host copy, then apply a full-frame bokeh blur OpenCL kernel conditioned on that mask — producing `output_mask.bmp` and `output_blurred.bmp` with inference and blur times printed separately.

## Constraints & Rules

- **Standard constraints** inherited from `.claude/rules/00_master_specs.md`: C++17, `cl.hpp`, CLI11, `create_context()`, `cl::Event` profiling, `CL_CHECK`, standalone CMake, kernel copy rule, integer overflow safety.
- **OpenCV DNN target**: `net.setPreferableTarget(DNN_TARGET_OPENCL)`. If `DNN_TARGET_OPENCL` is unavailable at runtime, print a descriptive error and `exit(0)` — do NOT crash or silently fall back.
- **DNN OpenCL verification**: Before running inference, verify OpenCV was built with OpenCL by calling `cv::ocl::haveOpenCL()`. If false, print a message and exit with code 0.
- **UMat handle lifetime**: The `cl_mem` handle retrieved via `umat.handle(cv::ACCESS_READ)` is valid only while the `UMat` is alive and unmodified. The blur kernel `enqueueNDRangeKernel` + `finish()` must complete before the `UMat` goes out of scope.
- **No extra memory copy**: The mask must flow from DNN output `cv::UMat` directly to `cl::Buffer` via handle extraction. `clEnqueueReadBuffer` / `enqueueWriteBuffer` on the mask data is forbidden.
- **Bokeh kernel naive branch**: The kernel must use a literal `if (mask[id] == BACKGROUND)` branch — do NOT pre-optimize with `select()` (thread divergence is intentional per design decision §3).
- **CLI args**: `--input` (path to BGRA BMP/PNG), `--model` (path to `.onnx`), `--threshold` (float, default 0.5). `GPU` env var for device selection.
- **Integer overflow safety**: when computing buffer sizes from width × height, promote the first operand to `size_t` before multiplying (master_specs §7.1).
- **OpenCV dependency**: `find_package(OpenCV REQUIRED)` in CMakeLists.txt. OpenCV 4.5+.

---

## Implementation

1. **Directory scaffold**: Create `02_Projects/A_Multimedia/A3_1_OpenCV_DNN/` with `CMakeLists.txt`, `main.cpp`, and `kernels/bokeh_blur.cl`.

2. **CMakeLists.txt**:
   - Standalone: `cmake_minimum_required(VERSION 3.18)`, `set(CMAKE_CXX_STANDARD 17)`, `set(CMAKE_CXX_EXTENSIONS OFF)`.
   - `find_package(OpenCL REQUIRED)`, `find_package(OpenCV REQUIRED)`.
   - Include `common/common.cmake` for CLI11 and shared headers.
   - Link: `OpenCL::OpenCL`, `${OpenCV_LIBS}`, `CLI11::CLI11`.
   - Kernel copy rule: copy `kernels/` to binary dir post-build (master_specs §1).

3. **`main.cpp` — host logic**:
   a. Parse CLI args with CLI11: `--input`, `--model`, `--threshold`.
   b. Call `create_context()` from `common/ocl_wrapper.hpp`.
   c. Check `cv::ocl::haveOpenCL()` — exit 0 with message if false.
   d. Load input image: `cv::imread(input_path, cv::IMREAD_COLOR)` → `cv::Mat bgr`.
   e. Convert to `cv::UMat`: `bgr.getUMat(cv::ACCESS_READ)` (stays GPU-resident).
   f. Load model: `cv::dnn::readNetFromONNX(model_path)`. Set backend `DNN_BACKEND_OPENCV`, target `DNN_TARGET_OPENCL`. Guard target availability as noted in constraints.
   g. Preprocess: build blob from UMat (`cv::dnn::blobFromImage`), set as net input.
   h. **Time inference** (CPU wall-clock via `std::chrono::steady_clock`): `net.forward(output_umat, output_layer_name)`. The output UMat contains the float mask.
   i. **Extract cl_mem handle**: `cl_mem raw_mask = static_cast<cl_mem>(output_umat.handle(cv::ACCESS_READ))`. Wrap as `cl::Buffer mask_buf(raw_mask, true)` (retain = true, no copy).
   j. Load input into a separate `cl::Buffer input_buf` for the blur kernel (from `bgr` → RGBA flat buffer).
   k. Create `cl::Buffer output_buf` for blurred result.
   l. Build kernel from `kernels/bokeh_blur.cl`. Set args: `input_buf`, `output_buf`, `mask_buf`, width, height (as `cl_int`, with overflow guard), threshold.
   m. **Time blur kernel** via `cl::Event`: `enqueueNDRangeKernel` with 1D global size = `width * height`.
   n. `enqueueReadBuffer` → host vector → save `output_blurred.bmp` via `common/image_utils.hpp` or `stb_image_write`.
   o. Threshold float mask → 8-bit grayscale → save `output_mask.bmp`.
   p. Print timing table:
      ```
      [A3_1] Inference (CPU wall-clock): X.XXX ms
      [A3_1] Bokeh blur (cl::Event):     X.XXX ms
      [A3_1] Total:                       X.XXX ms
      ```

4. **`kernels/bokeh_blur.cl`**:
   - Signature: `__kernel void bokeh_blur(__global const uchar4* input, __global uchar4* output, __global const float* mask, int width, int height, float threshold)`.
   - `size_t gid = get_global_id(0)` — NOT `int`.
   - Guard: `if (gid >= (size_t)(width * height)) return;`.
   - Branch: `if (mask[gid] < threshold) { /* blur: box filter 5×5 from input */ } else { output[gid] = input[gid]; }`.
   - Box filter samples neighbours with boundary clamp (do not read out of bounds).

5. **Prerequisite check**: Assert `assets/selfie_segmentation.onnx` exists in repo root. If missing, the CLI `--model` arg must point to it; document this in the run instructions within the task.

---

## Definition of Done (DoD)

Standard items from `.claude/rules/00_master_specs.md §8` apply:
- [ ] `cmake -B build && cmake --build build` succeeds with zero errors and zero warnings.
- [ ] Binary runs without arguments and completes without error (with a default or required `--input` / `--model` arg — if required, a missing-arg error from CLI11 is acceptable; zero crash on valid input is mandatory).
- [ ] `--help` prints CLI11-generated usage including `--input`, `--model`, `--threshold`.
- [ ] `GPU=<vendor> ./build/a3_1_opencv_dnn` selects correct device without crashing.

Task-specific:
- [ ] `output_mask.bmp` written — visually shows foreground (white) vs background (black) regions.
- [ ] `output_blurred.bmp` written — background pixels are blurred, foreground pixels are sharp.
- [ ] Console prints a 3-row timing table with inference and blur times separated.
- [ ] `cv::ocl::haveOpenCL()` check is present; binary exits with code 0 and a message if false.
- [ ] No `clEnqueueReadBuffer` / `enqueueWriteBuffer` on the mask data (handle extracted directly from UMat).
- [ ] Kernel uses `size_t gid = get_global_id(0)` with an upper-bound guard.
- [ ] Integer overflow safety: buffer size computed as `static_cast<size_t>(width) * height * channels`.
- [ ] Performance gate (hardware-dependent, discrete GPU required): inference + blur < 15 ms @ 1080p. iGPU results documented but exempt from pass/fail.

---

## Execution Report

- **Status:** PENDING
- **Session:** —

### Validation
```
[output here]
```

### Changed Files
| File | Change |
|------|--------|
| `02_Projects/A_Multimedia/A3_1_OpenCV_DNN/CMakeLists.txt` | Created |
| `02_Projects/A_Multimedia/A3_1_OpenCV_DNN/main.cpp` | Created |
| `02_Projects/A_Multimedia/A3_1_OpenCV_DNN/kernels/bokeh_blur.cl` | Created |

### Remaining
- [ ] All DoD items above.
