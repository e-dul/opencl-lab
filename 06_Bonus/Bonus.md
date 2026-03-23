# Bonus Modules

Advanced standalone recipes. No track dependency — pick any module independently.

## Prerequisites
- See [main README](../README.md) for base requirements (OpenCL 1.2+, CMake 3.18+).
- Module 1 complete (`01_Host_API/`) is sufficient for all modules unless noted below.

## Contents

| Module | Topic | Notes |
|:-------|:------|:------|
| [vkFFT Audio](vkFFT_Audio/vkFFTAudio.md) | GPU FFT via vkFFT library — no custom kernel required | Any Module 2 track |
| [Voxel Mapping](Voxel_Mapping/VoxelMapping.md) | 3D occupancy grid from ROS 2 point cloud via DDA ray casting | Requires B3 + C3 complete; ROS 2 Jazzy |
| [CLBlast MatMul](CLBlast_MatMul/) | GPU matrix multiplication via CLBlast (drop-in BLAS) | Module 1 sufficient |
| [Device Enqueue](Device_Enqueue/) | OpenCL 2.0 device-side enqueue (kernel spawning kernels) | OpenCL 2.0+ device required |
