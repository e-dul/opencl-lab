cmake_minimum_required(VERSION 3.18)
project(YUVPipeline CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(OpenCV REQUIRED)
add_definitions(-DCL_HPP_ENABLE_EXCEPTIONS -DCL_HPP_TARGET_OPENCL_VERSION=120)

# Module is 2 levels deep: 02_Multimedia/A2_YUV_Pipeline/
include(${CMAKE_CURRENT_SOURCE_DIR}/../../common/common.cmake)

add_executable(yuv_pipeline main.cpp)
opencl_lab_target(yuv_pipeline)

target_link_libraries(yuv_pipeline PRIVATE ${OpenCV_LIBS})
target_include_directories(yuv_pipeline PRIVATE ${OpenCV_INCLUDE_DIRS})

copy_kernels(yuv_pipeline)
symlink_assets(yuv_pipeline)
