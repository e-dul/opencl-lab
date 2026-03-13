# Task 026: A5 Privacy Mode — T-API ROI Blur vs Custom Kernel

## Context

- **Design Feature:** `workflow/design/04-multimedia-projects.md` Phase 6
- **Relevant Files:**
  - `workflow/design/04-multimedia-projects.md` — (read-only)
  - `02_Projects/A_Multimedia/A4_Smart_Webcam/` — (read-only: reference for CMake/common patterns)
  - `02_Projects/A_Multimedia/A5_Privacy_Mode/` — (new directory, all files to create)
  - `assets/blaze.onnx` — (already present)
  - `common/ocl_wrapper.hpp`, `common/opencl_utils.hpp`, `common/image_utils.hpp` — (read-only)

## Objective

New standalone app `A5_Privacy_Mode` that:

1. Detects a face using `blaze.onnx` via OpenCV DNN with `DNN_TARGET_OPENCL` (T-API inference — no OpenVINO).
2. Blurs the face ROI via two paths and prints per-frame timing for both:
   - **Path A (T-API):** `cv::UMat` submat + `cv::blur` — zero custom kernel code, GPU-dispatched automatically.
   - **Path B (custom kernel):** `roi_blur.cl` with `global_work_offset` — explicit NDRange control.
3. Supports both file input and live webcam.
4. Saves output BMP in file mode.

**Core teaching point:** `DNN_TARGET_OPENCL` and `cv::UMat` give GPU acceleration without writing a single OpenCL kernel. Path B shows the explicit alternative — letting the student judge the trade-off.

## Constraints & Rules

- All standard constraints from `.claude/rules/00_master_specs.md` apply (C++17, `cl.hpp`, CLI11, `create_context()`, standalone CMake, kernel copy rule).
- **No OpenVINO dependency** in this module. Only `OpenCV` and `OpenCL`.
- **`cv::ocl::attachContext()`** must be called after `create_context()` so T-API uses our `cl_context`. Read `common/ocl_wrapper.hpp` to understand what `OclContext` exposes.
- **`cl::Event` profiling** mandatory for path B kernel dispatch.
- **Path A timing** via `std::chrono::steady_clock` + `queue.finish()` sync gate before stop.
- **Integer safety (§7.1):** promote `int` dimensions before multiplying for buffer sizes.

---

## Implementation

### 1. Directory structure

```text
02_Projects/A_Multimedia/A5_Privacy_Mode/
  CMakeLists.txt
  main.cpp
  kernels/
    roi_blur.cl
```

### 2. CMakeLists.txt

- Copy from `A4_Smart_Webcam/CMakeLists.txt` as starting point; remove OpenVINO lines.
- `find_package(OpenCV REQUIRED COMPONENTS core imgproc dnn highgui videoio)`
- `find_package(OpenCL REQUIRED)` via `common/common.cmake`.
- Binary name: `privacy_mode`.
- Kernel copy rule for `kernels/roi_blur.cl`.

### 3. CLI flags

- `--input <path>`: input image file. Mutually exclusive with `--device`.
- `--device <int>` (default `0`): webcam index for live capture. Mutually exclusive with `--input`.
- `--width <int>` (default `640`), `--height <int>` (default `480`): capture resolution (webcam mode only).
- `--output <path>` (default `output.bmp`): saved result (file mode only; ignored in webcam mode).
- `--face-model <path>` (default `assets/blaze.onnx`).
- `--conf-threshold <float>` (default `0.6`).
- `--blur-radius <int>` (default `15`): box blur size for both paths.
- `--loop`: replay `--input` in a live preview window (offline webcam simulation; requires `--input`).
- `--exposure <int>` (optional, default `-1` = auto): manual V4L2 absolute exposure in 100 µs units. Webcam mode only; ignored in file mode. Sets `CAP_PROP_AUTO_EXPOSURE=1` then `CAP_PROP_EXPOSURE` when value ≥ 0.

One of `--input` or `--device` must be provided; throw `std::runtime_error` if neither or both are given.

### 4. Initialization

```cpp
OclContext ocl = create_context();

// WHY attachContext: T-API must share our cl_context so cv::UMat operations
// dispatch on the same device without creating a second context.
cv::ocl::attachContext(
    ocl.platform.getInfo<CL_PLATFORM_NAME>(),
    ocl.platform.get(),
    ocl.context.get(),
    ocl.device.get());
```

### 5. Face detection — `blaze.onnx` via OpenCV DNN T-API

```cpp
cv::dnn::Net net = cv::dnn::readNetFromONNX(face_model_path);
net.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
net.setPreferableTarget(cv::dnn::DNN_TARGET_OPENCL);
```

**Pre-processing (per frame):**

- `cv::resize` frame BGR → 128×128.
- `cv::dnn::blobFromImage` with `scalefactor=1/255.0`, `swapRB=true` (BGR→RGB), no mean subtraction.
- `net.setInput(blob)`.

**Inference:**

```cpp
auto t0 = std::chrono::steady_clock::now();
cv::Mat detections = net.forward();   // shape: [1, N, 16]
auto t1 = std::chrono::steady_clock::now();
```

**Box parsing:**

