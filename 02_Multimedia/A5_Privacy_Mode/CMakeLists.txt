cmake_minimum_required(VERSION 3.18)
project(privacy_mode CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)

# WHY add_definitions here: opencl_utils.hpp has #ifndef guards so these can
# also be passed via compiler flags; common.cmake adds OpenCL but not these macros.
# CL_QUEUE_PROFILING_ENABLE is a runtime flag, not a compile macro — it is set
# in the CommandQueue constructor, not here.
add_definitions(-DCL_HPP_ENABLE_EXCEPTIONS -DCL_HPP_TARGET_OPENCL_VERSION=120)

# OpenCV: imgproc for cv::blur, dnn for face detection, highgui for display,
# videoio for webcam capture.
find_package(OpenCV REQUIRED COMPONENTS core imgproc dnn objdetect highgui videoio)

# Module is 2 levels deep: 02_Multimedia/A5_Privacy_Mode/
include(${CMAKE_CURRENT_SOURCE_DIR}/../../common/common.cmake)

add_executable(privacy_mode main.cpp)

# opencl_lab_target wires OpenCL::OpenCL, common headers, CLI11, and stb includes.
opencl_lab_target(privacy_mode)

target_link_libraries(privacy_mode PRIVATE ${OpenCV_LIBS})

# POST_BUILD: copy kernels/ next to the binary so runtime load_kernel_source() works.
copy_kernels(privacy_mode)
