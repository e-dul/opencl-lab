cmake_minimum_required(VERSION 3.18)
project(B3_Ray_Tracer_BVH_Dynamic CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)

# WHY add_definitions: opencl_utils.hpp guards these with #ifndef so no
# double-definition warnings; explicit flags keep IDE tooling aware.
add_definitions(-DCL_HPP_ENABLE_EXCEPTIONS -DCL_HPP_TARGET_OPENCL_VERSION=120)

# Module is 3 levels deep: 02_Projects/B_Graphics_HPC/B3_Ray_Tracer_BVH_Dynamic/
# common.cmake provides OpenCL, CLI11, stb, and opencl_lab_target() / copy_kernels().
include(${CMAKE_CURRENT_SOURCE_DIR}/../../../common/common.cmake)

# ── tinyobjloader ─────────────────────────────────────────────────────────────
# WHY FetchContent: keeps the module self-contained; no system install required.
FetchContent_Declare(
    tinyobjloader
    GIT_REPOSITORY https://github.com/tinyobjloader/tinyobjloader.git
    GIT_TAG        v2.0.0rc13
)
FetchContent_MakeAvailable(tinyobjloader)

add_executable(b3_ray_tracer_dynamic main.cpp)

opencl_lab_target(b3_ray_tracer_dynamic)
target_link_libraries(b3_ray_tracer_dynamic PRIVATE tinyobjloader)

# Headless-only: no GL interop needed for benchmark mode
target_compile_definitions(b3_ray_tracer_dynamic PRIVATE NO_GL_INTEROP)

copy_kernels(b3_ray_tracer_dynamic)
