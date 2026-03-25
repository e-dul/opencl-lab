cmake_minimum_required(VERSION 3.18)
project(OpenVINOGPU CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)

# WHY add_definitions here: opencl_utils.hpp has #ifndef guards so these can
# also be passed via compiler flags; common.cmake adds OpenCL but not these macros.
add_definitions(-DCL_HPP_ENABLE_EXCEPTIONS -DCL_HPP_TARGET_OPENCL_VERSION=120)

# OpenVINO must be found before common.cmake so OpenCL can be shared between them.
find_package(OpenVINO REQUIRED)

# Module is 2 levels deep: 02_Multimedia/A3_2_OpenVINO_GPU/
include(${CMAKE_CURRENT_SOURCE_DIR}/../../common/common.cmake)

add_executable(openvino_gpu main.cpp)

# opencl_lab_target wires OpenCL::OpenCL, common headers, CLI11, and stb includes.
opencl_lab_target(openvino_gpu)

target_link_libraries(openvino_gpu PRIVATE openvino::runtime)

# POST_BUILD: symlink kernels/ next to the binary so runtime load_kernel_source() works.
symlink_kernels(openvino_gpu)
symlink_assets(openvino_gpu)
