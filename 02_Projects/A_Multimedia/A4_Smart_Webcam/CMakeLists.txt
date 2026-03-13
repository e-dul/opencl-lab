cmake_minimum_required(VERSION 3.18)
project(smart_webcam CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)

# WHY add_definitions here: opencl_utils.hpp has #ifndef guards so these can
# also be passed via compiler flags; common.cmake adds OpenCL but not these macros.
add_definitions(-DCL_HPP_ENABLE_EXCEPTIONS -DCL_HPP_TARGET_OPENCL_VERSION=120)

# OpenVINO must be found before common.cmake so OpenCL can be shared between them.
find_package(OpenVINO REQUIRED)

# OpenCV: only for webcam capture (VideoCapture) and display (imshow).
find_package(OpenCV REQUIRED COMPONENTS core highgui videoio)

# Module is 3 levels deep: 02_Projects/A_Multimedia/A4_Smart_Webcam/
include(${CMAKE_CURRENT_SOURCE_DIR}/../../../common/common.cmake)

add_executable(smart_webcam main.cpp)

# opencl_lab_target wires OpenCL::OpenCL, common headers, CLI11, and stb includes.
opencl_lab_target(smart_webcam)

target_link_libraries(smart_webcam PRIVATE openvino::runtime ${OpenCV_LIBS})

# POST_BUILD: copy kernels/ next to the binary so runtime load_kernel_source() works.
copy_kernels(smart_webcam)
