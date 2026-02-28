# Module 4: Add-ons

Elective case studies for after you've completed a Module 2 track. Non-linear — pick any section in any order based on what you need next.

## Prerequisites
See [main README](../README.md) for base requirements (OpenCL 1.2+, CMake 3.18+).

## How to Use This Module

Each add-on is self-contained. Come here when one of these applies:
- You hit a new integration problem (FFmpeg, vkFFT, ROS 2 + ray casting)
- You want to understand a concept more deeply (SVM hardware model, memory coherency)
- You need to ship your project (deployment, packaging)
- You've finished both Track B and C and want the grand finale (4.5)

## Contents

| Add-on | When to reach for it | Folder |
|:-------|:---------------------|:-------|
| [4.1 vkFFT Audio](4_1_vkFFT_Audio/README.md) | Need GPU FFT without writing the kernel | `4_1_vkFFT_Audio/` |
| [4.2 OpenCL vs CUDA](4_2_OpenCL_vs_CUDA/README.md) | Choosing a GPU API for a new project | `4_2_OpenCL_vs_CUDA/` |
| [4.3 Deployment](4_3_Deployment/README.md) | Shipping an OpenCL app to a customer | `4_3_Deployment/` |
| [4.4 SVM Deep Dive](4_4_SVM_Theory/README.md) | Zero-copy toolbox wasn't enough — want hardware explanation | `4_4_SVM_Theory/` |
| [4.5 Voxel Mapping](4_5_Voxel_Mapping/README.md) | Grand finale: Track B + C combined (requires both) | `4_5_Voxel_Mapping/` |
| [4.6 FFmpeg Pipeline](4_6_FFmpeg_Pipeline/README.md) | Apply Track A filter to a video file offline | `4_6_FFmpeg_Pipeline/` |
| [4.7 SoftISP](4_7_SoftISP/README.md) | Raw Bayer debayering: LDS optimization on a real problem | `4_7_SoftISP/` |

## What's Next

All tracks and add-ons complete. If you want to go further:
- Contribute a new tool to the [Optimization Toolbox](../99_Toolbox/Toolbox.md)
- Port one of the Module 2 flagship projects to a second GPU vendor and document the differences
- Open an issue if a performance gate doesn't hold on your hardware — hardware-specific regressions are worth documenting
