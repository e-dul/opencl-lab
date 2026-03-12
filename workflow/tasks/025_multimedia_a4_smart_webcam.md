# Task 025: A4 Smart Webcam — Flagship Live Pipeline

## Context
- **Design Feature:** `workflow/design/04-multimedia-projects.md`
- **Milestone:** Phase 5 — A4 AI Smart Webcam (Bokeh mode)
- **Relevant Files:**
  - `workflow/design/04-multimedia-projects.md` §Architecture §A4 Data Flow — read-only: architecture reference
  - `02_Projects/A_Multimedia/Multimedia.md` §A4_Smart_Webcam — read-only: educational spec
  - `02_Projects/A_Multimedia/A3_2_OpenVINO_GPU/` — read-only: reference implementation for RemoteTensor wiring and `preprocess_nchw.cl`
  - `common/ocl_wrapper.hpp` — read-only: `create_context()`
  - `assets/face.png` — read-only: offline test image
  - `assets/selfie_segmentation.onnx` — read-only: segmentation model
  - `02_Projects/A_Multimedia/A4_Smart_Webcam/` — new directory (all files created here)

## Objective

Build a zero-copy, live-or-offline GPU pipeline in `A4_Smart_Webcam/` that chains OpenVINO inference directly into an OpenCL bokeh blur kernel via shared `cl_mem`, runs at ≥ 30 FPS @ 1080p on a mid-range discrete GPU, and prints per-frame timing with Frame 1 annotated as JIT warm-up.

## Constraints & Rules

- All standard constraints from `.claude/rules/00_master_specs.md` apply (C++17, `cl.hpp`, CLI11, `create_context()`, `cl::Event` profiling, `CL_CHECK`, standalone CMake, kernel copy rule).
- **OpenVINO dependency**: `find_package(OpenVINO REQUIRED)`. No OpenCV dependency in this sub-project.
- **Shared OpenCL context**: OpenVINO must be initialized with `ClContext(core, ctx.get())` — the same `cl_context` as all other OpenCL operations. Separate context creation is forbidden.
- **Output tensor pre-binding**: `req.set_output_tensor()` with a pre-allocated `ClBufferTensor` must be called before the first `req.infer()`. Lazy binding (post-`infer()` extraction without pre-binding) is forbidden.
- **Frame loop gate**: `CL_CHECK(queue.finish())` must be called after the blur kernel enqueue, before any loop restart or `req.infer()` call. Omitting this causes undefined behaviour (corrupted output / crash after N frames).
- **Buffer flags**: All `cl::Buffer` objects passed to the OpenVINO GPU plugin must use `CL_MEM_READ_WRITE`. `CL_MEM_READ_ONLY` / `CL_MEM_WRITE_ONLY` will be rejected at runtime.
- **`retain=true`** when wrapping output `cl_mem` from `ClBufferTensor::get()` — the handle is owned by OpenVINO; `retain=false` causes a double-free.
- **Thread divergence**: The bokeh blur kernel must use a naive `if (mask_value < threshold) { apply_blur; }` conditional. Do not pre-optimize with `select()` — that is the mini-challenge left intentional.
- **JIT annotation**: Print Frame 1 timing with the suffix `(JIT warm-up — do not measure FPS here)`. FPS average must exclude frame 1.
- **Integer safety**: If `width * height > INT_MAX`, throw `std::runtime_error` before passing to any kernel as `cl_int`.

---

## Implementation

### Directory structure to create

```
02_Projects/A_Multimedia/A4_Smart_Webcam/
├── CMakeLists.txt
├── main.cpp
└── kernels/
    ├── preprocess_nchw.cl    (copy from A3_2 or re-implement — same spec)
    └── bokeh_blur.cl
```

### Steps

1. **Scaffold `CMakeLists.txt`**
   - `cmake_minimum_required(VERSION 3.18)`, `project(smart_webcam)`, `set(CMAKE_CXX_STANDARD 17)`, `set(CMAKE_CXX_EXTENSIONS OFF)`.
   - `find_package(OpenCL REQUIRED)`, `find_package(OpenVINO REQUIRED)`.
   - Include `common/common.cmake` (CLI11, `create_context()`).
   - Link: `OpenCL::OpenCL`, `openvino::runtime`.
   - Kernel copy rule: `add_custom_command` to copy `kernels/` to `$<TARGET_FILE_DIR:smart_webcam>/kernels/`.

2. **Write `kernels/preprocess_nchw.cl`**
   - Identical spec to A3_2: reads RGBA `uchar4` at `(x, y)`, normalises to `[0, 1]` f32, writes NCHW layout `[C][H][W]` into a flat f32 buffer at model resolution (256×256 for selfie segmentation). Input is full-res RGBA; the kernel must be dispatched with `global_work_size = {model_w, model_h}`.
   - Signature: `kernel void preprocess_nchw(__global const uchar* rgba_in, int in_w, int in_h, __global float* nchw_out, int model_w, int model_h)`.

3. **Write `kernels/bokeh_blur.cl`**
   - Reads full-res RGBA buffer + float mask buffer (model resolution — bilinear-interpolated index mapping required).
   - Per-pixel: if `mask_value < 0.5f` (background), apply a 5×5 box blur from the RGBA buffer; else copy pixel unchanged.
   - Naive conditional (`if/else`) — intentional thread divergence left as mini-challenge.
   - Signature: `kernel void bokeh_blur(__global const uchar* rgba_in, __global const float* mask, int width, int height, int model_w, int model_h, __global uchar* rgba_out)`.

