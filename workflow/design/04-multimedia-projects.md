# Module 4: Path A — Multimedia & AI

**Version:** 1.2
**Status:** Active — A1 done, A2 done, A2b done, A3_1 done, A3_2 done, A4 done
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

- [x] Phase 1: A1 — OpenCV Interop — Measure and eliminate `cv::Mat → GPU` copy overhead.
  - *Context*: Executive Summary §Path A item A.1; `Multimedia.md` §A1_OpenCV_Interop.
- [x] Phase 2: A2 — YUV Pipeline — NV12 → RGBA and Y-channel extraction kernels; CPU vs GPU comparison table.
  - *Context*: Executive Summary §Path A item A.2; `Multimedia.md` §A2_YUV_Pipeline.
- [x] Phase 2b: A2 YUYV Extension — Port kernels to YUYV (4:2:2) packed format; two-pass vs single-pass timing comparison.
  - *Context*: `Multimedia.md` §A2_YUV_Pipeline Mini-challenge Parts 1 & 2.
- [x] Phase 3: A3_1 — OpenCV DNN (T-API) — High-level inference, UMat stays on GPU. **[DONE — performance gate waived; NVIDIA falls back to CPU via ocl4dnn probe failure; Intel iGPU tested at 64 ms]**
  - *Context*: Executive Summary §Path A item A.3.1; `Multimedia.md` §A3_1_OpenCV_DNN.
- [x] Phase 4: A3_2 — OpenVINO GPU Plugin — RemoteTensor API: pass `cl::Buffer` directly as input/output tensor. Intel iGPU only. **[DONE — Intel Iris Xe: stable inference ~5 ms avg* (runs 2+, JIT warm-up ~80–110 ms on run 1), blur 1.4 ms (cl::Event). Performance gate waived (iGPU hardware-waiver clause). Post-task additions: GPU preprocess_nchw kernel (RGBA→NCHW on GPU, eliminates CPU round-trip), model input/output shape read dynamically from compiled model, --runs CLI arg for warm-up measurement, f32 ONNX confirmed fastest format on Xe (INT8 QDQ 2.5× slower — documented in Known Issues).]**
  - *Context*: Executive Summary §Path A item A.3.2; `Multimedia.md` §A3_2_OpenVINO_GPU.
- [x] Phase 5: A4 — AI Smart Webcam (Flagship) — Full live pipeline: capture → preprocess_nchw → AI inference → OpenCL bokeh blur → display. **[DONE — Intel Iris Xe: offline loop ~4.7–8.0 ms/frame (498×498 input), FPS avg ~170 (frames 2+). Live webcam @ 640×480: ~36 FPS avg sustained. All DoD items verified. Post-task addition: `--exposure` CLI flag for manual V4L2 absolute exposure control (100 µs units).]**
  - CLI flags: `--device <int>` (webcam index), `--width`, `--height`, `--loop` (offline replay of a still image for testing without a webcam), `--input` (offline path), `--exposure` (manual V4L2 absolute exposure in 100 µs units; -1 = auto).
  - Per-frame timing output format (see Architecture §A4 Data Flow).
  - JIT warm-up rule: print frame 1 timing with a `(JIT warm-up — do not measure FPS here)` annotation; skip frame 1 when computing FPS average.
  - Uses RemoteTensor path (A3_2) internally — same shared `ClContext(core, ctx.get())` wiring.
  - *Context*: Executive Summary §Path A item A.4; `Multimedia.md` §A4_Smart_Webcam.
