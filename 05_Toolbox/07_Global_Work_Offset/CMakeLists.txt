cmake_minimum_required(VERSION 3.18)
project(GlobalWorkOffset CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# WHY ../../common: this module sits 2 levels deep (05_Toolbox/GlobalWorkOffset/).
include(${CMAKE_CURRENT_SOURCE_DIR}/../../common/common.cmake)

add_executable(global_work_offset main.cpp)

# Wire include dirs (stb, common, vendor) and link libs (OpenCL, CLI11, common).
opencl_lab_target(global_work_offset)

# Copy kernels/ next to the binary so the executable can locate them via a
# relative path regardless of where cmake --build is invoked.
symlink_kernels(global_work_offset)
