# Path A: Multimedia & AI

Process real video with a GPU pipeline that ships in production. You start with a single annoying copy that eats half your frame budget, eliminate it, then chain a neural network directly into your OpenCL kernel to build a smart webcam.

## Prerequisites

- Base requirements: see [main README](../README.md) (OpenCL, CMake 3.18+).
- OpenCV 4.5+: `sudo apt install libopencv-dev` — verify: `pkg-config --modversion opencv4`
- 05 / 06 only: Intel iGPU required. See [05 SETUP](05_OpenVINO_GPU/SETUP.md).
- Assets: `assets/face.png`, `assets/selfie_segmentation.onnx` — included in the repository. See `assets/assets.md` at the repository root.

## Contents

| Sub-module | Goal | Doc |
| :--------- | :--- | :-- |
| 01 — OpenCV Interop | Measure and eliminate cv::Mat → GPU copy overhead | [OpenCVInterop.md](01_OpenCV_Interop/OpenCVInterop.md) |
| 02 — YUV Pipeline | NV12 → RGBA single-pass kernel (< 2 ms @ 1080p) | [YUVPipeline.md](02_YUV_Pipeline/YUVPipeline.md) |
| 03 — YUYV Extension | Port 02 to packed YUYV webcam format | [YUYVExtension.md](03_YUYV_Extension/YUYVExtension.md) |
| 04 — OpenCV DNN | Segmentation via T-API (UMat stays on GPU) | [OpenCVDNN.md](04_OpenCV_DNN/OpenCVDNN.md) |
| 05 — OpenVINO GPU | Segmentation via RemoteTensor (zero-copy cl_mem) *(Intel OpenVINO SDK)* | [OpenVINOGPU.md](05_OpenVINO_GPU/OpenVINOGPU.md) |
| 06 — Smart Webcam | Flagship: person segmentation + Bokeh blur ≥ 30 FPS *(Intel OpenVINO SDK)* | [SmartWebcam.md](06_Smart_Webcam/SmartWebcam.md) |
| 07 — Privacy Mode | ROI blur with global_work_offset (⚠️ under review) | [PrivacyMode.md](07_Privacy_Mode/PrivacyMode.md) |
| 08 — FFmpeg Pipeline | Hardware decode → OpenCL filter, no CPU copies | [FFmpegPipeline.md](08_FFmpeg_Pipeline/FFmpegPipeline.md) |
| 09 — SoftISP | Real-time 4K Bayer debayering with LDS tiling | [SoftISP.md](09_SoftISP/SoftISP.md) |

## Performance Gates

| Sub-module | Metric | Target |
| :--------- | :----- | :----- |
| 01 OpenCV Interop | Zero-copy faster than copy (discrete GPU) | — |
| 02 YUV Pipeline | Kernel time (`cl::Event`) | < 2 ms @ 1920×1080 |
| 06 Smart Webcam | Total frame time | < 33 ms @ 1080p (30 FPS) |
| 07 Privacy ROI | ROI blur frame time | < 20 ms @ 1080p ⚠️ under review |

**Measure with `cl::Event` profiling**, not wall-clock estimates.

## Troubleshooting

- **Green-pink checkerboard**: NV12 buffer fed to an RGB kernel without conversion. Run 02 first.
- **`DNN_TARGET_OPENCL` falls back to CPU**: OpenCV not built with OpenCL. Check: `python3 -c "import cv2; print(cv2.getBuildInformation())"`.
- **OpenVINO GPU plugin not found**: See [05_OpenVINO_GPU/SETUP.md](05_OpenVINO_GPU/SETUP.md) for installation instructions.
- **First frame ~100–160 ms inference (05/06)**: JIT warm-up — expected. Measure FPS from frame 2+.
- **01: both paths similar timing (iGPU)**: UMA hardware — expected. Test on discrete GPU.

## What's Next

[Track B: Graphics & HPC](../03_GraphicsHPC/GraphicsHPC.md) — ray tracing, BVH, OpenGL interop.

[Track C: Robotics & ROS 2](../04_Robotics/RoboticsROS2.md) — GPU acceleration in ROS 2 nodes.

[Optimization Toolbox](../05_Toolbox/Toolbox.md) — Zero-Copy, Thread Divergence, Local Memory.
