#pragma once

#include "opencl_utils.hpp"  // pulls in opencl.hpp + CL_HPP_* macros

#include <iostream>
#include <stdexcept>
#include <vector>

// Aggregates the four objects every OpenCL program needs.
struct OclContext {
    cl::Platform     platform;
    cl::Device       device;
    cl::Context      context;
    cl::CommandQueue queue;
};

// GPU-first device selection with CPU fallback.
// Prints selected platform and device to stdout.
inline OclContext create_context() {
    std::vector<cl::Platform> platforms;
    cl::Platform::get(&platforms);
    if (platforms.empty()) {
        throw std::runtime_error("No OpenCL platforms found. Check your drivers.");
    }

    // Pick first platform with a GPU; if none exists, take first platform's CPU.
    cl::Platform selected_platform;
    cl::Device   selected_device;
    bool         found = false;

    for (auto& p : platforms) {
        std::vector<cl::Device> gpus;
        p.getDevices(CL_DEVICE_TYPE_GPU, &gpus);
        if (!gpus.empty()) {
            selected_platform = p;
            selected_device   = gpus.front();
            found             = true;
            break;
        }
    }

    if (!found) {
        // CPU fallback
        for (auto& p : platforms) {
            std::vector<cl::Device> cpus;
            p.getDevices(CL_DEVICE_TYPE_CPU, &cpus);
            if (!cpus.empty()) {
                selected_platform = p;
                selected_device   = cpus.front();
                found             = true;
                break;
            }
        }
    }

    if (!found) {
        throw std::runtime_error("No usable OpenCL device found.");
    }

    std::cout << "Platform : " << selected_platform.getInfo<CL_PLATFORM_NAME>() << "\n";
    std::cout << "Device   : " << selected_device.getInfo<CL_DEVICE_NAME>()   << "\n";

    cl::Context      ctx(selected_device);
    cl::CommandQueue q(ctx, selected_device);

    return OclContext{selected_platform, selected_device, ctx, q};
}
