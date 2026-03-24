cmake_minimum_required(VERSION 3.18)
project(SyncAtomics CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# common.cmake is two levels up from 99_Toolbox/SyncAtomics/
include(${CMAKE_CURRENT_SOURCE_DIR}/../../common/common.cmake)

add_executable(sync_atomics main.cpp)

# Wires OpenCL::OpenCL, common headers (common/ + vendor/), CLI11::CLI11
opencl_lab_target(sync_atomics)

# Copies kernels/ next to the binary after build
copy_kernels(sync_atomics)
symlink_assets(sync_atomics)

# Expose common/ and vendor/ for opencl_utils.hpp, ocl_wrapper.hpp
target_include_directories(sync_atomics PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../common
    ${CMAKE_CURRENT_SOURCE_DIR}/../../vendor
)
