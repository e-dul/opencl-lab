cmake_minimum_required(VERSION 3.18)
project(VisualKernel LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../common/common.cmake)

add_executable(visual_kernel src/main.cpp)
opencl_lab_target(visual_kernel)
symlink_kernels(visual_kernel)
symlink_assets(visual_kernel)

message(STATUS "VisualKernel configured. Build then run: ./visual_kernel --contrast 1.5 --brightness 20")
