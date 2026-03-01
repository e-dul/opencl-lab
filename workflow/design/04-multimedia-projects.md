# Module 4: Path A — Multimedia & AI

**Version:** 1.0
**Status:** Active — implementation not started
**Module Path:** `02_Projects/A_Multimedia/`

---

## Goal

Build a production-grade GPU video pipeline that processes 1080p at ≥ 30 FPS. The core engineering problem across every step is memory bandwidth: a naive `cv::Mat` copy to the GPU consumes a disproportionate fraction of the frame budget before a single pixel is processed. Each step in this path teaches a concrete technique to eliminate or amortize that cost, culminating in an AI-powered smart webcam.

## Non-goals

- OpenGL/Vulkan interop (covered in Path B)
- ROS 2 message transport optimization (covered in Path C)
- NVDEC/VAAPI hardware decoding (deferred to Add-on 4.6)
- SVM fine-grained coherency theory (covered in Toolbox: SVM)
- Model training or fine-tuning (inference integration only)
- Audio processing

---

## Roadmap / Status

- [ ] Phase 1: A1 — OpenCV Interop — Measure and eliminate `cv::Mat → GPU` copy overhead.
  - *Context*: Executive Summary §Path A item A.1; `Multimedia.md` §A1_OpenCV_Interop.
- [ ] Phase 2: A2 — YUV Pipeline — NV12 → RGBA and Y-channel extraction kernels.
  - *Context*: Executive Summary §Path A item A.2; `Multimedia.md` §A2_YUV_Pipeline.
- [ ] Phase 3: A3_1 — OpenCV DNN (T-API) — High-level inference, UMat stays on GPU.
  - *Context*: Executive Summary §Path A item A.3.1; `Multimedia.md` §A3_1_OpenCV_DNN.
- [ ] Phase 4: A3_2 — TFLite GPU Delegate — Explicit `clEnqueueMapBuffer` buffer handoff.
  - *Context*: Executive Summary §Path A item A.3.2; `Multimedia.md` §A3_2_TFLite_GPU.
- [ ] Phase 5: A4 — AI Smart Webcam (Flagship) — Full live pipeline: capture → AI → OpenCL blur → display.
  - *Context*: Executive Summary §Path A item A.4; `Multimedia.md` §A4_Smart_Webcam.
- [ ] Phase 6: A4 Challenge — Privacy Mode — ROI-scoped blur via `global_work_offset`.
- [ ] Phase 7: Module review and cleanup — remove distraction, focus on what matters.
  - Extract common utils
  - Clean code to focus on key objectives 

---

## Specifications

> **Inherits**: `.claude/rules/00_master_specs.md`

**Additional constraints for this path:**

- **OpenCV Version**: 4.5+ required. Detected via `find_package(OpenCV REQUIRED)`.
- **Image Formats for Intermediate Verification**: BMP or PNG only (no JPEG). NV12 raw files loaded as flat byte buffers.
- **Inference Backends**: OpenCV DNN (A3_1) and TensorFlow Lite GPU delegate (A3_2). These are mutually exclusive sub-projects.
- **CLI**: All binaries must expose at minimum `--input` (file path) and a GPU selection path via the `GPU` env var. Webcam binaries expose `--device` (integer index) and `--width`/`--height`.

---

## Architecture (high-level)

### Components

- **OpenCV Interop Layer** (`A1_OpenCV_Interop`): Benchmarks two transfer paths — `clEnqueueWriteBuffer` from a `cv::Mat` vs `UMat` zero-copy via `CL_MEM_USE_HOST_PTR`. Outputs timing to console; no visual kernel required (transfer benchmark only).
- **YUV Conversion Kernel** (`A2_YUV_Pipeline`): Two kernels — `nv12_to_rgba` and `extract_y_channel`. Reads a flat NV12 byte buffer with explicit stride/pitch handling. Outputs `output_rgba.bmp` and `output_y_channel.bmp`.
- **DNN T-API Bridge** (`A3_1_OpenCV_DNN`): Wraps OpenCV DNN with `DNN_TARGET_OPENCL`. Extracts the `cl_mem` handle from the output `cv::UMat` via `umat.handle(cv::ACCESS_READ)` and passes it directly to the blur kernel as a `cl::Buffer`.
- **TFLite GPU Bridge** (`A3_2_TFLite_GPU`): Uses `TfLiteGpuDelegateV2` with `TFLITE_GPU_EXPERIMENTAL_FLAGS_CL_COMMAND_QUEUE_IMPORT`. Input/output tensors are bound to OpenCL buffers via `clEnqueueMapBuffer`. Buffer must be created with `CL_MEM_ALLOC_HOST_PTR`.
- **Bokeh Kernel** (`A4_Smart_Webcam`): Full-frame conditional blur — `if (mask[id] == BACKGROUND)` applies Gaussian/box filter; foreground pixels pass through. Controlled by a binary segmentation mask from AI stage.
- **Privacy ROI Kernel** (A4 Challenge): Identical blur kernel launched over a sub-region using `global_work_offset` and a restricted `global_work_size` matching the detected face bounding box.

