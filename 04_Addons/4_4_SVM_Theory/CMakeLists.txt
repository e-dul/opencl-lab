cmake_minimum_required(VERSION 3.18)
project(svm_deep_dive CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../common/common.cmake)

add_executable(svm_deep_dive main.cpp)
opencl_lab_target(svm_deep_dive)
copy_kernels(svm_deep_dive)
