cmake_minimum_required(VERSION 3.18)
project(b4_device_enqueue CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)

# WHY add_definitions: opencl_utils.hpp guards with #ifndef so no double-definition
# warnings; explicit flags keep IDE tooling aware. Target 1.2 for the C++ wrapper,
# device enqueue is guarded at runtime.
add_definitions(-DCL_HPP_ENABLE_EXCEPTIONS -DCL_HPP_TARGET_OPENCL_VERSION=120)

# WHY explicit CL_VERSION_2_0: system OpenCL headers may be 1.2-era and never
# define CL_VERSION_2_0, even when the device runtime supports 2.0. CMake's
# OpenCL_VERSION_MAJOR reflects what the headers expose, so if headers report
# >= 2.0 we define the guard; otherwise the device-enqueue block compiles out.
# The runtime parse_opencl_c_major() check is still the actual gate.
find_package(OpenCL REQUIRED)
if(OpenCL_VERSION_MAJOR GREATER_EQUAL 2)
    add_definitions(-DCL_VERSION_2_0)
    message(STATUS "OpenCL headers >= 2.0 — CL_VERSION_2_0 defined")
else()
    message(STATUS "OpenCL headers < 2.0 (${OpenCL_VERSION_STRING}) — device enqueue block compiled out")
endif()

# Module is 3 levels deep: 02_Projects/B_Graphics_HPC/B4_Device_Enqueue/
include(${CMAKE_CURRENT_SOURCE_DIR}/../../../common/common.cmake)

add_executable(b4_device_enqueue main.cpp)

opencl_lab_target(b4_device_enqueue)

# ── tinyobjloader ─────────────────────────────────────────────────────────────
opencl_lab_fetch_tinyobjloader(b4_device_enqueue)

# ── Optional GL interop ───────────────────────────────────────────────────────
opencl_lab_optional_gl_interop(b4_device_enqueue)

copy_kernels(b4_device_enqueue)
