cmake_minimum_required(VERSION 3.18)
project(SoftISP CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../common/common.cmake)

add_executable(soft_isp main.cpp)
opencl_lab_target(soft_isp)
symlink_kernels(soft_isp)
symlink_assets(soft_isp)

# Warn if asset missing — not a build error
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../assets/raw_bayer_4k.raw")
  message(WARNING "assets/raw_bayer_4k.raw not found. Binary will fail at runtime if --input is not overridden.")
endif()
