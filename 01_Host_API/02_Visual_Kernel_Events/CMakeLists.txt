cmake_minimum_required(VERSION 3.18)
project(VisualKernelEvents LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../common/common.cmake)

add_executable(visual_kernel_events src/main.cpp)
opencl_lab_target(visual_kernel_events)
symlink_kernels(visual_kernel_events)
symlink_assets(visual_kernel_events)

message(STATUS "VisualKernelEvents configured. Build then run: ./build/visual_kernel_events -p")
