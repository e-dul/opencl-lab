cmake_minimum_required(VERSION 3.18)
project(a3_1_opencv_dnn CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(OpenCV REQUIRED)
add_definitions(-DCL_HPP_ENABLE_EXCEPTIONS -DCL_HPP_TARGET_OPENCL_VERSION=120)

# Module is 2 levels deep: 02_Multimedia/A3_1_OpenCV_DNN/
include(${CMAKE_CURRENT_SOURCE_DIR}/../../common/common.cmake)

add_executable(a3_1_opencv_dnn main.cpp)
opencl_lab_target(a3_1_opencv_dnn)

target_link_libraries(a3_1_opencv_dnn PRIVATE ${OpenCV_LIBS})
target_include_directories(a3_1_opencv_dnn PRIVATE ${OpenCV_INCLUDE_DIRS})

copy_kernels(a3_1_opencv_dnn)