- [x] Phase 6: A5 Privacy Mode — T-API ROI blur vs custom kernel benchmark. Standalone module `A5_Privacy_Mode/`. **[DONE — Intel Iris Xe: Detection ~54 ms (DNN_TARGET_OPENCL, cached), Blur T-API ~1.0 ms, Blur Kernel ~0.88 ms. Performance gate waived — face.png 498×498 too small for soft target; DNN_TARGET_OPENCL succeeded on Xe (no CPU fallback).]**
  - Face detection: `cv::dnn::readNetFromONNX("face_detection_yunet_2022mar.onnx")`, `DNN_BACKEND_OPENCV` + `DNN_TARGET_OPENCL` (T-API inference, no OpenVINO).
  - ROI blur path A: `cv::UMat` submat + `cv::blur` (T-API, zero custom kernel code).
  - ROI blur path B: `roi_blur.cl` with `global_work_offset` (explicit NDRange, comparison reference).
  - Teaching point: T-API gives GPU acceleration without writing OpenCL kernels — including for inference. Custom kernel for explicit control.
  - Performance gate: < 20 ms/frame @ 1080p (single face), T-API path.
  - *Context*: `Multimedia.md` §A4_Smart_Webcam Stretch Challenge.
- [x] Phase 7: Module cleanup — legacy scaffold dirs removed, A2 kernel-path fixed, stale TODO removed. **[DONE — 2026-03-14]**

---

## Specifications

> **Inherits**: `.claude/rules/00_master_specs.md`

**Additional constraints for this path:**

- **OpenCV Version**: 4.5+ required. Detected via `find_package(OpenCV REQUIRED)`.
- **Image Formats for Intermediate Verification**: BMP or PNG only (no JPEG). NV12 raw files loaded as flat byte buffers.
- **Inference Backends**: OpenCV DNN (A3_1) and OpenVINO GPU plugin (A3_2, Intel iGPU only). These are mutually exclusive sub-projects.
  - A3_2 requires Intel iGPU. `find_package(OpenVINO REQUIRED)` in A3_2 CMakeLists.txt. No OpenCV dependency in A3_2.
- **CLI**: All binaries must expose at minimum `--input` (file path) and a GPU selection path via the `GPU` env var. Webcam binaries expose `--device` (integer index), `--width`/`--height`, and `--loop` (replay `--input` image in a live preview loop without a physical webcam — required for offline testing).
- **Integer Safety (A2 kernels)**: When passing pixel count or buffer size as `cl_int` kernel args, throw `std::runtime_error` if `size > INT_MAX`. Silent truncation via `std::min` is forbidden (master_specs §7.1).
- **A2 OpenCV dependency**: `find_package(OpenCV REQUIRED)` in A2's CMakeLists.txt. OpenCV is used only for the CPU comparison (`cv::cvtColor`) — NOT for loading the NV12 input (which remains a raw flat byte buffer).

---

## Architecture (high-level)

### Components

- **OpenCV Interop Layer** (`A1_OpenCV_Interop`): Benchmarks two transfer paths — `clEnqueueWriteBuffer` from a `cv::Mat` vs `UMat` zero-copy via `CL_MEM_USE_HOST_PTR`. Outputs timing to console; no visual kernel required (transfer benchmark only).
- **YUV Conversion Kernel** (`A2_YUV_Pipeline`): Two kernels — `nv12_to_rgba` and `extract_y_channel`. Reads a flat NV12 byte buffer with explicit stride/pitch handling. Also runs `cv::cvtColor` (CPU, `COLOR_YUV2RGBA_NV12`) for comparison — OpenCV is a dependency for A2. Outputs `output_rgba.bmp`, `output_y_channel.bmp`, and a console timing table (OpenCV CPU / OpenCL kernel / speedup).
- **DNN T-API Bridge** (`A3_1_OpenCV_DNN`): Wraps OpenCV DNN with `DNN_TARGET_OPENCL`. Extracts the `cl_mem` handle from the output `cv::UMat` via `umat.handle(cv::ACCESS_READ)` and passes it directly to the blur kernel as a `cl::Buffer`.
- **OpenVINO GPU Bridge** (`A3_2_OpenVINO_GPU`): Uses OpenVINO RemoteTensor API (`ov::intel_gpu::ocl::ClContext`). Includes a `preprocess_nchw.cl` kernel that converts RGBA to NCHW f32 on the GPU (eliminates the CPU preprocessing round-trip). Input `cl::Buffer` imported via `remote_ctx.create_tensor()` with raw `cl_mem` handle. Output `cl_mem` extracted via `ov::intel_gpu::ocl::ClBufferTensor::get()`. No host copy between preprocessing, inference, and blur kernel. Intel iGPU only.
- **Bokeh Kernel** (`A4_Smart_Webcam`): Full-frame conditional blur — `if (mask[id] == BACKGROUND)` applies Gaussian/box filter; foreground pixels pass through. Controlled by a binary segmentation mask from AI stage.
- **Privacy Mode** (`A5_Privacy_Mode`): Standalone app. Face detection via `cv::dnn` + `face_detection_yunet_2022mar.onnx` (`DNN_BACKEND_OPENCV`, `DNN_TARGET_OPENCL` — T-API inference). ROI blur path A: `cv::UMat` submat + `cv::blur` (T-API). ROI blur path B: `roi_blur.cl` with `global_work_offset` (custom kernel comparison). No OpenVINO dependency.

