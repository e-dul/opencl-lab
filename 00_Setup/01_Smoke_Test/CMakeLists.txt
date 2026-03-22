cmake_minimum_required(VERSION 3.18)
project(SmokeTest LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find OpenCL
find_package(OpenCL REQUIRED)

# Include directories
# We include the root 'vendor' to find CL/cl.hpp if it's placed there
# include_directories(${CMAKE_SOURCE_DIR}/../../../vendor)

# Executable
add_executable(smoke_test src/main.cpp)

# Link OpenCL
target_link_libraries(smoke_test PRIVATE OpenCL::OpenCL)

# Copy kernel file to build directory
# This ensures ./kernels/vector_add.cl is found when running the binary
file(COPY kernels/vector_add.cl DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/kernels)

message(STATUS "Smoke Test configured. Run 'make' and then './smoke_test' to verify.")
