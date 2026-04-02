cmake_minimum_required(VERSION 3.18)
project(BufferFlags LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../common/common.cmake)

add_executable(buffer_flags src/main.cpp)
opencl_lab_target(buffer_flags)
symlink_kernels(buffer_flags)
symlink_assets(buffer_flags)

message(STATUS "BufferFlags configured. Build then run: ./build/buffer_flags")
