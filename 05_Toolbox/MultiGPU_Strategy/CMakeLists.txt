cmake_minimum_required(VERSION 3.18)
project(MultiGPU_Strategy CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# common.cmake is two levels up from 99_Toolbox/MultiGPU_Strategy/
include(${CMAKE_CURRENT_SOURCE_DIR}/../../common/common.cmake)

add_executable(multigpu_strategy main.cpp)

# Wires OpenCL::OpenCL, common headers (common/ + vendor/), CLI11::CLI11
opencl_lab_target(multigpu_strategy)

# Copies kernels/ next to the binary after build
copy_kernels(multigpu_strategy)

# Expose common/ and vendor/ for all translation units in this target
target_include_directories(multigpu_strategy PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../common
    ${CMAKE_CURRENT_SOURCE_DIR}/../../vendor
)
