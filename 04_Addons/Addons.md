# Module 4: Add-ons

Elective case studies for after you've completed a Module 2 track. Non-linear — pick any section in any order based on what you need next.

## Prerequisites
- Module 1 complete + at least one Module 2 track (most add-ons). Exception: 4.3 requires Module 1 only.
- See [main README](../README.md) for base requirements (OpenCL 1.2+, CMake 3.18+).

| Add-on | Additional Prerequisites |
|:-------|:-------------------------|
| 4.1 vkFFT Audio | Any Module 2 track. `sudo apt install libfftw3-dev` (optional — CPU reference; GPU path works without). Asset: `assets/sample.wav`. |
| 4.2 OpenCL vs CUDA | Any Module 2 track. No binary; no extra deps. |
| 4.3 Deployment | Module 1 only. Docker installed. |
| 4.4 SVM Deep Dive | Any Module 2 track. OpenCL 2.0+ device for fine-grained SVM paths (falls back gracefully on 1.2). |
| 4.5 Voxel Mapping | Track B (B3 complete) + Track C (C3 complete) + ROS 2 Humble+. Asset: `assets/lidar_sample.bag`. |
| 4.6 FFmpeg Pipeline | Track A (A4 complete). `sudo apt install libavcodec-dev libavformat-dev libavutil-dev`. Asset: `assets/sample.mp4`. |
| 4.7 SoftISP | Toolbox `LocalMemory` reviewed. No external library deps. Asset: `assets/raw_bayer_4k.raw`. |

## How to Use This Module

Each add-on is self-contained. Come here when one of these applies:
- You hit a new integration problem (FFmpeg, vkFFT, ROS 2 + ray casting)
- You want to understand a concept more deeply (SVM hardware model, memory coherency)
- You need to ship your project (deployment, packaging)
- You've finished both Track B and C and want the grand finale (4.5)

## Contents

| Add-on | When to reach for it | Folder |
|:-------|:---------------------|:-------|
| [4.1 vkFFT Audio](4_1_vkFFT_Audio/vkFFTAudio.md) | Need GPU FFT without writing the kernel | `4_1_vkFFT_Audio/` |
| [4.2 OpenCL vs CUDA](4_2_OpenCL_vs_CUDA/OpenCLvsCUDA.md) | Choosing a GPU API for a new project | `4_2_OpenCL_vs_CUDA/` |
| [4.3 Deployment](4_3_Deployment/Deployment.md) | Shipping an OpenCL app to a customer | `4_3_Deployment/` |
| [4.4 SVM Deep Dive](4_4_SVM_Theory/SVMTheory.md) | Zero-copy toolbox wasn't enough — want hardware explanation | `4_4_SVM_Theory/` |
| [4.5 Voxel Mapping](4_5_Voxel_Mapping/VoxelMapping.md) | Grand finale: Track B + C combined (requires both) | `4_5_Voxel_Mapping/` |
| [4.6 FFmpeg Pipeline](4_6_FFmpeg_Pipeline/FFmpegPipeline.md) | Apply Track A filter to a video file offline | `4_6_FFmpeg_Pipeline/` |
| [4.7 SoftISP](4_7_SoftISP/SoftISP.md) | Raw Bayer debayering: LDS optimization on a real problem | `4_7_SoftISP/` |

## What's Next

All tracks and add-ons complete. If you want to go further:
- Contribute a new tool to the [Optimization Toolbox](../99_Toolbox/Toolbox.md)
- Port one of the Module 2 flagship projects to a second GPU vendor and document the differences
- Open an issue if a performance gate doesn't hold on your hardware — hardware-specific regressions are worth documenting
