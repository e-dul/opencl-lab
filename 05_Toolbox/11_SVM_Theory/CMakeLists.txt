cmake_minimum_required(VERSION 3.18)
project(SVMTheory CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../common/common.cmake)

add_executable(svm_theory main.cpp)
opencl_lab_target(svm_theory)
symlink_kernels(svm_theory)
symlink_assets(svm_theory)
