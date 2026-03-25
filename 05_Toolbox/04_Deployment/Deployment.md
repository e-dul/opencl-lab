# 4.3 — Deployment: Shipping an OpenCL Application

**When to use**: you've built something that works on your machine and need it to run on a customer's.

## Prerequisites
See [main README](../../README.md) for base requirements (OpenCL 1.2+, CMake 3.18+, Docker 20.10+).

- Module 1 sufficient — no track required.

## Build & Run
```bash
cd 05_Toolbox/04_Deployment
cmake -B build && cmake --build build
./build/deployment     # tests runtime dependency resolution
```

## Verify
```
[ICD Loader] Found 2 platforms: NVIDIA, Intel
[Runtime   ] Selected: NVIDIA GeForce RTX 3080
[Self-check] Kernel compiled and executed successfully
```

> **Note:** AppImage packaging is a separate step — run `bash appimage.sh` after building.

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

Run `bash appimage.sh` — tools are downloaded automatically if not in PATH.

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

> **Note:** The `rocm/opencl-dev` image name is unverified. AMD ROCm Docker images typically use the naming pattern `rocm/dev-ubuntu-<version>` (e.g., `rocm/dev-ubuntu-22.04`). Verify the correct base image tag at [hub.docker.com/r/rocm](https://hub.docker.com/r/rocm) before use.

Mount `/dev/dri` for AMD/Intel passthrough: `docker run --device /dev/dri ...`

**Windows installer**: include `OpenCL.dll` (Khronos ICD loader) in your installer. Vendor ICDs are installed with the GPU driver — link to the driver download page in your installer UI.

## Mini-Challenge

Build a Docker image that runs `deployment` using PoCL (CPU fallback) — no GPU required. This is useful for CI pipelines that test OpenCL logic without GPU access:
```dockerfile
FROM ubuntu:24.04
RUN apt-get install -y pocl-opencl-icd ocl-icd-libopencl1
```

## Troubleshooting

- **`clGetPlatformIDs` returns 0 platforms in Docker**: missing `--device /dev/dri` mount or the ICD file is not present inside the container. Fix: mount the host ICD directory and the DRI device:
  ```bash
  docker run --rm \
    --device /dev/dri \
    -v /etc/OpenCL/vendors:/etc/OpenCL/vendors:ro \
    -e GPU=AMD \
    -v $(pwd)/assets:/assets -v $(pwd)/docker_out:/output \
    deployment_test \
    ./deployment --input /assets/sample.bmp --output /output/output.bmp
  ```
  The `-v /etc/OpenCL/vendors:ro` mount makes the host GPU ICD visible inside the container. `--device /dev/dri` grants access to the DRI render node. For NVIDIA use `--gpus all` instead of `--device /dev/dri`.
- **AppImage works on your machine, fails on target**: the target may have a different glibc version. Build the AppImage on the oldest supported Ubuntu LTS.

---

[Back to Toolbox.md](../Toolbox.md)
