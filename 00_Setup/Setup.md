# Module 0: Setup & Fundamentals

**Target OS:** Ubuntu 24.04 LTS (Noble Numbat)
**Hardware Focus:** Intel Integrated Graphics (Default), NVIDIA/AMD (Optional)

---

## 1. Intel Integrated Graphics (Quick Start)

For most laptops with Intel CPUs (6th Gen "Skylake" and newer), OpenCL drivers are available directly from the official Ubuntu repositories.

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

Using Docker isolates your environment and ensures everything works regardless of your host system version.

# TODO: Dockerfile

### Step 1: Install Docker

```bash
# Official Docker installation script
curl -fsSL https://get.docker.com -o get-docker.sh
sudo sh get-docker.sh

# Add your user to the docker group (to avoid using sudo)
sudo usermod -aG docker $USER
# Log out and log back in
```

### Step 2: Run a Container with GPU Access

For Intel GPUs, simply pass the `/dev/dri` device to the container.

```bash
docker run --rm -it   --device /dev/dri   --group-add video   --group-add render   ubuntu:24.04   bash -c "apt update && apt install -y intel-opencl-icd clinfo && clinfo"
```

If `clinfo` inside the container sees the GPU, your setup is correct.

---

## 3. Alternative Hardware Setup

### NVIDIA GPU (Ubuntu 24.04)
Requires proprietary NVIDIA drivers and the NVIDIA Container Toolkit for Docker.

1.  **Drivers:**
    ```bash
    sudo apt install nvidia-driver-550
    sudo reboot
    ```
2.  **OpenCL:** The NVIDIA driver package includes the ICD. Verify with `clinfo`.
3.  **Docker:** Install the `nvidia-container-toolkit` and run containers with the `--gpus all` flag.

### AMD GPU (ROCm)
For Ubuntu 24.04, use the ROCm stack (version 6.x+ is recommended).

1.  **Installation:** Follow the official AMD ROCm documentation for Ubuntu 24.04. This typically involves adding the `amdgpu` repository.
2.  **ICD:** Install the `rocm-opencl-runtime` package.
3.  **Groups:** Add your user to the `render` and `video` groups.

#### this worked

```bash
sudo apt update
sudo apt install ocl-icd-libopencl1 mesa-opencl-icd ocl-icd-opencl-dev clinfo
```

---

## 4. Build Tools

To compile the projects in this course, you will need:

```bash
sudo apt install build-essential cmake git ocl-icd-opencl-dev opencl-headers
```

Verification:
```bash
cmake --version  # Expected >= 3.18
g++ --version    # Expected C++17 support
```
