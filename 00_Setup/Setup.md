# Module 0: Setup & Fundamentals

**Target OS:** Ubuntu 24.04 LTS (Noble Numbat)
**Hardware Focus:** Intel Integrated Graphics (Default), NVIDIA/AMD (Optional)

> **How OpenCL driver stacks work:** OpenCL uses a three-layer model (ICD Loader → vendor ICD → device). See [Deployment](../05_Toolbox/04_Deployment/Deployment.md) for the full breakdown.

After completing setup, validate your installation by following [01_Smoke_Test/SmokeTest.md](01_Smoke_Test/SmokeTest.md).

---

## 1. Intel Integrated Graphics (Quick Start)

For most laptops with Intel CPUs (5th Gen "Broadwell" and newer), OpenCL drivers are available directly from the official Ubuntu repositories.

### Step 1: Install Drivers

Open a terminal and run:

```bash
sudo apt update
sudo apt install intel-opencl-icd clinfo
```

*   `intel-opencl-icd`: The Intel Neo OpenCL driver (for Broadwell processors and newer).
*   `clinfo`: A utility to verify available OpenCL platforms and devices.

### Step 2: Verification

Run the `clinfo` command. You should see output similar to this:

```text
$ clinfo

Number of platforms                               1
  Platform Name                                   Intel(R) OpenCL HD Graphics
  Platform Vendor                                 Intel(R) Corporation
  Platform Version                                OpenCL 3.0 
  ...
  Device Name                                     Intel(R) Iris(R) Xe Graphics
  Device Type                                     GPU
  Device Vendor                                   Intel(R) Corporation
  Device OpenCL C Version                         OpenCL C 3.0 
```

**Success:** If you see "Number of platforms: 1" (or more) and your GPU name listed under "Device Name", your environment is ready.

**Troubleshooting:** If `Number of platforms: 0`:
1.  Ensure you have permissions for the render device:
    ```bash
    sudo usermod -aG render $USER
    sudo usermod -aG video $USER
    ```
    Log out and log back in for changes to take effect.
2.  For very old processors (pre-Broadwell), the `beignet-opencl-icd` package might be needed (not recommended for this course).

---

## 2. Docker Setup (Optional but Recommended)

> **Docker support is coming soon.** A `Dockerfile` with full GPU passthrough will be added in a future update.

The commands below verify GPU passthrough works in a temporary container. **This container is temporary — it only verifies GPU passthrough. Development itself happens on the host or in a persistent container image.**

### Step 1: Install Docker

```bash
# Official Docker installation script
curl -fsSL https://get.docker.com -o get-docker.sh
sudo sh get-docker.sh

# Add your user to the docker group (to avoid using sudo)
sudo usermod -aG docker $USER
# Log out and log back in
```

### Step 2: Verify GPU Passthrough

For Intel GPUs, simply pass the `/dev/dri` device to the container.

```bash
docker run --rm -it   --device /dev/dri   --group-add video   --group-add render   ubuntu:24.04   bash -c "apt update && apt install -y intel-opencl-icd clinfo && clinfo"
```

If `clinfo` inside the container sees the GPU, passthrough is working.

---

## 3. Alternative Hardware Setup

### NVIDIA GPU (Ubuntu 24.04)
Requires proprietary NVIDIA drivers and the NVIDIA Container Toolkit for Docker.

1.  **Drivers:**
    ```bash
    ubuntu-drivers devices        # see recommended driver
    sudo ubuntu-drivers autoinstall
    sudo reboot
    ```
2.  **OpenCL:** The NVIDIA driver package includes the ICD. Verify with `clinfo`.
3.  **Docker:** Install the `nvidia-container-toolkit` and run containers with the `--gpus all` flag.

**If NVIDIA is not visible in clinfo check power profiles and enable performance mode**

### AMD GPU (ROCm)
For Ubuntu 24.04, use the ROCm stack (version 6.x+ is recommended).

1.  **Installation:** Follow the official AMD ROCm documentation for Ubuntu 24.04. This typically involves adding the `amdgpu` repository.
2.  **ICD:** Install the `rocm-opencl-runtime` package.
3.  **Groups:** Add your user to the `render` and `video` groups.

#### AMD — Mesa OpenCL (Quick Start)

The Mesa OpenCL ICD provides a quick way to get OpenCL running on AMD hardware without the full ROCm stack. Install it with:

```bash
sudo apt update
sudo apt install ocl-icd-libopencl1 mesa-opencl-icd ocl-icd-opencl-dev clinfo
```

*   `ocl-icd-libopencl1`: ICD Loader — dispatches OpenCL API calls to the correct vendor driver at runtime.
*   `ocl-icd-opencl-dev`: compile-time dev files (headers + link stub) — required when building OpenCL programs.

---

## 4. Build Tools

To compile the projects in this course, you will need:

```bash
sudo apt install build-essential cmake git ocl-icd-opencl-dev opencl-headers
```

> **Note:** `ocl-icd-opencl-dev` is a superset of `opencl-headers` — it includes the Khronos headers plus the ICD link stub. `opencl-headers` installs Khronos headers only (no link stub).

Verification:
```bash
cmake --version  # Expected >= 3.18
g++ --version    # Expected C++17 support
```

Verify that the OpenCL C++ headers were installed correctly:

```bash
ls /usr/include/CL/
```
You should see `cl.hpp`, `opencl.hpp`, and related headers listed. If the directory is empty or missing, `opencl-headers` was not installed correctly. If CMake < 3.18, install a newer version via `pip install cmake` or the [Kitware APT repository](https://apt.kitware.com/).

---

[← Back to README](../README.md)
