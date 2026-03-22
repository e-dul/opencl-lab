/*
 * Applied OpenCL Lab - Module 0: Smoke Test
 * 
 * Goal: Verify OpenCL drivers, C++ compiler, and CMake build system.
 * This file is deliberately self-contained (no 'common' dependencies) 
 * to isolate environment issues.
 */

#define CL_HPP_TARGET_OPENCL_VERSION 120
#define CL_HPP_MINIMUM_OPENCL_VERSION 120
#define CL_HPP_ENABLE_EXCEPTIONS

#include <CL/opencl.hpp>
#include <iostream>
#include <vector>
#include <fstream>
#include <sstream>

// Helper to load kernel source
std::string load_kernel_source(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open kernel file: " + filename);
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

int main() {
    const int N = 1024;
    const size_t bytes = N * sizeof(float);

    try {
        // 1. Get Platforms
        std::vector<cl::Platform> platforms;
        cl::Platform::get(&platforms);
        if (platforms.empty()) {
            std::cerr << "No OpenCL platforms found. Check drivers!" << std::endl;
            return 1;
        }

        auto platform = platforms.front();
        std::cout << "Using Platform: " << platform.getInfo<CL_PLATFORM_NAME>() << std::endl;

        // 2. Get Devices
        std::vector<cl::Device> devices;
        platform.getDevices(CL_DEVICE_TYPE_GPU, &devices);
        if (devices.empty()) {
            std::cout << "No GPU found, trying CPU..." << std::endl;
            platform.getDevices(CL_DEVICE_TYPE_CPU, &devices);
        }
        if (devices.empty()) {
            std::cerr << "No OpenCL devices found!" << std::endl;
            return 1;
        }

        auto device = devices.front();
        std::cout << "Using Device: " << device.getInfo<CL_DEVICE_NAME>() << std::endl;

        // 3. Context & Command Queue
        cl::Context context(device);
        cl::CommandQueue queue(context, device);

        // 4. Host Data
        std::vector<float> host_a(N, 1.0f);
        std::vector<float> host_b(N, 2.0f);
        std::vector<float> host_c(N);

        // 5. Device Buffers
        cl::Buffer buffer_a(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, bytes, host_a.data());
        cl::Buffer buffer_b(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, bytes, host_b.data());
        cl::Buffer buffer_c(context, CL_MEM_WRITE_ONLY, bytes);

        // 6. Load & Build Kernel
        // Assuming kernel file is in "kernels/" relative to binary or explicitly defined
        // For simplicity in CMake, we often copy kernels to build dir.
        // Here we assume it's accessible.
        std::string source_str = load_kernel_source("kernels/vector_add.cl");
        cl::Program::Sources sources;
        sources.push_back({source_str.c_str(), source_str.length()});

        cl::Program program(context, sources);
        if (program.build({device}) != CL_SUCCESS) {
            std::cerr << "Error building: " << program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(device) << std::endl;
            return 1;
        }

        // 7. Execute Kernel
        cl::Kernel kernel(program, "vector_add");
        kernel.setArg(0, buffer_a);
        kernel.setArg(1, buffer_b);
        kernel.setArg(2, buffer_c);
        kernel.setArg(3, N);

        queue.enqueueNDRangeKernel(kernel, cl::NullRange, cl::NDRange(N), cl::NullRange);
        queue.finish();

        // 8. Read Result
        queue.enqueueReadBuffer(buffer_c, CL_TRUE, 0, bytes, host_c.data());

        // 9. Verify
        bool correct = true;
        for (int i = 0; i < N; i++) {
            if (host_c[i] != 3.0f) {
                std::cout << "Error at " << i << ": " << host_c[i] << " != 3.0" << std::endl;
                correct = false;
                break;
            }
        }

        if (correct) {
            std::cout << "✅ SUCCESS: Vector Add (1.0 + 2.0 = 3.0) verified!" << std::endl;
        } else {
            std::cout << "❌ FAILURE: Incorrect results." << std::endl;
        }

    } catch (cl::Error& err) {
        std::cerr << "OpenCL Error: " << err.what() << "(" << err.err() << ")" << std::endl;
        return 1;
    }

    return 0;
}
