cmake_minimum_required(VERSION 3.18)
project(MultiGPUStrategy CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# common.cmake is two levels up from 99_Toolbox/MultiGPU_Strategy/
include(${CMAKE_CURRENT_SOURCE_DIR}/../../common/common.cmake)

add_executable(multi_gpu_strategy main.cpp)

# Wires OpenCL::OpenCL, common headers (common/ + vendor/), CLI11::CLI11
opencl_lab_target(multi_gpu_strategy)

# Copies kernels/ next to the binary after build
symlink_kernels(multi_gpu_strategy)
symlink_assets(multi_gpu_strategy)

# Expose common/ and vendor/ for all translation units in this target
target_include_directories(multi_gpu_strategy PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../common
    ${CMAKE_CURRENT_SOURCE_DIR}/../../vendor
)
