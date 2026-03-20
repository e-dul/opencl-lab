cmake_minimum_required(VERSION 3.18)
project(ffmpeg_opencl_transcoder CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Pull in CLI11, stb, and OpenCL via the shared helper (idempotent).
include(${CMAKE_CURRENT_SOURCE_DIR}/../../common/common.cmake)

# OpenCL is already included by common.cmake; listed explicitly per spec.
find_package(OpenCL REQUIRED)

# FFmpeg via pkg-config (libswscale needed for YUV↔RGBA conversion).
find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG REQUIRED libavcodec libavformat libavutil libswscale)

add_executable(ffmpeg_opencl_transcoder main.cpp)

target_include_directories(ffmpeg_opencl_transcoder PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../common
    ${CMAKE_CURRENT_SOURCE_DIR}/../../vendor
    ${stb_SOURCE_DIR}
    ${FFMPEG_INCLUDE_DIRS}
)

target_compile_definitions(ffmpeg_opencl_transcoder PRIVATE
    CL_HPP_ENABLE_EXCEPTIONS
    CL_HPP_TARGET_OPENCL_VERSION=120
    CL_HPP_MINIMUM_OPENCL_VERSION=120
)

target_link_directories(ffmpeg_opencl_transcoder PRIVATE ${FFMPEG_LIBRARY_DIRS})

target_link_libraries(ffmpeg_opencl_transcoder PRIVATE
    OpenCL::OpenCL
    CLI11::CLI11
    ${FFMPEG_LIBRARIES}
    va          # libva — VASurfaceID, VADisplay (cl_intel_va_api_media_sharing)
)

target_compile_options(ffmpeg_opencl_transcoder PRIVATE ${FFMPEG_CFLAGS_OTHER})

# Copy kernels/ next to binary so load path "kernels/filter.cl" resolves at runtime.
copy_kernels(ffmpeg_opencl_transcoder)

# Warn if the default sample asset is absent — build still succeeds.
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../assets/sample.mp4")
    message(WARNING "assets/sample.mp4 not found — provide via --input at runtime")
endif()
