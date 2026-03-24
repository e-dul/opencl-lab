# A.3.1 — OpenCV DNN: High-Level Inference (T-API)

**Goal**: Run a segmentation model via OpenCV DNN with `DNN_TARGET_OPENCL` and pass the output mask directly to your OpenCL kernel — `cv::UMat` never touches the CPU.

## Prerequisites (delta from module index)

- [A.1 — OpenCV Interop](../A1_OpenCV_Interop/OpenCVInterop.md) completed (UMat concepts).
- Assets: `assets/face.png`, `assets/selfie_segmentation.onnx` — included in the repository.

## Build & Run

```bash
cd A3_1_OpenCV_DNN
cmake -B build
cmake --build build
./build/opencv_dnn --input assets/face.png --model assets/selfie_segmentation.onnx
```

## Verify

- `output_mask.bmp` — binary mask (white = person, black = background)
- `output_blurred.bmp` — background blurred, subject sharp
- Console prints inference time and blur kernel time separately

## Key Concepts

### OpenCV DNN T-API

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

**vs A3_2**: T-API is lower integration effort; OpenVINO RemoteTensor gives explicit buffer ownership and control. See [A.3.2](../A3_2_OpenVINO_GPU/OpenVINOGPU.md) for the comparison table.

## Mini-Challenge

Swap `DNN_TARGET_OPENCL` for `DNN_TARGET_CPU`. Compare inference times at 224×224 vs 1080p input. At what resolution does GPU start winning?

## Troubleshooting

- **`DNN_TARGET_OPENCL` silently falls back to CPU**: OpenCV not built with OpenCL support. Check: `python3 -c "import cv2; print(cv2.getBuildInformation())"` and look for `OpenCL: YES`.
- **UMat interop crashes**: OpenCV and your OpenCL runtime must share the same ICD. Verify `clinfo -l` matches what OpenCV reports internally.

---

[Path A: Multimedia & AI](../Multimedia.md)
