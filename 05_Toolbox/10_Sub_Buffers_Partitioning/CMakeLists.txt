cmake_minimum_required(VERSION 3.18)
project(SubBuffersPartitioning CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# WHY ../../common: this module sits 2 levels deep (05_Toolbox/10_Sub_Buffers_Partitioning/).
include(${CMAKE_CURRENT_SOURCE_DIR}/../../common/common.cmake)

add_executable(sub_buffers main.cpp)

# Wire include dirs (stb, common, vendor) and link libs (OpenCL, CLI11, common).
opencl_lab_target(sub_buffers)

# Symlink kernels/ next to the binary so .cl files are editable without rebuild.
symlink_kernels(sub_buffers)
