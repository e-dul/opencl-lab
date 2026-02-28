# Path A: Multimedia & AI

Process real video with a GPU pipeline that ships in production. You start with a single annoying copy that eats half your frame budget, eliminate it, then chain a neural network directly into your OpenCL kernel to build a smart webcam.

## Prerequisites
See [main README](../../README.md) for base requirements (OpenCL, CMake, Docker setup).

**Additional:**
- OpenCV 4.5+: `sudo apt install libopencv-dev`
- Verify: `pkg-config --modversion opencv4`

## Contents
```
A1_OpenCV_Interop/      Measure and eliminate the cv::Mat → GPU copy overhead
A2_YUV_Pipeline/        YUV color space kernels: NV12 → RGBA conversion
A3_1_OpenCV_DNN/        Inference via OpenCV DNN T-API (UMat stays on GPU)
A3_2_TFLite_GPU/        Inference via TFLite GPU delegate (explicit buffer mapping)
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

## A2_YUV_Pipeline — See What the Camera Actually Sends

**Goal**: Write OpenCL kernels for YUV color space conversion and understand why raw camera frames look like a green-pink checkerboard before conversion.

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
- `output_y_channel.bmp` — grayscale image (the Y luminance plane extracted without a copy)

### Core Concept: Why YUV?
Real cameras and codecs don't output RGB. They output YUV — luminance (Y) separate from chrominance (U, V) — because human vision is ~4× more sensitive to brightness than color. Chroma subsampling (4:2:0) stores one U/V sample per 2×2 pixel block, cutting bandwidth roughly in half with near-zero perceptual loss.

**NV12 layout (the format your webcam likely uses):**
```
Y plane:   YYYYYYYY   ← full resolution, 1 byte/pixel
UV plane:  UVUVUVUV   ← half resolution, interleaved, 2 bytes per 2×2 block
```
Your kernel receives a flat byte buffer. Stride (pitch) can be wider than width — always use `pitch` for row offsets, never `width`.

### Mini-challenge
Extract only the U channel into a separate BMP. What does it look like on a natural image? What does it look like on a solid red patch?

---

## A3_1_OpenCV_DNN — High-Level Inference (T-API)

**Goal**: Run a segmentation model via OpenCV DNN with `DNN_TARGET_OPENCL` and pass the output mask directly to your OpenCL kernel — `cv::UMat` never touches the CPU.

### Build & run
```bash
cd A3_1_OpenCV_DNN
cmake -B build
cmake --build build
./build/opencvdnn_demo --input ../../../assets/person.jpg --model ../../../assets/selfie_segmentation.onnx
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
kernel.setArg(1, cl::Buffer(raw));    // hand off to your blur kernel
```

**When to use**: OpenCV is already in your stack and ease of integration matters. The T-API hides memory management but gives you less control over buffer layout.

### Mini-challenge
Swap `DNN_TARGET_OPENCL` for `DNN_TARGET_CPU`. Compare inference times at 224×224 vs 1080p input. At what resolution does GPU start winning?

---

## A3_2_TFLite_GPU — Low-Level Inference (Explicit Buffer Mapping)

**Goal**: Run the same model via TensorFlow Lite with the GPU delegate, using `clEnqueueMapBuffer` to hand off the OpenCL buffer pointer directly to `TfLiteGpuDelegateV2` — and back out again.

### Build & run
```bash
cd A3_2_TFLite_GPU
cmake -B build
cmake --build build
./build/tflite_gpu_demo --input ../../../assets/person.jpg --model ../../../assets/selfie_segmentation.tflite
```

### Verify
Same visual outputs as A3_1. Console additionally prints:
```
Map buffer (input):    0.02 ms
TFLite inference:      6.1 ms
Map buffer (output):   0.02 ms
Blur kernel:           3.4 ms
```

### Core Concept: Explicit Buffer Handoff
TFLite GPU delegate can accept and return raw `cl_mem` handles, skipping the serialization step entirely.

```cpp
// Map your cl::Buffer into the TFLite input tensor
void* mapped = clEnqueueMapBuffer(queue, input_buf, CL_TRUE,
                                  CL_MAP_WRITE, 0, size, 0, nullptr, nullptr, &err);
