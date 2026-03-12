# Path A: Multimedia & AI

Process real video with a GPU pipeline that ships in production. You start with a single annoying copy that eats half your frame budget, eliminate it, then chain a neural network directly into your OpenCL kernel to build a smart webcam.

## Prerequisites
See [main README](../../README.md) for base requirements (OpenCL, CMake, Docker setup).

**Additional:**
- OpenCV 4.5+: `sudo apt install libopencv-dev` — verify: `pkg-config --modversion opencv4`
- A3_2 only: Intel iGPU required. See [A3_2_OpenVINO_GPU/SETUP.md](A3_2_OpenVINO_GPU/SETUP.md).
- Assets in repository root: `assets/face.png`, `assets/selfie_segmentation.onnx` — included in the repository. See [assets/assets.md](../../assets/assets.md) for the full catalogue.

## Contents
```
A1_OpenCV_Interop/      Measure and eliminate the cv::Mat → GPU copy overhead
A2_YUV_Pipeline/        NV12 layout → single-pass GPU conversion (faster than CPU cvtColor)
A3_1_OpenCV_DNN/        Inference via OpenCV DNN T-API (UMat stays on GPU)
A3_2_OpenVINO_GPU/      Inference via OpenVINO GPU plugin (zero-copy cl_mem into RemoteTensor)
A4_Smart_Webcam/        Flagship project: person segmentation + real-time Bokeh blur
```

---

## A1_OpenCV_Interop — Kill the Copy

**Goal**: Measure how much time the naive `cv::Mat` copy costs, then replace it with `UMat` zero-copy interop.

### Build & run
```bash
cd A1_OpenCV_Interop
cmake -B build
cmake --build build
./build/opencv_interop_demo --input ../../../assets/sample.bmp
# Pin GPU if needed: GPU=NVIDIA ./build/opencv_interop_demo --input ...
```

### Verify
Console prints two transfer times. Expected output varies by hardware:

**Discrete GPU:**
```
[COPY]     cv::Mat → clEnqueueWriteBuffer:  8.4 ms
[ZERO-COPY] UMat → cl::Buffer (map):        0.1 ms
```

**iGPU (Intel/ARM Mali — UMA):**
```
[COPY]     cv::Mat → clEnqueueWriteBuffer:  0.2 ms
[ZERO-COPY] UMat → cl::Buffer (map):        0.1 ms
```
On iGPU, the GPU buffer shares physical memory with system RAM — both paths converge to near zero. This is expected, not a bug. The technique matters on discrete GPU or large buffers that exceed cache.

The technique behind this demo: [Toolbox: Zero-Copy](../../99_Toolbox/ZeroCopy/ZeroCopy.md). On UMA hardware (Intel iGPU, ARM Mali), `CL_MEM_USE_HOST_PTR` makes the buffer physically shared — see [Toolbox: SVM](../../99_Toolbox/SVM/SVM.md) for the hardware explanation of why.

### Mini-challenge
Change the input image to `4096×4096` and re-run. At what resolution does the copy time exceed 1 ms? 5 ms? This is your "copy budget" for later work.

---

## A2_YUV_Pipeline — One-Pass NV12 to RGBA on the GPU

**Goal**: Write a single-pass OpenCL kernel that converts a raw NV12 camera frame to RGBA — faster than the equivalent CPU `cv::cvtColor` path — and understand why the raw layout knowledge is what makes this possible.

### Build & run
```bash
cd A2_YUV_Pipeline
cmake -B build
cmake --build build
./build/yuv_pipeline_demo --input ../../../assets/sample_nv12.yuv --width 1920 --height 1080
```

### Verify
Two output files appear:
- `output_rgba.bmp` — correctly colored 1920×1080 image (green-pink = conversion bug)
  - Green-pink means the Y/UV byte offsets are wrong — check that the UV plane starts at byte `width * height`, not at `width`.
- `output_y_channel.bmp` — grayscale image (the Y luminance plane extracted without a copy)

