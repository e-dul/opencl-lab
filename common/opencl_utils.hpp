#pragma once

// OpenCL version targeting (must be defined before including opencl.hpp)
// WHY #ifndef: CMakeLists.txt may pass -DCL_HPP_* via add_definitions(); guards
// prevent "macro redefined" warnings when both the compiler flag and this header
// define the same macro.
#ifndef CL_HPP_TARGET_OPENCL_VERSION
#define CL_HPP_TARGET_OPENCL_VERSION  120
#endif
#ifndef CL_HPP_MINIMUM_OPENCL_VERSION
#define CL_HPP_MINIMUM_OPENCL_VERSION 120
#endif
#ifndef CL_HPP_ENABLE_EXCEPTIONS
#define CL_HPP_ENABLE_EXCEPTIONS
#endif

#include <CL/opencl.hpp>

#include <filesystem>
#include <fstream>
#include <iostream>
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

// ---------------------------------------------------------------------------
// round_up — round n up to the nearest multiple of `multiple`.
//
// WHY: OpenCL NDRange global work size must be an exact multiple of local
// work size when both are specified. We pad to the next tile boundary and
// guard excess work-items inside the kernel (gid >= width/height).
// ---------------------------------------------------------------------------
inline size_t round_up(size_t n, size_t multiple) {
    return ((n + multiple - 1) / multiple) * multiple;
}

// ---------------------------------------------------------------------------
// get_binary_dir — return the directory of the running executable.
//
// WHY /proc/self/exe: the binary may be invoked from any working directory.
// cmake's copy_kernels() places kernel .cl files next to the executable, so
// resolving the exe path is the only reliable way to find them at runtime.
// Linux-only.
// ---------------------------------------------------------------------------
inline std::filesystem::path get_binary_dir() {
    return std::filesystem::canonical("/proc/self/exe").parent_path();
}

// ---------------------------------------------------------------------------
// build_program — compile a single .cl file and return the cl::Program.
//
// WHY separate function: error reporting (getBuildInfo log) is identical
// across all modules; centralising avoids copy-pasted try/catch blocks.
//
// Parameters:
//   ctx         — OpenCL context
//   dev         — target device (used for build log on failure)
//   kernel_path — absolute path to the .cl source file
//   build_opts  — compiler options string (e.g. "-cl-std=CL1.2")
// ---------------------------------------------------------------------------
inline cl::Program build_program(const cl::Context& ctx,
                                  const cl::Device&  dev,
                                  const std::string& kernel_path,
                                  const std::string& build_opts = "-cl-std=CL1.2")
{
    auto src = load_kernel_source(kernel_path);
    cl::Program prog(ctx, src);
    try {
        prog.build({dev}, build_opts.c_str());
    } catch (const cl::Error&) {
        // Surface the full compiler error — essential for kernel debugging.
        std::string log = prog.getBuildInfo<CL_PROGRAM_BUILD_LOG>(dev);
        throw std::runtime_error("Kernel build failed (" + kernel_path + "):\n" + log);
    }
    return prog;
}
