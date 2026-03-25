cmake_minimum_required(VERSION 3.18)
project(ThreadDivergence CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Pull in stb, CLI11 and the common INTERFACE target.
# WHY ../../common: this module sits 2 levels deep (99_Toolbox/ThreadDivergence/).
include(${CMAKE_CURRENT_SOURCE_DIR}/../../common/common.cmake)

add_executable(thread_divergence main.cpp)

# Wire include dirs (stb, common, vendor) and link libs (OpenCL, CLI11, common).
opencl_lab_target(thread_divergence)

symlink_kernels(thread_divergence)
symlink_assets(thread_divergence)