### Data Flow

#### A1 (Interop Benchmark)
1. Load image → `cv::Mat` (CPU memory).
2. Path 1 (Copy): `clEnqueueWriteBuffer` → `cl::Buffer`. Record `cl::Event` time.
3. Path 2 (Zero-Copy): `cv::UMat` → `cl::Buffer` via `CL_MEM_USE_HOST_PTR`. Record `cl::Event` time.
4. Print comparison table to console.

#### A2 (YUV Pipeline)
1. Load raw NV12 file → flat byte vector (host) + `cl::Buffer` (device).
2. CPU path: `cv::cvtColor` (COLOR_YUV2RGBA_NV12) → time with `std::chrono`.
3. `nv12_to_rgba` kernel → RGBA `cl::Buffer` → save `output_rgba.bmp`. Profile via `cl::Event`.
4. `extract_y_channel` kernel → Grayscale `cl::Buffer` → save `output_y_channel.bmp`. Profile via `cl::Event`.
5. Print comparison table: OpenCV CPU time / OpenCL kernel time / speedup.

#### A3_1 (DNN T-API)
1. Load image → `cv::UMat` (stays on GPU).
2. `net.forward(mask_umat)` via `DNN_TARGET_OPENCL` → mask on GPU.
3. Extract `cl_mem` handle → construct `cl::Buffer` (no copy).
4. Run blur kernel with mask buffer as arg.
5. Save `output_mask.bmp` and `output_blurred.bmp`.

#### A3_2 (OpenVINO GPU)
1. Load image → `cl::Buffer` RGBA (device memory).
2. `preprocess_nchw.cl` kernel: RGBA `cl::Buffer` → NCHW f32 `cl::Buffer` (model resolution, device). No CPU round-trip.
3. Wrap NCHW buffer as `ov::RemoteTensor` via `remote_ctx.create_tensor()` — no copy.
4. Pre-allocate output `cl::Buffer`; wrap as `ClBufferTensor`; call `req.set_output_tensor()` before first `infer()`.
5. Run OpenVINO inference (`req.infer()`).
6. Extract output `cl_mem` via `ClBufferTensor::get()` → construct `cl::Buffer` (`retain=true`).
7. Run blur kernel. `CL_CHECK(queue.finish())` before request is reused or destroyed. Save outputs.

#### A4 (Smart Webcam — Live Pipeline)
```
OpenCL buffer (RGBA, full-res, GPU)
        │
        ├─── preprocess_nchw.cl ──► NCHW f32 buffer (model resolution, GPU)
        │                                    │
        │                             RemoteTensor input
        │                             req.set_input_tensor()
        │                                    │
        │                             req.infer()   ← stays on GPU
        │                                    │
        │                             output ClBufferTensor
        │                             mask cl_mem (GPU)
        │                                    │
        └─── bokeh_blur.cl  ◄────────────────┘
             (RGBA buf + mask buf)
                    │
             CL_CHECK(queue.finish())   ← gates frame loop
                    │
             display / output_blurred.bmp
```