### Data Flow

#### A1 (Interop Benchmark)
1. Load image → `cv::Mat` (CPU memory).
2. Path 1 (Copy): `clEnqueueWriteBuffer` → `cl::Buffer`. Record `cl::Event` time.
3. Path 2 (Zero-Copy): `cv::UMat` → `cl::Buffer` via `CL_MEM_USE_HOST_PTR`. Record `cl::Event` time.
4. Print comparison table to console.

#### A2 (YUV Pipeline)
1. Load raw NV12 file → flat `cl::Buffer` (device).
2. `nv12_to_rgba` kernel → RGBA `cl::Buffer` → save `output_rgba.bmp`.
3. `extract_y_channel` kernel → Grayscale `cl::Buffer` → save `output_y_channel.bmp`.
4. Profile both kernels via `cl::Event`.

#### A3_1 (DNN T-API)
1. Load image → `cv::UMat` (stays on GPU).
2. `net.forward(mask_umat)` via `DNN_TARGET_OPENCL` → mask on GPU.
3. Extract `cl_mem` handle → construct `cl::Buffer` (no copy).
4. Run blur kernel with mask buffer as arg.
5. Save `output_mask.bmp` and `output_blurred.bmp`.

#### A3_2 (TFLite GPU)
1. Load image → `cl::Buffer` (`CL_MEM_ALLOC_HOST_PTR`).
2. `clEnqueueMapBuffer` → bind to TFLite input tensor.
3. Run TFLite inference on GPU delegate.
4. `clEnqueueMapBuffer` (output) → extract as `cl::Buffer`.
5. Run blur kernel. Save outputs.

#### A4 (Smart Webcam — Live Pipeline)
1. `cv::VideoCapture` frame → `cv::UMat`.
2. AI inference (A3_1 or A3_2 path, selected at build time) → mask `cl::Buffer`.
3. Bokeh kernel: full-frame conditional blur using mask.
4. Transfer result → `cv::Mat` → `cv::imshow`.
5. Per-frame profiling: capture / inference / kernel / display printed to console.

---

## Key Decisions (and Rationale)

1. **Two Inference Sub-paths (A3_1 / A3_2) as Separate Snapshot Directories**
   - **Why**: Educational contrast — T-API hides memory management (ease) vs explicit `cl_mem` wiring (control). Side-by-side snapshots let users diff the integration cost.

2. **NV12 Input for A2 via Raw Byte File, Not OpenCV Decode**
   - **Why**: `cv::imread` silently converts to BGR, hiding the raw memory layout. Loading a flat `.yuv` file forces the user to reason about stride, Y-plane size, and UV interleaving explicitly.

3. **Bokeh Kernel Uses Full-Frame Dispatch (Not Two-Pass Masked)**
   - **Why**: Demonstrates the thread divergence problem concretely. The `if (mask[id] == BACKGROUND)` conditional is intentionally naive; the Toolbox: Thread Divergence mini-challenge replaces it with `select()`.

4. **A4 Privacy Mode Uses `global_work_offset`, Not a Separate Cropped Buffer**
   - **Why**: Demonstrates ROI processing as a kernel launch configuration problem, not a memory copy problem. Avoids creating a teaching moment that encourages unnecessary copies.

5. **`CL_MEM_ALLOC_HOST_PTR` for TFLite Delegate Buffers (A3_2)**
   - **Why**: Required for `clEnqueueMapBuffer` portability across discrete GPU (PCIe) and iGPU (UMA). `CL_MEM_COPY_HOST_PTR` alone is not mappable on all drivers.

