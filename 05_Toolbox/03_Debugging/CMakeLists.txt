cmake_minimum_required(VERSION 3.18)
project(Debugging CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Pull in stb, CLI11 and the common INTERFACE target.
# WHY ../../common: this module sits 2 levels deep (99_Toolbox/Debugging/).
include(${CMAKE_CURRENT_SOURCE_DIR}/../../common/common.cmake)

add_executable(debugging main.cpp)

# Wire include dirs (stb, common, vendor) and link libs (OpenCL, CLI11, common).
opencl_lab_target(debugging)

# Copy kernels/ next to the binary at build time so the executable can locate
# them via a relative path regardless of where cmake --build is invoked.
symlink_kernels(debugging)
symlink_assets(debugging)

# Optional: detect Oclgrind for the user.  The build must NOT fail if absent;
# only the runtime verification (oclgrind ./build/debugging) requires it.
find_program(OCLGRIND_EXECUTABLE oclgrind)
if(NOT OCLGRIND_EXECUTABLE)
    message(WARNING
        "oclgrind not found — runtime verification unavailable. "
        "Install with: sudo apt install oclgrind")
else()
    message(STATUS "oclgrind found: ${OCLGRIND_EXECUTABLE}")
endif()