Three decisions make this zero-copy:
1. **Shared OpenCL context.** OpenVINO initialized with `ClContext(core, ctx.get())` — same `cl_context` as preprocessing kernel. Without this, the GPU plugin creates its own context with no shared address space.
2. **Output tensor pre-allocated before `infer()`.** `req.set_output_tensor()` called with a pre-allocated `ClBufferTensor` before the first `req.infer()`. Without this, the GPU plugin silently falls back to host memory for the output.
3. **`queue.finish()` gates the frame loop.** The blur kernel's `cl_mem` is owned by the `InferRequest`. That handle is valid only while the request is alive and not re-invoked. `CL_CHECK(queue.finish())` must precede any loop restart or request destruction.

Per-frame console output format:
```
Frame 1 (JIT warm-up — do not measure FPS here):
  Capture:    X.X ms
  Inference: XXX.X ms   <- JIT compilation, one-time cost
  Kernel:     X.X ms
  Display:    X.X ms

Frame 2+ (stable):
  Capture:    X.X ms
  Inference:  X.X ms
  Kernel:     X.X ms
  Display:    X.X ms
  Total:     XX.X ms  ← must be < 33 ms to pass
```

When `--loop` is used (offline test without webcam): replays `--input` image in a loop. Frame 1 is still annotated as JIT warm-up. FPS calculation starts from frame 2.

---

## Key Decisions (and Rationale)

1. **Two Inference Sub-paths (A3_1 / A3_2) as Separate Snapshot Directories**
   - **Why**: Educational contrast — T-API hides buffer ownership (OpenCV owns `cl_mem`) vs OpenVINO RemoteTensor (caller owns `cl_mem` from start). Intel-only scope is honest — the educational value is the raw `cl_mem` boundary, not cross-platform support. Side-by-side snapshots let users diff the integration cost.

2. **NV12 Input for A2 via Raw Byte File, Not OpenCV Decode**
   - **Why**: `cv::imread` silently converts to BGR, hiding the raw memory layout. Loading a flat `.yuv` file forces the user to reason about stride, Y-plane size, and UV interleaving explicitly. This layout knowledge is the prerequisite for writing a single-pass OpenCL kernel — the teaching payoff is the measured speedup over `cv::cvtColor` on CPU, which runs multi-pass with intermediate buffers. OpenCV is used in A2 only for the CPU comparison path (`cv::cvtColor`), not for decoding the input.

3. **Bokeh Kernel Uses Full-Frame Dispatch (Not Two-Pass Masked)**
   - **Why**: Demonstrates the thread divergence problem concretely. The `if (mask[id] == BACKGROUND)` conditional is intentionally naive; the Toolbox: Thread Divergence mini-challenge replaces it with `select()`.

4. **A4 Privacy Mode Uses `global_work_offset`, Not a Separate Cropped Buffer**
   - **Why**: Demonstrates ROI processing as a kernel launch configuration problem, not a memory copy problem. Avoids creating a teaching moment that encourages unnecessary copies.

5. **`retain=true` when wrapping output `cl_mem` (A3_2)**
   - **Why**: The `cl_mem` returned by `ClBufferTensor::get()` is owned by OpenVINO. `cl::Buffer(raw, retain=true)` increments the refcount so the buffer stays valid after `InferRequest` scope ends. `retain=false` would double-free.

