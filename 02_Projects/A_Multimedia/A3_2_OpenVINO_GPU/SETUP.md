# A3_2 OpenVINO + OpenCL Setup — Ubuntu 24.04 / Intel Iris Xe

**DRAFT - NOT TESTED**

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
sudo apt install -y libopenvino-dev
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
#define CL_HPP_TARGET_OPENCL_VERSION 300
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