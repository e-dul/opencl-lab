cmake_minimum_required(VERSION 3.18)
project(B3_Ray_Tracer_BVH CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)

# WHY add_definitions: opencl_utils.hpp guards these with #ifndef so no
# double-definition warnings; explicit flags keep IDE tooling aware.
add_definitions(-DCL_HPP_ENABLE_EXCEPTIONS -DCL_HPP_TARGET_OPENCL_VERSION=120)

# Module is 3 levels deep: 02_Projects/B_Graphics_HPC/B3_Ray_Tracer_BVH/
# common.cmake provides OpenCL, CLI11, stb, and opencl_lab_target() / copy_kernels().
include(${CMAKE_CURRENT_SOURCE_DIR}/../../../common/common.cmake)

add_executable(b3_ray_tracer_bvh main.cpp)

opencl_lab_target(b3_ray_tracer_bvh)

# ── tinyobjloader ─────────────────────────────────────────────────────────────
opencl_lab_fetch_tinyobjloader(b3_ray_tracer_bvh)

# ── Optional GL interop ───────────────────────────────────────────────────────
opencl_lab_optional_gl_interop(b3_ray_tracer_bvh)

copy_kernels(b3_ray_tracer_bvh)
