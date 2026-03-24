cmake_minimum_required(VERSION 3.18)
project(AsyncMultiThread CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# common.cmake is two levels up from 99_Toolbox/AsyncMultiThread/
include(${CMAKE_CURRENT_SOURCE_DIR}/../../common/common.cmake)

# std::thread support
find_package(Threads REQUIRED)

add_executable(async_multi_thread
    main_dispatch.cpp
    01_BasicSync/main.cpp
    02_AsyncSingle/main.cpp
    03_MultiThreadAsync/main.cpp
)

# Wires OpenCL::OpenCL, common headers (common/ + vendor/), CLI11::CLI11
opencl_lab_target(async_multi_thread)

target_link_libraries(async_multi_thread PRIVATE Threads::Threads)

# Copies kernels/ next to the binary after build
copy_kernels(async_multi_thread)
symlink_assets(async_multi_thread)

# Expose common/ and vendor/ for all translation units in this target
target_include_directories(async_multi_thread PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../common
    ${CMAKE_CURRENT_SOURCE_DIR}/../../vendor
)
