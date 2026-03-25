# A.5 — Privacy Mode: Face Detection + ROI Blur

**Goal**: Detect face bounding boxes and blur only the detected ROI using `global_work_offset` — launching OpenCL threads exclusively over the target tile without processing the full frame.

> ⚠️ This module overlaps significantly with 04 (T-API + DNN) and the 06 Stretch Challenge. The unique teaching point is `global_work_offset`. Consider doing the 06 Stretch Challenge instead if you have completed 06.

## Prerequisites (delta from module index)

- [A.3.1 — OpenCV DNN](../04_OpenCV_DNN/OpenCVDNN.md) completed (DNN_TARGET_OPENCL, T-API concepts).
- YuNet face detector model. Download from OpenCV model zoo:
  ```bash
  wget -q https://raw.githubusercontent.com/opencv/opencv_zoo/main/models/face_detection_yunet/face_detection_yunet_2023mar.onnx \
      -O assets/yunet.onnx
  ```

## Build & Run

```bash
cd 07_Privacy_Mode
cmake -B build
cmake --build build
./build/privacy_mode --input assets/face.png --model assets/yunet.onnx
# GPU=NVIDIA ./build/privacy_mode --input assets/face.png --model assets/yunet.onnx
```

## Verify

- `output_blurred.bmp` — input image with face bounding box regions blurred; non-ROI pixels unmodified
- Console prints per-detection timing:
  ```
  Detected 1 face(s)
  ROI blur (global_work_offset):   0.8 ms
  Full-frame blur (comparison):    3.2 ms
  ROI speedup: 4.0x
  ```

**Performance gate**: ROI blur < 20 ms @ 1080p for a single detected face.

## Key Concepts

### global_work_offset: Launch Threads Only Over a Tile

Standard `enqueueNDRangeKernel` launches one thread per pixel of the full image. When you only need to blur a 200×200 face bounding box inside a 1920×1080 frame, 99.9% of those threads are wasted.

`global_work_offset` lets you specify the starting point of the global ID space. Threads launch only over the bounding box:

```cpp
// Bounding box at (x0, y0) with size (w, h)
cl::NDRange offset(x0, y0);
cl::NDRange global(w, h);
cl::NDRange local(16, 16);
CL_CHECK(queue.enqueueNDRangeKernel(blur_kernel, offset, global, local, nullptr, &event));
```

Inside the kernel, `get_global_id(0)` and `get_global_id(1)` already give the correct image coordinates — no arithmetic needed to map back from tile space.

**When to use**: processing only a small region of a large buffer (ROI, sub-tile, sparse updates). Eliminates wasted GPU threads without kernel changes.

## Troubleshooting

- **No faces detected**: check the input image has a visible face. YuNet requires faces ≥ 20×20 pixels in the input resolution.
- **Wrong GPU**: `GPU=NVIDIA ./build/privacy_mode`, `GPU=AMD ./build/privacy_mode`.

---

[Path A: Multimedia & AI](../Multimedia.md)