- `detections` shape is `[1, N, 16]`. Iterate rows; each row: `[score, x1, y1, x2, y2, ...]` (normalized — verify layout by printing row 0 on first detection and documenting with a `WHY` comment).
- Find row with `score >= conf_threshold`. Scale coords to frame size. Clamp to bounds.
- If no detection: skip blur, log `No face detected` at most once/second.

> **Note:** The exact column order of `selectedBoxes` must be verified at runtime. Print the raw float row on the first valid detection and document the confirmed layout in a `WHY` comment before merging.

### 6. Path A — T-API blur

```cpp
// Wrap frame in UMat — shares GPU memory via shared cl_context.
cv::UMat frame_umat;
frame_bgr.copyTo(frame_umat);  // uploads once per frame

cv::Rect roi(face.x, face.y, face.width, face.height);
cv::UMat roi_view = frame_umat(roi);

auto t0 = std::chrono::steady_clock::now();
cv::blur(roi_view, roi_view, cv::Size(blur_radius, blur_radius));
// WHY finish(): T-API enqueues asynchronously; must sync before timing stop.
CL_CHECK(ocl.queue.finish());
auto t1 = std::chrono::steady_clock::now();
double tapi_ms = duration_ms(t0, t1);

frame_umat.copyTo(frame_bgr);  // download result
```

### 7. Path B — `roi_blur.cl` with `global_work_offset`

Kernel signature:

```c
// roi_blur.cl
__kernel void roi_blur(__global uchar* img, int full_width, int full_height, int blur_radius)
```

- 2D, 1 byte/channel (BGR packed as 3-byte stride — or RGBA if simpler; document choice).
- `get_global_id(0)` = absolute x, `get_global_id(1)` = absolute y (offsets handled by NDRange).
- Box blur reading from absolute frame coords, writing in-place.
- Guard: `if (x >= (size_t)full_width || y >= (size_t)full_height) return;`
- `WHY` comment explaining that `global_work_offset` shifts work-item IDs to absolute frame coords, eliminating manual `x_local + roi_x` arithmetic inside the kernel.

Host dispatch:

```cpp
// WHY global_work_offset: work-item IDs start at (face.x, face.y) —
// no offset arithmetic needed inside the kernel body.
cl::NDRange offset(static_cast<size_t>(face.x), static_cast<size_t>(face.y));
cl::NDRange gws   (static_cast<size_t>(face.width), static_cast<size_t>(face.height));
cl::Event ev;
CL_CHECK(ocl.queue.enqueueNDRangeKernel(k_roi, offset, gws, cl::NullRange, nullptr, &ev));
CL_CHECK(ev.wait());
double kernel_ms = (ev.getProfilingInfo<CL_PROFILING_COMMAND_END>() -
                    ev.getProfilingInfo<CL_PROFILING_COMMAND_START>()) * 1e-6;
```

### 8. Per-frame console output

```text
Frame N:
  Detection:   X.XXX ms   (DNN_TARGET_OPENCL, std::chrono)
  Blur T-API:  X.XXX ms   (cv::blur on cv::UMat)
  Blur Kernel: X.XXX ms   (roi_blur.cl, cl::Event)
```

Frame 1: annotate `(JIT warm-up — exclude from averages)`.

---

## Definition of Done (DoD)

Standard items from `.claude/rules/00_master_specs.md` §8 apply.

- [ ] `cmake -B build && cmake --build build` in `A5_Privacy_Mode/` succeeds with zero errors and zero warnings.
- [ ] `./build/privacy_mode --help` lists all flags.
- [ ] `./build/privacy_mode --input assets/face.png` runs without error; output BMP shows blurred face region; per-frame timings printed for both paths.
- [ ] When no face is detected, frame passes through unmodified; `No face detected` logged (rate-limited).
- [ ] Both blur paths produce visually equivalent output — verified by inspection.
- [ ] `Blur T-API` and `Blur Kernel` timings printed per frame to 3 decimal places.
- [ ] BlazeFace output column layout documented with a `WHY` comment in `main.cpp`.
- [ ] Performance results documented in Execution Report (no hard gate — record and compare):
  - **Detection:** wall-clock ms. Note if `DNN_TARGET_OPENCL` fell back to CPU (expected on NVIDIA due to ocl4dnn probe failure — see design doc Known Issues). Record backend: GPU or CPU fallback.
  - **Blur T-API vs Blur Kernel:** both measured on the same face ROI from `assets/sample_1080p.bmp`. Report ratio. Expected: comparable within 2×; the result itself is the lesson.
  - **Soft target:** combined `Detection + Blur T-API` < 20 ms on Intel iGPU with working `DNN_TARGET_OPENCL`. Waived on NVIDIA (ocl4dnn CPU fallback) and any iGPU where ocl4dnn probe fails.
- [ ] `roi_blur.cl` copied to build directory by CMake post-build command.

---

## Execution Report

<!-- Filled by @coder after implementation. -->

- **Status:** PENDING
- **Session:** [YYYY-MM-DD]

### Validation

```text
[output here]
```

### Changed Files

| File | Change |
|------|--------|
| `02_Projects/A_Multimedia/A5_Privacy_Mode/main.cpp` | Created |
| `02_Projects/A_Multimedia/A5_Privacy_Mode/CMakeLists.txt` | Created |
| `02_Projects/A_Multimedia/A5_Privacy_Mode/kernels/roi_blur.cl` | Created |

### Remaining

- [ ] [Remaining item]
