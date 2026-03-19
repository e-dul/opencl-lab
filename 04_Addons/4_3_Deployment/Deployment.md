# 4.3 — Deployment: Shipping an OpenCL Application

**When to use**: you've built something that works on your machine and need it to run on a customer's.

## Prerequisites
See [main README](../../README.md) for base requirements (OpenCL 1.2+, CMake 3.18+).

- Module 1 sufficient — no track required.

## Build & Run
```bash
cd 04_Addons/4_3_Deployment
cmake -B build && cmake --build build
./build/deployment_demo     # tests runtime dependency resolution
```

## Verify
```
[ICD Loader] Found 2 platforms: NVIDIA, Intel
[Runtime   ] Selected: NVIDIA GeForce RTX 3080
[Self-check] Kernel compiled and executed successfully
[Package   ] AppImage bundle: deployment_demo-x86_64.AppImage
```

## Concept: The ICD Loader

OpenCL applications link against `libOpenCL.so` (the ICD Loader), not the vendor driver directly. At runtime the loader reads `/etc/OpenCL/vendors/*.icd` to discover installed platforms.

```
your_app
└── libOpenCL.so      (ICD Loader — ship this)
    ├── nvidia.icd     (installed by user's driver — DO NOT ship)
    └── intel.icd      (installed by user's driver — DO NOT ship)
```

Your app needs:
1. `libOpenCL.so` — provided by `ocl-icd-libopencl1` on Ubuntu
2. At least one vendor ICD file — installed with the GPU driver (user's responsibility)

## Packaging by Target

**Linux AppImage**:
```bash
# Bundle libOpenCL.so inside the AppImage
linuxdeploy --appdir AppDir --library /usr/lib/x86_64-linux-gnu/libOpenCL.so.1
appimagetool AppDir deployment_demo-x86_64.AppImage
```
ICD files come from the user's driver — you cannot bundle them.

**Docker**:
```dockerfile
# Nvidia
FROM nvidia/opencl:runtime
# Intel iGPU / CPU
FROM intel/oneapi-basekit
# AMD (ROCm)
FROM rocm/opencl-dev
```
Mount `/dev/dri` for AMD/Intel passthrough: `docker run --device /dev/dri ...`

**Windows installer**: include `OpenCL.dll` (Khronos ICD loader) in your installer. Vendor ICDs are installed with the GPU driver — link to the driver download page in your installer UI.

## Mini-Challenge

Build a Docker image that runs `deployment_demo` using PoCL (CPU fallback) — no GPU required. This is useful for CI pipelines that test OpenCL logic without GPU access:
```dockerfile
FROM ubuntu:22.04
RUN apt-get install -y pocl-opencl-icd ocl-icd-libopencl1
```

## Troubleshooting

- **`clGetPlatformIDs` returns 0 platforms in Docker**: missing `--device /dev/dri` mount or the ICD file is not present inside the container.
- **AppImage works on your machine, fails on target**: the target may have a different glibc version. Build the AppImage on the oldest supported Ubuntu LTS.

---

[Back to Add-ons](../Addons.md)
