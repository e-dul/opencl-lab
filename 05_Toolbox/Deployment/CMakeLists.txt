cmake_minimum_required(VERSION 3.18)
project(deployment_demo CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# common.cmake provides: find_package(OpenCL), FetchContent for stb + CLI11,
# opencl_lab_target(), copy_kernels() helpers.
# Depth note: this module is 2 levels deep (04_Addons/4_3_Deployment/).
include(${CMAKE_CURRENT_SOURCE_DIR}/../../common/common.cmake)

add_executable(deployment_demo src/main.cpp)
opencl_lab_target(deployment_demo)

# POST_BUILD: copy kernels/ next to the binary so load_kernel_source() finds them.
copy_kernels(deployment_demo)
symlink_assets(deployment_demo)

# ── Install rules ─────────────────────────────────────────────────────────────
# Binary → <prefix>/bin/deployment_demo
install(TARGETS deployment_demo RUNTIME DESTINATION bin)

# Kernels → <prefix>/share/deployment_demo/kernels/
# WHY DIRECTORY (not FILES): preserves future additions without CMake edits.
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/kernels/
        DESTINATION share/deployment_demo/kernels)

message(STATUS "deployment_demo configured. Build: cmake --build build")
