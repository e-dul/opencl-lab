# Path A: Multimedia & AI

Process real video with a GPU pipeline that ships in production. You start with a single annoying copy that eats half your frame budget, eliminate it, then chain a neural network directly into your OpenCL kernel to build a smart webcam.

## Prerequisites
See [main README](../../README.md) for base requirements (OpenCL, CMake, Docker setup).

**Additional:**
- OpenCV 4.5+: `sudo apt install libopencv-dev` — verify: `pkg-config --modversion opencv4`
- A3_2 only: Intel iGPU required. See [A3_2_OpenVINO_GPU/SETUP.md](A3_2_OpenVINO_GPU/SETUP.md).
- Assets in repository root: `assets/face.png`, `assets/selfie_segmentation.onnx`.

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
Console prints two transfer times:
```
[COPY]     cv::Mat → clEnqueueWriteBuffer:  8.4 ms
[ZERO-COPY] UMat → cl::Buffer (map):        0.1 ms
```
The zero-copy path should show a measurable reduction. On discrete GPU the gap is clear at 1080p; on integrated GPU (iGPU) the buffer may be physically shared, reducing it to near zero.

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

### Core Concept: NV12 Layout and the Single-Pass Advantage

Real cameras and codecs do not output RGB. They output YUV — luminance (Y) separate from chrominance (U, V) — because human vision is roughly 4x more sensitive to brightness than color. Chroma subsampling (4:2:0) stores one U/V sample per 2×2 pixel block, cutting bandwidth roughly in half with near-zero perceptual loss.

**NV12 memory layout (the format your webcam likely uses):**
```
Y plane:   YYYYYYYY   ← full resolution, 1 byte/pixel
UV plane:  UVUVUVUV   ← half resolution, interleaved, 2 bytes per 2×2 block
```
Your kernel receives a flat byte buffer. Stride (pitch) can be wider than width — always use `pitch` for row offsets, never `width`. Pitch equals bytes per row; it may exceed `width` when the driver pads rows for memory alignment — pass it as an explicit kernel argument alongside `width` and `height`.

**Why a single-pass kernel wins over CPU `cv::cvtColor`:**

`cv::cvtColor` converts NV12 to RGBA using multiple passes internally — it reads the Y plane, reads the UV plane, computes the conversion, and writes the result with intermediate buffers and no control over memory access patterns. On the CPU this also serializes across pixels.

The OpenCL kernel reads the NV12 buffer once per pixel, computes the YUV-to-RGBA conversion inline, and writes the result once. One pass, no intermediate copies, all pixels in parallel. This is only possible because you address the Y and UV planes directly at known byte offsets — which requires understanding the raw layout. The layout knowledge is not an end in itself; it is what unlocks the single-pass access pattern.

### Mini-challenge

**Part 1 — Port to YUYV (4:2:2)**

Webcams often output YUYV instead of NV12. The format is packed — no separate UV plane:
```
Byte stream: Y0 U0 Y1 V0 Y2 U1 Y3 V1 ...
             ↑──────────↑  ← 4 bytes encode 2 pixels
```
Each pair of pixels shares one U and one V sample. Index arithmetic for pixel `x`:
- `Y = buf[x * 2]`
- `U = buf[(x & ~1) * 2 + 1]`  (even column's U, shared with odd neighbour)
- `V = buf[(x & ~1) * 2 + 3]`

Write a `yuyv_to_rgba` kernel using the same BT.601 coefficients. The math is identical — only the index arithmetic changes. Verify with `output_yuyv_rgba.bmp`.

**Part 2 — Two-pass vs single-pass on YUYV**

Split the YUYV conversion into two separate kernel dispatches:
1. `extract_y_yuyv` — reads YUYV buffer, writes a Y-only grayscale buffer.
2. `yuyv_rgba_from_y` — reads the Y buffer + original YUYV (for U/V), writes RGBA.

Time both dispatches with `cl::Event` and sum them. (Sum the nanosecond durations from `CL_PROFILING_COMMAND_END - CL_PROFILING_COMMAND_START` for each event, then convert to ms — same pattern as Module 1.) Compare to your single-pass `yuyv_to_rgba` time.

The two-pass path reads the YUYV buffer twice and writes an intermediate Y buffer — doubling memory traffic. On hardware with a large GPU L2 cache the gap may be smaller than 2x if the intermediate buffer stays cached, but the extra write always costs something.

```
Single-pass yuyv_to_rgba:    X.X ms
Two-pass (Y extract + RGBA): X.X ms   ← expect ~2x
```

The extra pass reads the YUYV buffer twice and writes an intermediate Y buffer — pure memory bandwidth waste. This is the same cost that `cv::cvtColor` pays on CPU, now visible as a number.

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

### Mini-challenge
Swap `DNN_TARGET_OPENCL` for `DNN_TARGET_CPU`. Compare inference times at 224×224 vs 1080p input. At what resolution does GPU start winning?

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
```
OpenVINO inference:   X.X ms
Blur kernel:          X.X ms
```

Both times measured with `cl::Event` — no wall-clock estimates.

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

### Mini-challenge
Compare `DNN_TARGET_OPENCL` (A3_1) vs OpenVINO GPU plugin (A3_2) inference latency at 224×224 and 1080p input. Run each 100 times and report the median. Which wins at each resolution, and why? Consider: T-API kernel caching, RemoteTensor import overhead on first call, and driver-level scheduling differences between the two paths.

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

### Verify
Live preview window shows:
- Subject in sharp focus
- Background blurred (Gaussian / box filter kernel applied only to background pixels)
- Console prints per-frame breakdown:
  ```
  Capture:    2.1 ms
  Inference:  8.4 ms
  Kernel:     3.2 ms
  Display:    1.1 ms
  Total:     14.8 ms  ← must be < 33 ms to pass
  ```

### Privacy Mode Challenge
The main tutorial blurs the entire background. The challenge: blur only a detected face bounding box (ROI).

Replace the segmentation model with a face detector (YuNet) that outputs a bounding box, then launch the blur kernel only over that region. Key API to explore: `global_work_offset` and `global_work_size` in `enqueueNDRangeKernel`. Use `cl::Event` timing to compare Full-Frame vs ROI performance.

**Performance gate:** < 20 ms/frame @ 1080p (single face).

### Mini-challenge
Inside the Bokeh kernel, the condition `if (mask[id] == BACKGROUND)` causes thread divergence — within one warp, some threads blur and others do nothing. Replace it with `select()` (branchless) and measure the kernel time difference. See [Toolbox: Thread Divergence](../../99_Toolbox/ThreadDivergence/ThreadDivergence.md).

---

## Performance Gate

This track is complete when:

| Project | Metric | Target |
|:--------|:-------|:-------|
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
---

## What's Next

[Track B: Graphics/HPC](../B_Graphics_HPC/GraphicsHPC.md) — ray tracing, CLBlast, and BVH acceleration structures.

[Track C: Robotics/ROS 2](../C_Robotics_ROS2/RoboticsROS2.md) — GPU acceleration inside a ROS 2 node, Lidar perception.

[Optimization Toolbox](../../99_Toolbox/Toolbox.md) — Zero-Copy, Thread Divergence, Local Memory, Async Pipelines.
