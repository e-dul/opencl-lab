cmake_minimum_required(VERSION 3.18)
project(fast_math CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# common.cmake is two levels up from 99_Toolbox/FastMath/
include(${CMAKE_CURRENT_SOURCE_DIR}/../../common/common.cmake)

add_executable(fast_math main.cpp)

# Wires OpenCL::OpenCL, common headers (common/ + vendor/), CLI11::CLI11
opencl_lab_target(fast_math)

# Copies kernels/ next to the binary after build
copy_kernels(fast_math)
symlink_assets(fast_math)

# Expose common/ and vendor/ for opencl_utils.hpp, ocl_wrapper.hpp
target_include_directories(fast_math PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../common
    ${CMAKE_CURRENT_SOURCE_DIR}/../../vendor
)
