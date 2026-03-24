cmake_minimum_required(VERSION 3.18)
project(OpenCVDNN CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(OpenCV REQUIRED)
add_definitions(-DCL_HPP_ENABLE_EXCEPTIONS -DCL_HPP_TARGET_OPENCL_VERSION=120)

# Module is 2 levels deep: 02_Multimedia/A3_1_OpenCV_DNN/
include(${CMAKE_CURRENT_SOURCE_DIR}/../../common/common.cmake)

add_executable(opencv_dnn main.cpp)
opencl_lab_target(opencv_dnn)

target_link_libraries(opencv_dnn PRIVATE ${OpenCV_LIBS})
target_include_directories(opencv_dnn PRIVATE ${OpenCV_INCLUDE_DIRS})

copy_kernels(opencv_dnn)
symlink_assets(opencv_dnn)
