cmake_minimum_required(VERSION 3.18)
project(WorkGroupSizing CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Pull in stb, CLI11 and the common INTERFACE target.
# WHY ../../common: this module sits 2 levels deep (99_Toolbox/WorkGroupSizing/).
include(${CMAKE_CURRENT_SOURCE_DIR}/../../common/common.cmake)

add_executable(workgroup_sizing main.cpp)

# Wire include dirs (stb, common, vendor) and link libs (OpenCL, CLI11, common).
opencl_lab_target(workgroup_sizing)

# Copy kernels/ next to the binary at build time so the executable can locate
# them via a relative path regardless of where cmake --build is invoked.
copy_kernels(workgroup_sizing)
symlink_assets(workgroup_sizing)
