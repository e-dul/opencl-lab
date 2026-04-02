cmake_minimum_required(VERSION 3.18)
project(CLBlastMatMul CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)

# WHY add_definitions: opencl_utils.hpp guards these with #ifndef, so no
# double-definition warnings; explicit flags keep IDE tooling aware.
add_definitions(-DCL_HPP_ENABLE_EXCEPTIONS -DCL_HPP_TARGET_OPENCL_VERSION=120)

# Module is 2 levels deep: 06_Bonus/CLBlast_MatMul/
# common.cmake already calls find_package(OpenCL REQUIRED) and FetchContent for CLI11.
include(${CMAKE_CURRENT_SOURCE_DIR}/../../common/common.cmake)

# ── CLBlast ───────────────────────────────────────────────────────────────────
# WHY FetchContent: avoids a system install requirement; users can build
# out-of-the-box. CLBLAST_BUILD_TESTS=OFF cuts build time significantly.
include(FetchContent)
FetchContent_Declare(
    clblast
    GIT_REPOSITORY https://github.com/CNugteren/CLBlast.git
    GIT_TAG        1.6.3
)
set(CLBLAST_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(CLBLAST_BUILD_SAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(clblast)

add_executable(clblast_matmul main.cpp)

opencl_lab_target(clblast_matmul)

target_link_libraries(clblast_matmul PRIVATE clblast)

# WHY symlink_kernels: CLBlast uses its own built-in kernels compiled at runtime;
# this symlink only serves naive_matmul.cl which is the project's custom kernel.
symlink_kernels(clblast_matmul)
symlink_assets(clblast_matmul)
