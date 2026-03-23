# Path A: Multimedia & AI

Process real video with a GPU pipeline that ships in production. You start with a single annoying copy that eats half your frame budget, eliminate it, then chain a neural network directly into your OpenCL kernel to build a smart webcam.

## Prerequisites

- Base requirements: see [main README](../README.md) (OpenCL, CMake 3.18+).
- OpenCV 4.5+: `sudo apt install libopencv-dev` — verify: `pkg-config --modversion opencv4`
- A3_2 / A4 only: Intel iGPU required. See [A3_2 SETUP](A3_2_OpenVINO_GPU/SETUP.md).
- Assets: `assets/face.png`, `assets/selfie_segmentation.onnx` — included in the repository. See `assets/assets.md` at the repository root.

## Contents

| Sub-module | Goal | Doc |
| :--------- | :--- | :-- |
| A1 — OpenCV Interop | Measure and eliminate cv::Mat → GPU copy overhead | [OpenCVInterop.md](A1_OpenCV_Interop/OpenCVInterop.md) |
| A2 — YUV Pipeline | NV12 → RGBA single-pass kernel (< 2 ms @ 1080p) | [YUVPipeline.md](A2_YUV_Pipeline/YUVPipeline.md) |
| A2b — YUYV Extension | Port A2 to packed YUYV webcam format | [YUYVExtension.md](A2b_YUYV_Extension/YUYVExtension.md) |
| A3.1 — OpenCV DNN | Segmentation via T-API (UMat stays on GPU) | [OpenCVDNN.md](A3_1_OpenCV_DNN/OpenCVDNN.md) |
| A3.2 — OpenVINO GPU | Segmentation via RemoteTensor (zero-copy cl_mem) | [OpenVINOGPU.md](A3_2_OpenVINO_GPU/OpenVINOGPU.md) |
| A4 — Smart Webcam | Flagship: person segmentation + Bokeh blur ≥ 30 FPS | [SmartWebcam.md](A4_Smart_Webcam/SmartWebcam.md) |
| A5 — Privacy Mode | ROI blur with global_work_offset (⚠️ under review) | [PrivacyMode.md](A5_Privacy_Mode/PrivacyMode.md) |
| A.5 — FFmpeg Pipeline | Hardware decode → OpenCL filter, no CPU copies | [FFmpegPipeline.md](FFmpeg_Pipeline/FFmpegPipeline.md) |
| A.6 — SoftISP | Real-time 4K Bayer debayering with LDS tiling | [SoftISP.md](SoftISP/SoftISP.md) |

## Performance Gates

| Sub-module | Metric | Target |
| :--------- | :----- | :----- |
| A1 OpenCV Interop | Zero-copy faster than copy (discrete GPU) | — |
| A2 YUV Pipeline | Kernel time (`cl::Event`) | < 2 ms @ 1920×1080 |
| A4 Smart Webcam | Total frame time | < 33 ms @ 1080p (30 FPS) |
| A5 Privacy ROI | ROI blur frame time | < 20 ms @ 1080p ⚠️ under review |

**Measure with `cl::Event` profiling**, not wall-clock estimates.

## Troubleshooting

- **Green-pink checkerboard**: NV12 buffer fed to an RGB kernel without conversion. Run A2 first.
- **`DNN_TARGET_OPENCL` falls back to CPU**: OpenCV not built with OpenCL. Check: `python3 -c "import cv2; print(cv2.getBuildInformation())"`.
- **OpenVINO GPU plugin not found**: `sudo apt install libopenvino-dev` + `source /opt/intel/openvino/setupvars.sh`.
- **First frame ~100–160 ms inference (A3_2/A4)**: JIT warm-up — expected. Measure FPS from frame 2+.
- **A1: both paths similar timing (iGPU)**: UMA hardware — expected. Test on discrete GPU.

## What's Next

[Track B: Graphics & HPC](../03_GraphicsHPC/GraphicsHPC.md) — ray tracing, BVH, OpenGL interop.

[Track C: Robotics & ROS 2](../04_Robotics/RoboticsROS2.md) — GPU acceleration in ROS 2 nodes.

[Optimization Toolbox](../05_Toolbox/Toolbox.md) — Zero-Copy, Thread Divergence, Local Memory.
