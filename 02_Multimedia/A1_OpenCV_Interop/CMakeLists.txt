cmake_minimum_required(VERSION 3.18)
project(OpenCVInterop CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)


find_package(OpenCV REQUIRED)
add_definitions(-DCL_HPP_ENABLE_EXCEPTIONS -DCL_HPP_TARGET_OPENCL_VERSION=120)

# Module is 2 levels deep: 02_Multimedia/A1_OpenCV_Interop/
include(${CMAKE_CURRENT_SOURCE_DIR}/../../common/common.cmake)

add_executable(opencv_interop main.cpp)
opencl_lab_target(opencv_interop)

target_link_libraries(opencv_interop PRIVATE ${OpenCV_LIBS})
target_include_directories(opencv_interop PRIVATE ${OpenCV_INCLUDE_DIRS})
symlink_assets(opencv_interop)

# No kernel copy needed — pure host-side benchmark, no .cl files
