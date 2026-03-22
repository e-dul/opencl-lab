cmake_minimum_required(VERSION 3.18)
project(a3_2_openvino_gpu CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)

# WHY add_definitions here: opencl_utils.hpp has #ifndef guards so these can
# also be passed via compiler flags; common.cmake adds OpenCL but not these macros.
add_definitions(-DCL_HPP_ENABLE_EXCEPTIONS -DCL_HPP_TARGET_OPENCL_VERSION=120)

# OpenVINO must be found before common.cmake so OpenCL can be shared between them.
find_package(OpenVINO REQUIRED)

# Module is 2 levels deep: 02_Multimedia/A3_2_OpenVINO_GPU/
include(${CMAKE_CURRENT_SOURCE_DIR}/../../common/common.cmake)

add_executable(a3_2_openvino_gpu main.cpp)

# opencl_lab_target wires OpenCL::OpenCL, common headers, CLI11, and stb includes.
opencl_lab_target(a3_2_openvino_gpu)

target_link_libraries(a3_2_openvino_gpu PRIVATE openvino::runtime)

# POST_BUILD: copy kernels/ next to the binary so runtime load_kernel_source() works.
copy_kernels(a3_2_openvino_gpu)
