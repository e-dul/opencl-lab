# Bonus Modules

Advanced standalone recipes. No track dependency — pick any module independently.

## Prerequisites
- See [main README](../README.md) for base requirements (OpenCL 1.2+, CMake 3.18+).
- Module 1 complete (`01_Host_API/`) is sufficient for all modules unless noted below.

## Contents

| Module | Topic | Notes |
|:-------|:------|:------|
| [vkFFT Audio](03_VkFFT_Audio/vkFFTAudio.md) | GPU FFT via vkFFT library — no custom kernel required | Any Module 2 track |
| [Voxel Mapping](04_Voxel_Mapping/VoxelMapping.md) | 3D occupancy grid from ROS 2 point cloud via DDA ray casting | Requires 02 + 03 complete; ROS 2 Jazzy |
| [CLBlast MatMul](01_CLBlast_MatMul/CLBlastMatMul.md) | GPU matrix multiplication via CLBlast (drop-in BLAS) | Module 1 sufficient |
| [Device Enqueue](02_Device_Enqueue/DeviceEnqueue.md) | OpenCL 2.0 device-side enqueue (kernel spawning kernels) *(OpenCL 2.0)* | OpenCL 2.0+ device required |
