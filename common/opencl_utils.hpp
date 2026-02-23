#pragma once

// OpenCL version targeting (must be defined before including opencl.hpp)
#define CL_HPP_TARGET_OPENCL_VERSION  120
#define CL_HPP_MINIMUM_OPENCL_VERSION 120
#define CL_HPP_ENABLE_EXCEPTIONS

#include <CL/opencl.hpp>

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

// Throw on non-success CL error codes (use when exceptions are off or for clarity).
// With CL_HPP_ENABLE_EXCEPTIONS most cl:: calls throw automatically; this
// macro covers raw cl_int return values that won't throw on their own.
#define CL_CHECK(err)                                                        \
    do {                                                                     \
        if ((err) != CL_SUCCESS) {                                           \
            throw std::runtime_error(std::string("OpenCL error ") +          \
                                     std::to_string(err) + " at " +          \
                                     __FILE__ + ":" + std::to_string(__LINE__)); \
        }                                                                    \
    } while (0)

// Load an OpenCL kernel source file from disk.
inline std::string load_kernel_source(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open kernel file: " + path);
    }
    std::ostringstream buf;
    buf << file.rdbuf();
    return buf.str();
}

// Convert cl::Event timestamps from nanoseconds to milliseconds.
// WHY / 1e6: getProfilingInfo returns nanoseconds.
// Requires CL_QUEUE_PROFILING_ENABLE on the queue and queue.finish() before calling.
inline double duration_ms(const cl::Event& e) {
    return (e.getProfilingInfo<CL_PROFILING_COMMAND_END>() -
            e.getProfilingInfo<CL_PROFILING_COMMAND_START>()) / 1e6;
}