6. **Model: MediaPipe Selfie Segmentation (A4 Bokeh), YuNet (A4 Privacy)**
   - **Why**: Selfie segmentation outputs a per-pixel float mask directly usable as a `cl::Buffer` argument with no postprocessing. YuNet outputs a bounding box — teaching `global_work_offset` use requires a box, not a mask.

---

## Known Issues / Risks

- **OpenCV DNN OpenCL Fallback (A3_1)**: `DNN_TARGET_OPENCL` silently falls back to CPU if OpenCV was not built with OpenCL support. DoD for A3_1 must include a build-info verification step.
- **TFLite GPU Delegate x86 Availability (A3_2)**: The ARM-packaged `.so` does not work on x86. CMake must fetch or locate the correct delegate. This is a hard prerequisite; the task must document the exact FetchContent or find_library path.
- **UMat `cl_mem` Handle Stability (A3_1)**: The handle retrieved via `umat.handle(cv::ACCESS_READ)` is valid only while the `UMat` is alive and not modified. The blur kernel must complete before the `UMat` goes out of scope.
- **Webcam Default Resolution**: `cv::VideoCapture` defaults to 640×480 on many devices. CLI args `--width`/`--height` with `cv::CAP_PROP_FRAME_WIDTH/HEIGHT` must be set before the first frame read.
- **Thread Divergence in Bokeh Kernel**: The naive `if (mask[id] == BACKGROUND)` branch is a known inefficiency, intentionally left for the mini-challenge. It must not be pre-optimized in the base implementation.

---

## Performance Gates (Path Completion)

| Project | Metric | Target |
| :--- | :--- | :--- |
| A1 OpenCV Interop | Zero-copy vs copy speedup | Measurable reduction at 1080p (console-reported, `cl::Event`) |
| A2 YUV Pipeline | NV12 → RGBA kernel time | < 2 ms @ 1920×1080 |
| A3_1 / A3_2 | Inference + blur time | < 15 ms @ 1080p per frame |
| A4 Bokeh Mode | Total frame time | < 33 ms @ 1080p (≥ 30 FPS) |
| A4 Privacy Mode | Total frame time | < 20 ms @ 1080p (single face) |


---

## Specifications & Standards

- **Directory Structure**:
  ```
  02_Projects/A_Multimedia/
  ├── A1_OpenCV_Interop/
  │   ├── CMakeLists.txt
  │   ├── main.cpp
  │   └── kernels/          (passthrough or no kernel — transfer benchmark only)
  ├── A2_YUV_Pipeline/
  │   ├── CMakeLists.txt
  │   ├── main.cpp
  │   └── kernels/nv12_to_rgba.cl
  │   └── kernels/extract_y.cl
  ├── A3_1_OpenCV_DNN/
  │   ├── CMakeLists.txt
  │   ├── main.cpp
  │   └── kernels/bokeh_blur.cl
  ├── A3_2_TFLite_GPU/
  │   ├── CMakeLists.txt
  │   ├── main.cpp
  │   └── kernels/bokeh_blur.cl
  └── A4_Smart_Webcam/
      ├── CMakeLists.txt
      ├── main.cpp
      └── kernels/bokeh_blur.cl
      └── kernels/roi_blur.cl
  ```
- **Verification Standard**:
  - A1: Console timing table. No BMP required.
  - A2: `output_rgba.bmp` (correct colors) + `output_y_channel.bmp` (grayscale). Green-pink output = failure.
  - A3_1 / A3_2: `output_mask.bmp` + `output_blurred.bmp`. Console prints inference and blur times separately.
  - A4: Live preview window with per-frame timing breakdown printed to console.
- **Tooling** (module-specific additions to master_specs):
  - `find_package(OpenCV REQUIRED)` — OpenCV 4.5+.

---

## Prerequisites

- Module 1 completed (`01_Host_API/`): `cl.hpp` usage, `cl::Event` profiling, `CL_CHECK` error handling.
- OpenCV 4.5+ installed: `sudo apt install libopencv-dev`.
- For A3_2: TFLite GPU delegate library available (pre-built `.so` or built from source with `-DTFLITE_ENABLE_GPU=ON`).
- `assets/sample.bmp`, `assets/sample_nv12.yuv`, `assets/person.jpg`, `assets/selfie_segmentation.onnx`, `assets/selfie_segmentation.tflite` present in repository root.

See [main README](../../README.md) for base requirements (OpenCL 1.2+, CMake 3.18+, Docker setup).