4. **Write `main.cpp`**

   a. **CLI11 setup** — flags:
      - `--input` (string, path, optional — offline mode)
      - `--loop` (bool flag — replay `--input` in a loop; requires `--input`)
      - `--device` (int, default 0 — webcam index; ignored when `--input` set)
      - `--width` (int, default 1920)
      - `--height` (int, default 1080)
      - `--runs` (int, default 30 — number of frames to process in offline/loop mode before exit; webcam mode runs until key press)

   b. **OpenCL context** — `create_context()` from `common/ocl_wrapper.hpp`. Enable profiling queue: `CL_QUEUE_PROFILING_ENABLE`.

   c. **OpenVINO init with shared context** — use `ClContext(core, ctx.get())` overload. Compile model. Create `InferRequest`. Read model input/output shapes dynamically (`model.input().get_shape()`, `model.output().get_shape()`).

   d. **Buffer allocation** (all `CL_MEM_READ_WRITE`):
      - `rgba_buf`: full-res RGBA, size = `width * height * 4`.
      - `nchw_buf`: model input, size = `model_c * model_h * model_w * sizeof(float)`.
      - `mask_buf`: model output, size = `model_out_elements * sizeof(float)`.
      - `out_buf`: full-res RGBA output, same size as `rgba_buf`.

   e. **Pre-bind output tensor** before first `infer()`:
      ```cpp
      auto out_remote = remote_ctx.create_tensor(output_elem_type, output_shape, mask_buf.get());
      req.set_output_tensor(out_remote);
      ```

   f. **Frame loop** (for each frame):
      1. **Capture** (`std::chrono::steady_clock`): read frame from webcam or copy `--input` image into `rgba_buf` via `enqueueWriteBuffer`.
      2. **Preprocess** (`cl::Event`): dispatch `preprocess_nchw` over `{model_w, model_h}`.
      3. **Inference** (`std::chrono::steady_clock`): set input RemoteTensor from `nchw_buf`, call `req.infer()`.
      4. **Blur kernel** (`cl::Event`): dispatch `bokeh_blur` over `{width, height}`.
      5. **`CL_CHECK(queue.finish())`** — mandatory gate before loop restart.
      6. **Display** (`std::chrono::steady_clock`): read `out_buf` back to host, show via `cv::imshow` or save `output_blurred.bmp` (offline mode).
      7. **Print timing** — see format below. Frame 1: append `(JIT warm-up — do not measure FPS here)`. Frames 2+: include `Total` line and running FPS average.

   g. **Output format** (per design §A4 Data Flow):
      ```
      Frame 1 (JIT warm-up — do not measure FPS here):
        Capture:    X.X ms
        Preprocess: X.X ms
        Inference: XXX.X ms
        Kernel:     X.X ms
        Display:    X.X ms

      Frame N (stable):
        Capture:    X.X ms
        Preprocess: X.X ms
        Inference:  X.X ms
        Kernel:     X.X ms
        Display:    X.X ms
        Total:     XX.X ms
      FPS avg (frames 2–N): XX.X
      ```

   h. **Offline save**: in `--input` mode (non-loop or after `--runs` frames), save `output_blurred.bmp` using `stb_image_write.h`.

5. **Verify build** — `cmake -B build && cmake --build build` with zero errors and zero warnings.

---

## Definition of Done (DoD)

Standard items from `.claude/rules/00_master_specs.md §8`:
- [ ] `cmake -B build && cmake --build build` succeeds with zero errors and zero warnings.
- [ ] Binary runs without arguments and completes without error (prints `--help` hint and exits cleanly, or runs offline default).
- [ ] `--help` prints CLI11-generated usage including all defined flags.
- [ ] `GPU=INTEL ./build/smart_webcam --input ../../../assets/face.png --loop --runs 5` selects the Intel iGPU, runs 5 frames, prints per-frame timing breakdown without crash.

Task-specific outcomes:
- [ ] Frame 1 console output contains the string `JIT warm-up`.
- [ ] Frames 2+ console output contains `Total:` and `FPS avg` lines.
- [ ] `output_blurred.bmp` is written in offline mode (`--input` without `--loop`, or after `--runs` completes). File is a valid BMP with the subject sharp and background visually blurred.
- [ ] `output_blurred.bmp` is not a solid color, not all-black, and not identical to the input — blur must be visually distinguishable.
- [ ] `queue.finish()` is called after the blur kernel enqueue (verified by code review — absence causes frame-N crash, not frame-1 crash).
- [ ] All `cl::Buffer` objects passed to OpenVINO use `CL_MEM_READ_WRITE`.
- [ ] Output tensor pre-bound via `req.set_output_tensor()` before first `req.infer()`.
- [ ] **Performance gate (discrete GPU)**: `Total:` for frames 2+ is < 33 ms @ 1920×1080 on a mid-range discrete GPU (≥ GTX 1060 / RX 580). iGPU results must be documented but are exempt from pass/fail per hardware-waiver clause.

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
| `02_Projects/A_Multimedia/A4_Smart_Webcam/CMakeLists.txt` | Created |
| `02_Projects/A_Multimedia/A4_Smart_Webcam/main.cpp` | Created |
| `02_Projects/A_Multimedia/A4_Smart_Webcam/kernels/preprocess_nchw.cl` | Created |
| `02_Projects/A_Multimedia/A4_Smart_Webcam/kernels/bokeh_blur.cl` | Created |

### Remaining
- [ ] Implementation pending
