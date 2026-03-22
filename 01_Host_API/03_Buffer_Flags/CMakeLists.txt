cmake_minimum_required(VERSION 3.18)
project(BuffersLayout LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../common/common.cmake)

add_executable(buffers_layout_demo src/main.cpp)
opencl_lab_target(buffers_layout_demo)
copy_kernels(buffers_layout_demo)
symlink_assets(buffers_layout_demo)

message(STATUS "BuffersLayout configured. Build then run: ./build/buffers_layout_demo")