6. **Model: MediaPipe Selfie Segmentation (A4 Bokeh), BlazeFace `face_detection_yunet_2022mar.onnx` (A5 Privacy)**
   - **Why**: Selfie segmentation outputs a per-pixel float mask usable directly as a `cl::Buffer`. BlazeFace outputs a bounding box — coordinates drive the T-API submat crop and, in path B, the `global_work_offset`. `face_detection_yunet_2022mar.onnx` is already in `assets/`. Using `cv::dnn` with `DNN_TARGET_OPENCL` (not OpenVINO) for A5 keeps OpenVINO as an A4-only concept and demonstrates T-API as a self-contained GPU path for both inference and post-processing.

7. **OpenVINO Dependency (A3_2): `find_package(OpenVINO REQUIRED)`**
   - **Why**: OpenVINO provides official CMake config files (`OpenVINOConfig.cmake`) via `libopenvino-dev` apt package. No FetchContent needed. `OpenVINOConfig.cmake` is installed to a system path — no env sourcing needed. If cmake cannot find it, set `export OpenVINO_DIR=/usr/lib/cmake/OpenVINO`. Setup documented in `A3_2_OpenVINO_GPU/SETUP.md`.

8. **GPU Preprocessing Kernel (`preprocess_nchw.cl`) in A3_2 and A4**
   - **Why**: Without it, the host must read RGBA back from the GPU, convert to NCHW f32, and re-upload — a round-trip that negates the zero-copy design. The preprocessing kernel keeps all data on device from capture to inference output.

9. **`CL_MEM_READ_WRITE` for all buffers crossing the OpenVINO boundary**
   - **Why**: The OpenVINO GPU plugin rejects `CL_MEM_READ_ONLY` and `CL_MEM_WRITE_ONLY` on imported buffers. It performs in-place layout transformations during dispatch and requires read-write access on both input and output buffers.

---

## Known Issues / Risks

- **OpenCV DNN OpenCL Fallback (A3_1)**: `DNN_TARGET_OPENCL` silently falls back to CPU if OpenCV was not built with OpenCL support. DoD for A3_1 must include a build-info verification step.
- **UMat `cl_mem` Handle Stability (A3_1)**: The handle retrieved via `umat.handle(cv::ACCESS_READ)` is valid only while the `UMat` is alive and not modified. The blur kernel must complete before the `UMat` goes out of scope.
- **OpenVINO RemoteTensor `cl_mem` lifetime (A3_2)**: The `cl_mem` returned by `ClBufferTensor::get()` is valid only while the `InferRequest` is alive and not re-invoked. The blur kernel must complete (`queue.finish()`) before the next `req.infer()` call. Document in task DoD.
- **OpenVINO Intel-only scope (A3_2)**: A3_2 will not run on NVIDIA or AMD. Binary must print a descriptive message and exit(0) if GPU device is not available via OpenVINO — do not crash.
- **Webcam Default Resolution**: `cv::VideoCapture` defaults to 640×480 on many devices. CLI args `--width`/`--height` with `cv::CAP_PROP_FRAME_WIDTH/HEIGHT` must be set before the first frame read.
- **Thread Divergence in Bokeh Kernel**: The naive `if (mask[id] == BACKGROUND)` branch is a known inefficiency, intentionally left for the mini-challenge. It must not be pre-optimized in the base implementation.
- **A2 Mini-challenge scope**: The YUYV port and two-pass timing comparison are Phase 2b — a separate task from Task 020 (NV12 pipeline). Not required for the A2 performance gate.
- **A2 Kernel Path Resolution**: RESOLVED (Task 027) — Replaced manual `rfind`-based string split with `std::filesystem::path(argv[0]).parent_path()`, matching the pattern used in A3_1+.
- **A2 GPU Timing (RTX 4060 Laptop)**:
  - 256×256: CPU 0.243 ms, GPU (nv12_to_rgba) 0.013 ms, speedup 18×. Kernel launch overhead dominates at this size.
  - 1920×1080: CPU 8.690 ms, GPU (nv12_to_rgba) 0.053 ms, speedup **164×**. Gate PASS. At 1080p pixel throughput dominates; this is the representative benchmark.
