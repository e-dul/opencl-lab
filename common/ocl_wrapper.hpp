#pragma once

#include "opencl_utils.hpp"  // pulls in opencl.hpp + CL_HPP_* macros

#include <algorithm>
#include <cstdlib>
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
// Set GPU=NVIDIA / GPU=AMD / GPU=INTEL to pin a specific vendor.
// Prints selected platform and device to stdout.
inline OclContext create_context() {
    std::vector<cl::Platform> platforms;
    cl::Platform::get(&platforms);
    if (platforms.empty()) {
        throw std::runtime_error("No OpenCL platforms found. Check your drivers.");
    }

    // Read optional vendor hint from environment.
    const char* gpu_hint_raw = std::getenv("GPU");
    std::string gpu_hint     = gpu_hint_raw ? gpu_hint_raw : "";
    std::transform(gpu_hint.begin(), gpu_hint.end(), gpu_hint.begin(), ::toupper);

    cl::Platform selected_platform;
    cl::Device   selected_device;
    bool         found = false;

    if (!gpu_hint.empty()) {
        // Vendor-pinned selection: case-insensitive substring match on
        // CL_PLATFORM_VENDOR *or* CL_DEVICE_VENDOR of the first GPU.
        // The device check handles open-source stacks (e.g. Mesa/rusticl)
        // where CL_PLATFORM_VENDOR is "Mesa/X.org" but CL_DEVICE_VENDOR is "AMD".
        auto contains_hint = [&](const std::string& s) {
            std::string upper = s;
            std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);
            return upper.find(gpu_hint) != std::string::npos;
        };

        for (auto& p : platforms) {
            std::vector<cl::Device> gpus;
            p.getDevices(CL_DEVICE_TYPE_GPU, &gpus);
            if (gpus.empty()) continue;

            if (contains_hint(p.getInfo<CL_PLATFORM_VENDOR>()) ||
                contains_hint(gpus.front().getInfo<CL_DEVICE_VENDOR>())) {
                selected_platform = p;
                selected_device   = gpus.front();
                found             = true;
                break;
            }
        }
        if (!found) {
            std::string msg = "GPU vendor '" + std::string(gpu_hint_raw)
                            + "' not found. Available platforms:\n";
            for (auto& p : platforms) {
                msg += "  - " + p.getInfo<CL_PLATFORM_VENDOR>();
                std::vector<cl::Device> gpus;
                p.getDevices(CL_DEVICE_TYPE_GPU, &gpus);
                if (!gpus.empty())
                    msg += " / device: " + gpus.front().getInfo<CL_DEVICE_VENDOR>();
                msg += "\n";
            }
            throw std::runtime_error(msg);
        }
    } else {
        // Default: pick first platform with a GPU; fall back to CPU.
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
    }

    std::cout << "Platform : " << selected_platform.getInfo<CL_PLATFORM_NAME>();
    if (!gpu_hint.empty()) std::cout << "  [GPU=" << gpu_hint_raw << "]";
    std::cout << "\n";
    std::cout << "Device   : " << selected_device.getInfo<CL_DEVICE_NAME>() << "\n";

    cl::Context      ctx(selected_device);
    cl::CommandQueue q(ctx, selected_device);

    return OclContext{selected_platform, selected_device, ctx, q};
}
