cmake_minimum_required(VERSION 3.18)
project(A1_OpenCV_Interop CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)


find_package(OpenCV REQUIRED)
add_definitions(-DCL_HPP_ENABLE_EXCEPTIONS -DCL_HPP_TARGET_OPENCL_VERSION=120)

# Module is 2 levels deep: 02_Multimedia/A1_OpenCV_Interop/
include(${CMAKE_CURRENT_SOURCE_DIR}/../../common/common.cmake)

add_executable(A1_OpenCV_Interop main.cpp)
opencl_lab_target(A1_OpenCV_Interop)

target_link_libraries(A1_OpenCV_Interop PRIVATE ${OpenCV_LIBS})
target_include_directories(A1_OpenCV_Interop PRIVATE ${OpenCV_INCLUDE_DIRS})
symlink_assets(A1_OpenCV_Interop)

# No kernel copy needed — pure host-side benchmark, no .cl files