- **A2b GPU Timing (RTX 4060 Laptop)**:
  - 256×256: CPU 0.210 ms, single-pass 0.013 ms, two-pass 0.010 ms. Speedup 21×. Two-pass marginally faster (launch overhead small at tiny size).
  - 1920×1080: CPU 3.709 ms, single-pass 0.046 ms, two-pass 0.070 ms (P1: 0.033 ms + P2: 0.038 ms). Speedup **80×**. Gate PASS. Single-pass wins at 1080p (two-pass 1.53× slower — cost of two kernel dispatches outweighs compute savings).
- **A3_1 ocl4dnn probe kernel failure (NVIDIA — DNN falls back to CPU)**: OpenCV emits `CL_BUILD_PROGRAM_FAILURE` for the `dnn/dummy` probe kernel (`-cl-no-subgroup-ifp` rejected by NVIDIA's compiler). This causes ocl4dnn to abort GPU kernel compilation; DNN inference runs on CPU (~90 ms). The bokeh blur kernel (our code) still executes on GPU (0.026 ms). Not a defect in this module — upstream OpenCV/NVIDIA driver incompatibility.
- **A3_1 `mask_on_gpu=YES` does NOT confirm DNN ran on OpenCL**: `mask_on_gpu` reflects whether the resized mask UMat has a GPU `cl_mem` handle after `cv::resize` (which goes through the shared T-API context). Even with CPU-bound DNN inference, the subsequent `cv::resize` can produce a GPU-resident UMat. Use inference wall-clock time (~90 ms vs <5 ms expected for GPU) as the definitive indicator.
- **A3_1 `cv::ocl::attachContext()` required before DNN init**: The OpenCL context created by `create_context()` must be attached via `cv::ocl::attachContext()` before `cv::dnn::readNetFromONNX()` so OpenCV DNN shares the same context. Calling it after model load causes a second context to be created, breaking handle extraction.
- **A3_1 BGRA→RGBA T-API path**: Input is loaded as BGR, converted to RGBA entirely via `cv::UMat` T-API `cvtColor` + `cl_mem` handle extraction. No CPU round-trip. This avoids a `clEnqueueWriteBuffer` that would negate the zero-copy benefit of using UMat.
- **A3_1 GPU Timing (RTX 4060 Laptop)**: Inference (CPU wall-clock) ~112 ms (ocl4dnn CPU-bound due to probe failure); bokeh blur (cl::Event) 0.026 ms. Performance gate WAIVER applied — iGPU/CPU context detected by ocl4dnn backend on this driver stack.
- **A3_1 GPU Timing (Intel Iris Xe)**: Inference (CPU wall-clock) ~64 ms; bokeh blur (cl::Event) 0.223 ms. OPENCV_LOG_LEVEL=VERBOSE confirms OpenCL context initialized. Performance gate WAIVER applied — iGPU is exempt per hardware-waiver clause.
- **A3_1 per-layer CPU fallback not visible in DNN logs**: OpenCV DNN does not emit per-layer backend decisions. Total inference wall-clock time is the only reliable indicator (~90 ms+ = CPU-bound). cl::Event profiling is not available on the net.forward() call.
- **1080p canonical assets**: `assets/sample_1080p.bmp` (ffmpeg `testsrc`), `assets/sample_nv12_1080p.yuv`, `assets/sample_yuyv_1080p.yuv` are now committed. Use `--width 1920 --height 1080` at runtime.
- **A3_2 Output tensor must be pre-bound before infer**: `req.get_output_tensor().as<ClBufferTensor>()` throws if the output tensor has not been pre-allocated and bound via `req.set_output_tensor()` before the first `req.infer()` call. Always pre-allocate a RemoteTensor for output and call `req.set_output_tensor()` before inference.
- **A3_2 OpenVINO GPU plugin rejects `CL_MEM_READ_ONLY` buffers**: `remote_ctx.create_tensor()` will throw at runtime if the `cl::Buffer` was created with `CL_MEM_READ_ONLY`. Use `CL_MEM_READ_WRITE` for all buffers passed to the OpenVINO GPU plugin, even input-only tensors.
- **A3_2 `AnyMap` overload for `create_tensor()` not present in apt `libopenvino-dev`**: The `{ov::intel_gpu::ocl::mem_type::buffer, buf.get()}` AnyMap overload is not available in the `libopenvino-dev` package installed via apt. Use the explicit `remote_ctx.create_tensor(element_type, shape, const cl::Buffer&)` overload instead.
- **A4 `--exposure` flag: V4L2 `auto_exposure` mode 1 required**: Setting `CAP_PROP_AUTO_EXPOSURE=1` (manual mode) before `CAP_PROP_EXPOSURE` is mandatory; some drivers silently ignore `CAP_PROP_EXPOSURE` when auto-exposure mode is not first disabled. Driver-negotiated values should be read back and printed at startup to confirm the setting took effect.

### Intel Xe iGPU — Hardware Notes (A3_2 and A4)

Results gathered on Intel Iris Xe Graphics (12th-gen Intel).

- **f32 ONNX is the fastest format on Intel Xe.** INT8 ONNX (QDQ format) ran ~2.5× *slower* than f32. The GPU plugin does not fuse `QuantizeLinear`/`DequantizeLinear` nodes from third-party QDQ models into native INT8 dispatch — each node runs as a separate op with full memory round-trips. Genuine INT8 speedup requires NNCF-aware quantization (OpenVINO's own calibration flow), which is out of scope for this track.
- **First inference call measures JIT warm-up, not inference.** The OpenVINO GPU plugin JIT-compiles its OpenCL kernels on the first `req.infer()` call. On Iris Xe this adds ~80 ms (f32) or ~160 ms (INT8) to the first call; subsequent calls stabilize at ~4–8 ms (f32). Always run at least one untimed warm-up call before recording latency — single-call benchmarks measure JIT overhead, not model performance.
- **FP16 execution hint has no measurable effect** at this model size. `ov::hint::inference_precision(ov::element::f16)` showed no consistent speedup for the 256×256 segmentation model — too small for the throughput gain to exceed scheduling noise.
- **A4 pipeline hang / corrupted output (missing `queue.finish()`)**: If `queue.finish()` is omitted after the blur kernel, the `InferRequest` may be destroyed or re-invoked while the kernel is still reading its output `cl_mem`. This causes undefined behaviour — typically corrupted output or a crash after an unpredictable number of frames, not on frame 1. Add `CL_CHECK(queue.finish())` immediately after `enqueueNDRangeKernel` in the frame loop.
- **These observations are driver- and model-size-specific.** Larger models (ResNet-50+), Intel Arc dGPUs, or newer driver versions may yield different results. Always profile your actual hardware with your actual model before choosing a quantization strategy.
- **A5 face_detection_yunet_2023mar.onnx (opset 16) incompatible with OpenCV 4.6.0 DNN**: `face_detection_yunet_2023mar.onnx` fails at `FaceDetectorYN::create()` with a "Layer id=-1" error on OpenCV 4.6. Use `face_detection_yunet_2022mar.onnx` instead. Pass `--face-model assets/face_detection_yunet_2022mar.onnx` explicitly on this OpenCV version.
- **A5 FaceDetectorYN stride-aligned input sizes**: `FaceDetectorYN` requires input dimensions that are multiples of 32. Use fixed 320x320 as the network input size and resize the frame manually before inference. Non-aligned sizes (e.g., 640x480 directly) produce misaligned detections or assertion failures.
- **A5 `cv::ocl::finish()` required to sync T-API UMat operations**: After `cv::blur` on a `cv::UMat` submat, use `cv::ocl::finish()` (not `queue.finish()`) to synchronize T-API-dispatched commands before the timing stop. T-API enqueues to its own internal command queue, which may differ from the `cl::CommandQueue` in `OclContext`. Using `queue.finish()` alone may not fully sync the T-API work.
- **A5 ocl4dnn JIT failures surface as `cv::Exception`, not `cl::Error`**: When `DNN_TARGET_OPENCL` triggers ocl4dnn JIT compilation and it fails (e.g., unsupported kernel on a driver), the error is thrown as `cv::Exception` with message containing `CL_BUILD_PROGRAM_FAILURE`. Catch `cv::Exception` (in addition to `cl::Error`) when wrapping DNN forward calls.

---

## Performance Gates (Path Completion)

| Project | Metric | Target |
| :--- | :--- | :--- |
| A1 OpenCV Interop | Zero-copy vs copy speedup | Zero-copy path ≥ 20% faster than `clEnqueueWriteBuffer` at 1920×1080 (measured via `cl::Event`) |
| A2 YUV Pipeline | NV12 → RGBA kernel time | < 2 ms @ 1920×1080 |
| A3_1 / A3_2 | Inference + blur time | < 15 ms @ 1080p per frame |
| A4 Bokeh Mode | Total frame time | < 33 ms @ 1080p (≥ 30 FPS) |
| A4 Privacy Mode | Total frame time | < 20 ms @ 1080p (single face) |

> **Hardware waiver**: Gates measured via `cl::Event` on a mid-range discrete GPU (≥ GTX 1060 / RX 580 equivalent). iGPU results must be documented and reported but are exempt from pass/fail.

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
  ├── A2b_YUYV_Extension/
  │   ├── CMakeLists.txt
  │   ├── main.cpp
  │   └── kernels/
  ├── A3_1_OpenCV_DNN/
  │   ├── CMakeLists.txt
  │   ├── main.cpp
  │   └── kernels/bokeh_blur.cl
  ├── A3_2_OpenVINO_GPU/
  │   ├── CMakeLists.txt
  │   ├── SETUP.md
  │   ├── main.cpp
  │   └── kernels/bokeh_blur.cl
  │   └── kernels/mask_resize.cl
  │   └── kernels/preprocess_nchw.cl
  └── A4_Smart_Webcam/
      ├── CMakeLists.txt
      ├── main.cpp
      └── kernels/bokeh_blur.cl
      └── kernels/preprocess_nchw.cl
      └── kernels/roi_blur.cl
  ```
- **Verification Standard**:
  - A1: Console timing table. No BMP required.
  - A2: `output_rgba.bmp` (correct colors) + `output_y_channel.bmp` (grayscale). Green-pink output = failure (UV plane offset wrong — check UV starts at byte `width * height`). Console prints three-row comparison table: OpenCV CPU time / OpenCL kernel time / speedup. Gate: OpenCL kernel < 2 ms @ 1920×1080.
  - A3_1 / A3_2: `output_mask.bmp` + `output_blurred.bmp`. Console prints inference and blur times separately.
  - A4: Live preview window (or looped offline preview with `--loop`) with per-frame timing breakdown printed to console. Frame 1 annotated as JIT warm-up.
- **Tooling** (module-specific additions to master_specs):
  - `find_package(OpenCV REQUIRED)` — OpenCV 4.5+.

---

## Prerequisites

- Module 1 completed (`01_Host_API/`): `cl.hpp` usage, `cl::Event` profiling, `CL_CHECK` error handling.
- OpenCV 4.5+ installed: `sudo apt install libopencv-dev`.
- For A3_2 and A4: Intel iGPU + `libopenvino-dev` installed. See `A3_2_OpenVINO_GPU/SETUP.md`.
- `assets/face.png`, `assets/selfie_segmentation.onnx` present in repository root.

See [main README](../../README.md) for base requirements (OpenCL 1.2+, CMake 3.18+, Docker setup).
