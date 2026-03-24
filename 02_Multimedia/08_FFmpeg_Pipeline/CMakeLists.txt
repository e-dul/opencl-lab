cmake_minimum_required(VERSION 3.18)
project(FFmpegPipeline CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Pull in CLI11, stb, and OpenCL via the shared helper (idempotent).
include(${CMAKE_CURRENT_SOURCE_DIR}/../../common/common.cmake)

# FFmpeg via pkg-config (libswscale needed for YUV↔RGBA conversion).
find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG REQUIRED libavcodec libavformat libavutil libswscale)

add_executable(ffmpeg_pipeline main.cpp)

# WHY opencl_lab_target: provides OpenCL::OpenCL, CLI11::CLI11, and the common/vendor
# include paths transitively — no need to list them manually here.
opencl_lab_target(ffmpeg_pipeline)

target_include_directories(ffmpeg_pipeline PRIVATE
    ${FFMPEG_INCLUDE_DIRS}
)

# WHY explicit defines: opencl_lab_target does not set CL_HPP_* macros;
# these must be set before opencl_utils.hpp is included to enable exceptions
# and pin the target version.
target_compile_definitions(ffmpeg_pipeline PRIVATE
    CL_HPP_ENABLE_EXCEPTIONS
    CL_HPP_TARGET_OPENCL_VERSION=120
    CL_HPP_MINIMUM_OPENCL_VERSION=120
)

target_link_directories(ffmpeg_pipeline PRIVATE ${FFMPEG_LIBRARY_DIRS})

target_link_libraries(ffmpeg_pipeline PRIVATE
    ${FFMPEG_LIBRARIES}
    va          # libva — VASurfaceID, VADisplay (cl_intel_va_api_media_sharing)
)

target_compile_options(ffmpeg_pipeline PRIVATE ${FFMPEG_CFLAGS_OTHER})

# Copy kernels/ next to binary so get_binary_dir() / "kernels" resolves at runtime.
copy_kernels(ffmpeg_pipeline)
symlink_assets(ffmpeg_pipeline)

# Warn if the default sample asset is absent — build still succeeds.
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../assets/sample.mp4")
    message(WARNING "assets/sample.mp4 not found — provide via --input at runtime")
endif()
