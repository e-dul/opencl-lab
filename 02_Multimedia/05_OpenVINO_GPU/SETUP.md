# A3_2 OpenVINO + OpenCL Setup — Ubuntu 24.04 / Intel Iris Xe

## 1. Install Packages

```bash
sudo add-apt-repository universe
sudo apt update
sudo apt install -y \
    build-essential cmake git \
    intel-opencl-icd \
    ocl-icd-libopencl1 \
    ocl-icd-opencl-dev \
    clinfo
```

### Add Intel APT repository and install OpenVINO

```bash
wget -qO- https://apt.repos.intel.com/intel-gpg-keys/GPG-PUB-KEY-INTEL-SW-PRODUCTS.PUB \
    | gpg --dearmor \
    | sudo tee /usr/share/keyrings/intel-sw-products.gpg > /dev/null

echo "deb [signed-by=/usr/share/keyrings/intel-sw-products.gpg] https://apt.repos.intel.com/openvino/2024 ubuntu24 main" \
    | sudo tee /etc/apt/sources.list.d/intel-openvino.list

sudo apt update
sudo apt install -y libopenvino-dev-2024.6.0 libopenvino-intel-cpu-plugin-2024.6.0 libopenvino-intel-gpu-plugin-2024.6.0

```

---

## 2. Permissions

```bash
sudo usermod -a -G render $USER
sudo usermod -a -G video $USER
```
Apply immediately in current shell (temporary):

```bash
newgrp render
```

Full logout/login required for all sessions to pick it up

```bash
clinfo | grep -E "Platform Name|Device Name"
# Expected:
#   Platform Name   Intel(R) OpenCL HD Graphics
#   Device Name     Intel(R) Iris(R) Xe Graphics
```

---

## 3. Sanity Check Project

### `main.cpp`

```cpp
#include <openvino/openvino.hpp>
#include <openvino/runtime/intel_gpu/ocl/ocl.hpp>
#define CL_HPP_TARGET_OPENCL_VERSION 120
#include <CL/opencl.hpp>
#include <iostream>

int main() {
    ov::Core core;
    std::cout << "OpenVINO devices:\n";
    for (auto& d : core.get_available_devices())
        std::cout << "  " << d << "\n";

    std::vector<cl::Platform> platforms;
    cl::Platform::get(&platforms);
    std::cout << "\nOpenCL platforms:\n";
    for (auto& p : platforms) {
        std::cout << "  " << p.getInfo<CL_PLATFORM_NAME>() << "\n";
        std::vector<cl::Device> devices;
        p.getDevices(CL_DEVICE_TYPE_GPU, &devices);
        for (auto& d : devices)
            std::cout << "    -> " << d.getInfo<CL_DEVICE_NAME>() << "\n";
    }
    return 0;
}
```

### `CMakeLists.txt`

```cmake
cmake_minimum_required(VERSION 3.13)
project(hello_openvino)
find_package(OpenVINO REQUIRED)
find_package(OpenCL REQUIRED)
add_executable(hello main.cpp)
target_link_libraries(hello openvino::runtime OpenCL::OpenCL)
target_compile_features(hello PRIVATE cxx_std_17)
```

### Build and run

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
./build/hello
```

### Expected output

```
OpenVINO devices:
  CPU
  GPU

OpenCL platforms:
  Intel(R) OpenCL HD Graphics
    -> Intel(R) Iris(R) Xe Graphics
```

Both stacks reporting the same device confirms the setup is ready for `RemoteContext` cl_mem interop.
---

## 4. A3_2 Module: Build and Run

### Model

The ONNX model is already committed to the repository at `assets/selfie_segmentation.onnx`.
It expects input `[1, 3, 256, 256]` float32 (RGB, normalized [0,1], NCHW layout).
No download required.

### CMake — Finding OpenVINO

`find_package(OpenVINO REQUIRED)` searches standard system paths.
For the Intel APT install above, the config is at:

```
/usr/lib/cmake/openvino2024.6.0/OpenVINOConfig.cmake
```

If cmake reports "Could not find OpenVINO", set:

```bash
export OpenVINO_DIR=/usr/lib/cmake/openvino2024.6.0
```

### Build

```bash
cd 02_Multimedia/05_OpenVINO_GPU
cmake -B build && cmake --build build
```

### Run

```bash
# Intel iGPU required — GPU env var selects the OpenCL platform
GPU=INTEL ./build/openvino_gpu --input assets/sample_1080p.bmp

# Custom model or threshold
GPU=INTEL ./build/openvino_gpu \
    --input assets/sample_1080p.bmp \
    --model assets/selfie_segmentation.onnx \
    --threshold 0.5

# Help
./build/openvino_gpu --help
```

### Expected output

```
Platform : Intel(R) OpenCL HD Graphics  [GPU=INTEL]
Device   : Intel(R) Iris(R) Xe Graphics
Saved: output_blurred.bmp
Saved: output_mask.bmp

[A3_2 OpenVINO GPU]
Inference  (wall-clock): XX.XXX ms
Blur kernel (cl::Event): XX.XXX ms
```

### Output files

| File | Description |
|------|-------------|
| `output_mask.bmp` | Thresholded segmentation mask — white=person, black=background |
| `output_blurred.bmp` | Bokeh blur — background blurred, foreground sharp |

### Scope / Hardware

Intel iGPU only. On systems without an Intel GPU the binary prints a descriptive
message and exits with code 0 — this is expected behaviour, not a crash.

---

[Back to Multimedia.md](../Multimedia.md)