Console prints a three-row comparison:
```
OpenCV CPU (cvtColor):   XX.X ms
OpenCL kernel:            X.X ms
Speedup:                  X.Xx
```

The performance gate is the OpenCL kernel time: it must be **< 2 ms** at 1080p. The CPU time will vary by machine and is shown for comparison only.

### Mini-challenge: YUYV Port

**Part 1 — Port to YUYV (4:2:2)**

Webcams often output YUYV instead of NV12. The format is packed — no separate UV plane:

```text
Byte stream: Y0 U0 Y1 V0 Y2 U1 Y3 V1 ...
             ↑──────────↑  ← 4 bytes encode 2 pixels
```
Each pair of pixels shares one U and one V sample. Index arithmetic for pixel `x`:

- `Y = buf[x * 2]`
- `U = buf[(x & ~1) * 2 + 1]`  (even column's U, shared with odd neighbour)
- `V = buf[(x & ~1) * 2 + 3]`

Write a `yuyv_to_rgba` kernel using the same BT.601 coefficients. The math is identical — only the index arithmetic changes. Verify with `output_yuyv_rgba.bmp`.

#### Part 2 — Two-pass vs single-pass on YUYV

Split the YUYV conversion into two separate kernel dispatches:

1. `extract_y_yuyv` — reads YUYV buffer, writes a Y-only grayscale buffer.
2. `yuyv_rgba_from_y` — reads the Y buffer + original YUYV (for U/V), writes RGBA.

Time both dispatches with `cl::Event` and sum them. (Sum the nanosecond durations from `CL_PROFILING_COMMAND_END - CL_PROFILING_COMMAND_START` for each event, then convert to ms — same pattern as Module 1.) Compare to your single-pass `yuyv_to_rgba` time.

The two-pass path reads the YUYV buffer twice and writes an intermediate Y buffer — doubling memory traffic. On hardware with a large GPU L2 cache the gap may be smaller than 2x if the intermediate buffer stays cached, but the extra write always costs something.

```text
Single-pass yuyv_to_rgba:    X.X ms
Two-pass (Y extract + RGBA): X.X ms   ← expect ~2x
```

### Core Concept: NV12 Layout and the Single-Pass Advantage

Real cameras and codecs do not output RGB. They output YUV — luminance (Y) separate from chrominance (U, V) — because human vision is roughly 4x more sensitive to brightness than color. Chroma subsampling (4:2:0) stores one U/V sample per 2×2 pixel block, cutting bandwidth roughly in half with near-zero perceptual loss.

**NV12 memory layout (the format your webcam likely uses):**

```text
Y plane:   YYYYYYYY   ← full resolution, 1 byte/pixel
UV plane:  UVUVUVUV   ← half resolution, interleaved, 2 bytes per 2×2 block
```

Your kernel receives a flat byte buffer. Stride (pitch) can be wider than width — always use `pitch` for row offsets, never `width`. Pitch equals bytes per row; it may exceed `width` when the driver pads rows for memory alignment — pass it as an explicit kernel argument alongside `width` and `height`.

**Why a single-pass kernel wins over CPU `cv::cvtColor`:**

`cv::cvtColor` converts NV12 to RGBA using multiple passes internally — it reads the Y plane, reads the UV plane, computes the conversion, and writes the result with intermediate buffers and no control over memory access patterns. On the CPU this also serializes across pixels.

The OpenCL kernel reads the NV12 buffer once per pixel, computes the YUV-to-RGBA conversion inline, and writes the result once. One pass, no intermediate copies, all pixels in parallel. This is only possible because you address the Y and UV planes directly at known byte offsets — which requires understanding the raw layout. The layout knowledge is not an end in itself; it is what unlocks the single-pass access pattern.

---

## A3_1_OpenCV_DNN — High-Level Inference (T-API)

**Goal**: Run a segmentation model via OpenCV DNN with `DNN_TARGET_OPENCL` and pass the output mask directly to your OpenCL kernel — `cv::UMat` never touches the CPU.

### Build & run
```bash
cd A3_1_OpenCV_DNN
cmake -B build
cmake --build build
./build/opencvdnn_demo --input ../../../assets/face.png --model ../../../assets/selfie_segmentation.onnx
```

### Verify
- `output_mask.bmp` — binary mask (white = person, black = background)
- `output_blurred.bmp` — background blurred, subject sharp
- Console prints inference time and blur kernel time separately

### Mini-challenge: CPU vs GPU target

Swap `DNN_TARGET_OPENCL` for `DNN_TARGET_CPU`. Compare inference times at 224×224 vs 1080p input. At what resolution does GPU start winning?

### Core Concept: OpenCV DNN T-API

`setPreferableTarget(DNN_TARGET_OPENCL)` routes computation through OpenCL internally. The output blob stays in GPU memory as a `cv::UMat`. You pass its underlying `cl_mem` handle directly to your kernel — no CPU round-trip.

```cpp
cv::UMat mask_umat;
net.forward(mask_umat);               // stays on GPU
cl_mem raw = (cl_mem)mask_umat.handle(cv::ACCESS_READ);
// retain=true: cl::Buffer must not release a cl_mem it does not own
cl::Buffer mask_buf(raw, /*retain=*/true);
kernel.setArg(1, mask_buf);           // hand off to your blur kernel
```

**When to use**: OpenCV is already in your stack and ease of integration matters. The T-API hides memory management but gives you less control over buffer layout.

---

## A3_2_OpenVINO_GPU — Low-Level Inference (RemoteTensor API)

**Goal**: Run the same segmentation model via OpenVINO GPU plugin, passing a `cl::Buffer` directly as an input tensor — the inference engine reads from and writes to your OpenCL-managed memory with no host round-trip. Intel iGPU required.

### Build & run
```bash
source /opt/intel/openvino/setupvars.sh   # once per shell session
cd A3_2_OpenVINO_GPU
cmake -B build
cmake --build build
./build/openvino_gpu_demo --input ../../../assets/face.png --model ../../../assets/selfie_segmentation.onnx
```

### Verify
- `output_mask.bmp` — binary mask (white = person, black = background)
- `output_blurred.bmp` — background blurred, subject sharp
- Console prints:

```text
[A3_2 OpenVINO GPU]
Inference  (wall-clock, 5 runs):
  run  1:   110.413 ms  <- JIT warm-up
  run  2:     8.202 ms
  run  3:     4.443 ms
  run  4:     4.144 ms
  run  5:     4.174 ms
  --------------------------
  min:        4.144 ms
  avg*:       5.241 ms  (* runs 2+)
Blur kernel  (cl::Event):       2.067 ms
```

Run 1 includes GPU driver JIT compilation — expected, documented in **Known Issues** below. The stable inference latency is `avg*` (runs 2+). Blur is timed via `cl::Event`; inference uses `std::chrono::steady_clock` (OpenVINO does not expose a `cl::Event` for the full request). Use `--runs N` to control iteration count.

### Mini-challenge: A3_1 vs A3_2 Latency

Compare `DNN_TARGET_OPENCL` (A3_1) vs OpenVINO GPU plugin (A3_2) inference latency at 224×224 and 1080p input. Run each 100 times and report the median. Which wins at each resolution, and why? Consider: T-API kernel caching, RemoteTensor import overhead on first call, and driver-level scheduling differences between the two paths.

### Core Concept: OpenVINO RemoteTensor API

OpenVINO's GPU plugin runs inference internally on OpenCL. The RemoteTensor API exposes that internal `cl_mem` boundary: you can import your own `cl::Buffer` as an input tensor and export the output tensor's `cl_mem` handle directly into your next kernel call. Nothing leaves the GPU.

This is what A3_1's T-API hides. In A3_1, OpenCV creates and owns the GPU buffer; you extract the handle after the fact. Here, you own the buffer from the start and hand it in.

```cpp
ov::Core core;
ov::CompiledModel model = core.compile_model("selfie_segmentation.onnx", "GPU");

// Get the shared OpenCL context OpenVINO is using internally.
auto remote_ctx = model.get_context().as<ov::intel_gpu::ocl::ClContext>();

// Wrap your existing cl::Buffer as an OpenVINO RemoteTensor.
// OpenVINO reads from this buffer directly — no copy, no staging.
auto input_tensor = remote_ctx.create_tensor(
    model.input().get_element_type(),
    model.input().get_shape(),
    input_cl_buffer.get()    // raw cl_mem handle
);

ov::InferRequest req = model.create_infer_request();
req.set_input_tensor(input_tensor);
req.infer();

// Extract the output tensor's cl_mem handle and pass to the blur kernel.
auto output_tensor = req.get_output_tensor().as<ov::intel_gpu::ocl::ClBufferTensor>();
cl_mem mask_cl = output_tensor.get();
// retain=true: cl::Buffer must not release a cl_mem it does not own
cl::Buffer mask_buf(mask_cl, /*retain=*/true);
CL_CHECK(blur_kernel.setArg(1, mask_buf));
```

**When to use**: Intel iGPU in production pipelines where OpenCV is not in the stack, or where you need explicit control over tensor buffer lifetime and layout without the T-API abstraction overhead.

### A3_1 vs A3_2 — When to Use Which

| | A3_1 OpenCV DNN | A3_2 OpenVINO GPU |
|:--|:--|:--|
| **Integration effort** | Low (T-API handles it) | Medium (explicit RemoteTensor wiring) |
| **Control over buffers** | Low (OpenCV owns buffers) | High (you own the `cl_mem`) |
| **Target platforms** | Desktop / server with OpenCV | Intel iGPU, production pipelines |
| **Model format** | ONNX, Caffe, TF | ONNX (native), IR (converted) |
| **OpenCV dependency** | Required | None |
| **Postprocessing in OpenCL** | Straightforward | Zero-copy: output `cl_mem` wires directly |

---

## A4_Smart_Webcam — Flagship Project

**Goal**: Build a live webcam pipeline running at ≥ 30 FPS @ 1080p: capture → AI segmentation → OpenCL Bokeh blur → display, with the neural network output feeding directly into the kernel.

### Build & run
```bash
cd A4_Smart_Webcam
cmake -B build
cmake --build build
./build/smart_webcam --device 0
# Offline test (no webcam): ./build/smart_webcam --input ../../../assets/face.png --loop
# GPU=NVIDIA ./build/smart_webcam --device 0
```

A4 uses the RemoteTensor path from A3_2 internally. The first frame will show a JIT warm-up spike in the `Inference` column (~80–160 ms) — this is the OpenVINO GPU plugin compiling its OpenCL kernels on first use. It is expected, not a bug. Frames 2 onwards stabilize to ~4–8 ms inference (f32, 256×256 model, Intel Xe).

### Verify
Live preview window shows:
- Subject in sharp focus
- Background blurred (Gaussian / box filter kernel applied only to background pixels)
- Console prints per-frame breakdown:
  ```
  Frame 1 (JIT warm-up — expected, do not measure FPS here):
    Capture:    2.1 ms
    Inference: 143.2 ms   <- JIT compilation, one-time cost
    Kernel:     3.2 ms
    Display:    1.1 ms

  Frame 2+ (stable):
    Capture:    2.1 ms
    Inference:  5.4 ms
    Kernel:     3.2 ms
    Display:    1.1 ms
    Total:     11.8 ms  ← must be < 33 ms to pass
  ```

When measuring FPS, skip frame 1 — its `Inference` time includes one-time GPU driver JIT compilation and is not representative of runtime throughput.

### Stretch Challenge

The main tutorial blurs the entire background. The challenge: blur only a detected face bounding box (ROI).

Replace the segmentation model with a face detector (YuNet) that outputs a bounding box, then launch the blur kernel only over that region. Key API to explore: `global_work_offset` and `global_work_size` in `enqueueNDRangeKernel`. Use `cl::Event` timing to compare Full-Frame vs ROI performance.

**Performance gate:** < 20 ms/frame @ 1080p (single face).

### Core Concept: Pipeline Architecture

A4 is where the individual lessons from A3_1 and A3_2 compose into a production-quality zero-copy pipeline. Understanding why each wiring decision was made is more useful than the API calls themselves.

**How the stages chain:**

```text
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
             display / output_blurred.bmp
```

Three decisions make this zero-copy:

1. **Shared OpenCL context.** OpenVINO is initialized with `ClContext(core, ctx.get())` — the same `cl_context` your preprocessing kernel uses. Without this, the GPU plugin creates its own internal context and there is no shared address space to import buffers across.

2. **Output tensor pre-allocated before `infer()`.** `req.set_output_tensor()` is called with a pre-allocated `cl::Buffer` wrapped as a `ClBufferTensor` *before* the first `req.infer()`. Without this, `get_output_tensor()` after inference returns a plain host `Tensor` — the GPU plugin silently falls back to host memory. Pre-allocation pins the output to device memory from the start.

3. **`queue.finish()` gates the frame loop.** The blur kernel operates on a `cl_mem` owned by the `InferRequest`. That handle is only valid while the request is alive and not re-invoked. `CL_CHECK(queue.finish())` ensures the blur kernel has completed before the loop restarts or the request is destroyed. Omitting it causes undefined behaviour — typically corrupted output or a crash after an unpredictable number of frames, not on frame 1.

**Why `CL_MEM_READ_WRITE` for both buffers.** The OpenVINO GPU plugin rejects `CL_MEM_READ_ONLY` and `CL_MEM_WRITE_ONLY` on imported buffers — it performs in-place layout transformations during dispatch and requires read-write access on both. Allocate with `CL_MEM_READ_WRITE` for any buffer that crosses the OpenVINO boundary.

### Mini-challenge
Inside the Bokeh kernel, the condition `if (mask[id] == BACKGROUND)` causes thread divergence — within one warp, some threads blur and others do nothing. Replace it with `select()` (branchless) and measure the kernel time difference. See [Toolbox: Thread Divergence](../../99_Toolbox/ThreadDivergence/ThreadDivergence.md).

When measuring the FPS improvement, discard frame 1 from your calculation — the JIT warm-up spike will otherwise dominate the average and hide the kernel-level gain you are measuring.

---

## Performance Gate

This track is complete when:

| Project | Metric | Target |
|:--------|:-------|:-------|
| A1 OpenCV Interop | Zero-copy path faster than copy path (discrete GPU); near-equal on iGPU is expected | — |
| A2 YUV Pipeline — nv12_to_rgba | Kernel time (cl::Event) | < 2 ms @ 1920×1080 |
| AI Smart Webcam — Bokeh | Frame time | < 33 ms @ 1080p (30 FPS) |
| AI Smart Webcam — Privacy ROI | Frame time | < 20 ms @ 1080p |

**Measure with `cl::Event` profiling**, not wall-clock estimates. Profile each stage: upload, inference, kernel, download. The bottleneck will tell you which Toolbox technique to apply.

---

## Troubleshooting

- **Green-pink checkerboard output**: NV12 buffer fed to an RGB kernel without conversion. Run `A2_YUV_Pipeline` first.
- **`DNN_TARGET_OPENCL` silently falls back to CPU**: OpenCV not built with OpenCL support. Check: `python3 -c "import cv2; print(cv2.getBuildInformation())"` and look for `OpenCL: YES`.
- **UMat interop crashes (A3_1)**: OpenCV and your OpenCL runtime must share the same ICD. Verify `clinfo -l` matches what OpenCV reports internally.
- **OpenVINO GPU plugin not found (A3_2)**: Install `libopenvino-dev` and run `source /opt/intel/openvino/setupvars.sh` before building. Verify GPU device is visible: `python3 -c "from openvino import Core; print(Core().available_devices)"` — expect `GPU` in the list.
- **Webcam gives wrong resolution**: Add `--width 1920 --height 1080` flags; some webcams default to 640×480.
- **Wrong GPU**: `GPU=NVIDIA ./build/smart_webcam`, `GPU=AMD ./build/smart_webcam`, `GPU=INTEL ./build/smart_webcam`.
- **Inspect OpenCV**: use `OPENCV_LOG_LEVEL=VERBOSE`
- **First frame shows ~100–160 ms inference time (A4)**: JIT warm-up — the OpenVINO GPU plugin compiles its OpenCL kernels on the first `req.infer()` call. Normal behaviour. Frame 2+ stabilizes to ~4–8 ms. Do not measure FPS using frame 1.
- **INT8 ONNX model is slower than f32 on Intel Xe (A4)**: QDQ-format INT8 (`QuantizeLinear`/`DequantizeLinear` nodes) is not fused by the GPU plugin into native INT8 dispatch — each node runs as a separate op with a full memory round-trip, making it ~2.5× slower than f32. Use the f32 ONNX model.
- **Pipeline hangs or output corrupted after frame N (A4)**: Missing `queue.finish()` before the `InferRequest` goes out of scope or is reused. The blur kernel's `cl_mem` is owned by the request — if the kernel has not finished when the request is destroyed or re-invoked, the backing memory is freed or overwritten mid-kernel. Add `CL_CHECK(queue.finish())` immediately after `enqueueNDRangeKernel`.
- **A1: both paths show similar timing (iGPU)**: On iGPU, the GPU buffer shares physical memory with system RAM — zero-copy and copy paths converge. This is expected behavior, not a bug. Repeat the experiment on a discrete GPU to see the full gap.

---

## Known Issues / Hardware Notes — Intel Xe iGPU

Results below were gathered on Iris Xe Graphics (12th-gen Intel). Directly relevant to A3_2 and A4.

- **f32 ONNX is the fastest format on Intel Xe.** INT8 ONNX (QDQ format) and INT8 OpenVINO IR both ran ~2.5× *slower* than f32 at stable inference. The GPU plugin does not fuse `QuantizeLinear`/`DequantizeLinear` nodes from third-party QDQ models into native INT8 dispatch — they execute as separate ops with full memory round-trips. Genuine INT8 speedup requires NNCF-aware quantization (OpenVINO's own calibration flow), which is out of scope for this track.

- **First inference call measures JIT warm-up, not inference.** The OpenVINO GPU plugin JIT-compiles its OpenCL kernels on the first `req.infer()` call. On Iris Xe this adds ~80 ms (f32) or ~160 ms (INT8) to the first call; subsequent calls stabilize at ~4–8 ms (f32). Always run at least one untimed warm-up call before recording latency — single-call benchmarks measure JIT overhead, not model performance.

- **FP16 execution hint has no measurable effect** at this model size. Passing `ov::hint::inference_precision(ov::element::f16)` showed no consistent speedup for the 256×256 segmentation model — too small for the throughput gain to exceed scheduling noise.

- **These observations are driver- and model-size-specific.** Larger models (ResNet-50+), Intel Arc dGPUs, or newer driver versions may yield different results. Always profile your actual hardware with your actual model before choosing a quantization strategy.

---

## What's Next

[Track B: Graphics/HPC](../B_Graphics_HPC/GraphicsHPC.md) — ray tracing, CLBlast, and BVH acceleration structures.

[Track C: Robotics/ROS 2](../C_Robotics_ROS2/RoboticsROS2.md) — GPU acceleration inside a ROS 2 node, Lidar perception.

[Optimization Toolbox](../../99_Toolbox/Toolbox.md) — Zero-Copy, Thread Divergence, Local Memory, Async Pipelines.
