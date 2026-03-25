cmake_minimum_required(VERSION 3.18)
project(Deployment CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# common.cmake provides: find_package(OpenCL), FetchContent for stb + CLI11,
# opencl_lab_target(), symlink_kernels() helpers.
# Depth note: this module is 2 levels deep (05_Toolbox/Deployment/).
include(${CMAKE_CURRENT_SOURCE_DIR}/../../common/common.cmake)

add_executable(deployment src/main.cpp)
opencl_lab_target(deployment)

# POST_BUILD: symlink kernels/ next to the binary so load_kernel_source() finds them.
symlink_kernels(deployment)
symlink_assets(deployment)

# ── Install rules ─────────────────────────────────────────────────────────────
# Binary → <prefix>/bin/deployment
install(TARGETS deployment RUNTIME DESTINATION bin)

# Kernels → <prefix>/share/deployment/kernels/
# WHY DIRECTORY (not FILES): preserves future additions without CMake edits.
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/kernels/
        DESTINATION share/deployment/kernels)

message(STATUS "Deployment configured. Build: cmake --build build")