TfLiteGpuDelegateV2Options opts = TfLiteGpuDelegateV2OptionsDefault();
opts.experimental_flags |= TFLITE_GPU_EXPERIMENTAL_FLAGS_CL_COMMAND_QUEUE_IMPORT;
// ... bind queue, extract output cl_mem, pass to blur kernel
```

**When to use**: Edge targets (Raspberry Pi, Jetson, phones) where the OpenCV stack is too heavy, or when you need to control buffer alignment for the delegate's internal tiling.

### Mini-challenge
Profile the `clEnqueueMapBuffer` call with `CL_MAP_WRITE` vs `CL_MAP_READ`. Why does write-mapping cost more on discrete GPU than on iGPU? (Hint: UMA vs PCIe.)

### A3_1 vs A3_2 — When to Use Which

| | A3_1 OpenCV DNN | A3_2 TFLite GPU |
|:--|:--|:--|
| **Integration effort** | Low (T-API handles it) | High (explicit `cl_mem` wiring) |
| **Control over buffers** | Low | High |
| **Target platforms** | Desktop / server | Edge / embedded |
| **Model format** | ONNX, Caffe, TF | `.tflite` only |
| **Postprocessing in OpenCL** | Straightforward | Requires careful sync |

---

## A4_Smart_Webcam — Flagship Project

**Goal**: Build a live webcam pipeline running at ≥ 30 FPS @ 1080p: capture → AI segmentation → OpenCL Bokeh blur → display, with the neural network output feeding directly into the kernel.

### Build & run
```bash
cd A4_Smart_Webcam
cmake -B build
cmake --build build
./build/smart_webcam --device 0
# Offline test (no webcam): ./build/smart_webcam --input ../../../assets/person.jpg --loop
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

1. Replace the segmentation model with YuNet face detector.
2. Use `global_work_offset` and `global_work_size` to launch the blur kernel only over the ROI.
3. Handle boundary conditions when the face is partially outside the frame.
4. Compare Full-Frame vs ROI performance with `cl::Event` timing.

**Performance gate:** Privacy Mode < 20 ms/frame @ 1080p (single face).

### Mini-challenge
Inside the Bokeh kernel, the condition `if (mask[id] == BACKGROUND)` causes thread divergence — within one warp, some threads blur and others do nothing. Replace it with `select()` (branchless) and measure the kernel time difference. See [Toolbox: Thread Divergence](../../99_Toolbox/ThreadDivergence/ThreadDivergence.md).

---

## Performance Gate

This track is complete when:

| Project | Metric | Target |
|:--------|:-------|:-------|
| AI Smart Webcam — Bokeh | Frame time | < 33 ms @ 1080p (30 FPS) |
| AI Smart Webcam — Privacy ROI | Frame time | < 20 ms @ 1080p |

**Measure with `cl::Event` profiling**, not wall-clock estimates. Profile each stage: upload, inference, kernel, download. The bottleneck will tell you which Toolbox technique to apply.

---

## Troubleshooting

- **Green-pink checkerboard output**: NV12 buffer fed to an RGB kernel without conversion. Run `A2_YUV_Pipeline` first.
- **`DNN_TARGET_OPENCL` silently falls back to CPU**: OpenCV not built with OpenCL support. Check: `python3 -c "import cv2; print(cv2.getBuildInformation())"` and look for `OpenCL: YES`.
- **UMat interop crashes (A3_1)**: OpenCV and your OpenCL runtime must share the same ICD. Verify `clinfo -l` matches what OpenCV reports internally.
- **TFLite GPU delegate not found (A3_2)**: Build TFLite from source with `-DTFLITE_ENABLE_GPU=ON`, or use a pre-built delegate `.so` from the TFLite nightly releases. ARM-only delegates will not work on x86.
- **`clEnqueueMapBuffer` returns null (A3_2)**: Buffer must have been created with `CL_MEM_ALLOC_HOST_PTR` or `CL_MEM_USE_HOST_PTR` for host-mappable memory. `CL_MEM_COPY_HOST_PTR` alone is not mappable on all drivers.
- **Webcam gives wrong resolution**: Add `--width 1920 --height 1080` flags; some webcams default to 640×480.
- **Wrong GPU**: `GPU=NVIDIA ./build/smart_webcam`, `GPU=AMD ./build/smart_webcam`, `GPU=INTEL ./build/smart_webcam`.

---

## What's Next

[Track B: Graphics/HPC](../B_Graphics_HPC/GraphicsHPC.md) — ray tracing, CLBlast, and BVH acceleration structures.

[Track C: Robotics/ROS 2](../C_Robotics_ROS2/RoboticsROS2.md) — GPU acceleration inside a ROS 2 node, Lidar perception.

[Optimization Toolbox](../../99_Toolbox/Toolbox.md) — Zero-Copy, Thread Divergence, Local Memory, Async Pipelines.
