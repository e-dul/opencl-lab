# Module 4: Add-ons

Seven elective case studies that extend what you learned in the core tracks into real-world integration problems: external libraries, deployment, memory architecture theory, and multi-domain system design. Non-linear — pick the add-on that matches your current problem.

## Prerequisites

See [main README](../README.md) for base requirements (OpenCL 1.2+, CMake 3.18+).

| Add-on | Requires | Extra dependencies |
|:-------|:---------|:-------------------|
| [4.1 vkFFT Audio](4_1_vkFFT_Audio/vkFFTAudio.md) | Any Module 2 track | `sudo apt install libfftw3-dev` (optional — CPU reference only; GPU path works without) · Asset: `assets/sample.wav` |
| [4.2 OpenCL vs CUDA](4_2_OpenCL_vs_CUDA/OpenCLvsCUDA.md) | Any Module 2 track | None |
| [4.3 Deployment](4_3_Deployment/Deployment.md) | Module 1 only | Docker installed |
| [4.4 SVM Deep Dive](4_4_SVM_Theory/SVMTheory.md) | Any Module 2 track | OpenCL 2.0+ device for fine-grained SVM paths (falls back gracefully on 1.2) |
| [4.5 Voxel Mapping](4_5_Voxel_Mapping/VoxelMapping.md) | Track B (B3) + Track C (C3) + ROS 2 Jazzy | Asset: `assets/lidar_sample.bag` (*optional* — requires `assets/lidar_sample.bag`; use the synthetic publisher if unavailable) |
| [4.6 FFmpeg Pipeline](4_6_FFmpeg_Pipeline/FFmpegPipeline.md) | Track A (A4) | `sudo apt install libavcodec-dev libavformat-dev libavutil-dev libswscale-dev libva-dev` (includes `libswscale-dev libva-dev` required for 4.6) · Asset: `assets/sample.mp4` |
| [4.7 SoftISP](4_7_SoftISP/SoftISP.md) | Toolbox `LocalMemory` reviewed | Asset: `assets/raw_bayer_4k.raw` |

## Contents

| Add-on | Reach for it when… | Covers |
|:-------|:-------------------|:-------|
| [4.1 vkFFT Audio](4_1_vkFFT_Audio/vkFFTAudio.md) | Integrating a third-party GPU math library without writing the kernel yourself | GPU FFT via vkFFT; real-time spectrogram from a WAV file |
| [4.2 OpenCL vs CUDA](4_2_OpenCL_vs_CUDA/OpenCLvsCUDA.md) | Choosing a GPU API for a new project | Honest market analysis — CUDA's ecosystem lock-in vs OpenCL's portability (FPGA, mobile, AMD, Intel iGPU) |
| [4.3 Deployment](4_3_Deployment/Deployment.md) | Shipping to a machine where you cannot control the drivers | Packaging with runtime deps (ICD Loader, drivers) via Docker and AppImage |
| [4.4 SVM Deep Dive](4_4_SVM_Theory/SVMTheory.md) | Zero-copy Toolbox technique underperformed — want the hardware explanation | Memory coherency model behind SVM; PCIe vs UMA; why fine-grained SVM is expensive on discrete GPU |
| [4.5 Voxel Mapping](4_5_Voxel_Mapping/VoxelMapping.md) | Finished Track B + Track C and want the grand finale | Applying B3 ray casting to a 3D occupancy map from C3 LiDAR data |
| [4.6 FFmpeg Pipeline](4_6_FFmpeg_Pipeline/FFmpegPipeline.md) | Applying a Track A filter to a video file rather than a single image | Hardware decode (NVDEC/VAAPI) direct to GPU memory; zero-copy offline transcoder pattern |
| [4.7 SoftISP](4_7_SoftISP/SoftISP.md) | Raw sensor feed (ISP bypass) or wanting a concrete LDS optimization problem | Naive bilinear Bayer debayering → LDS tile-based V2; 5× speedup on a real 4K problem |

## What's Next

- Contribute a new tool to the [Optimization Toolbox](../99_Toolbox/Toolbox.md).
- Port one of the Module 2 flagship projects to a second GPU vendor and document the performance differences.
- Open an issue if a performance gate does not hold on your hardware — hardware-specific regressions are worth documenting.
