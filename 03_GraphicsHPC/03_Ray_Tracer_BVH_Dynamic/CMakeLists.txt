cmake_minimum_required(VERSION 3.18)
project(RayTracerBVHDynamic CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)

# WHY add_definitions: opencl_utils.hpp guards these with #ifndef so no
# double-definition warnings; explicit flags keep IDE tooling aware.
add_definitions(-DCL_HPP_ENABLE_EXCEPTIONS -DCL_HPP_TARGET_OPENCL_VERSION=120)

# Module is 2 levels deep: 03_GraphicsHPC/B3_Ray_Tracer_BVH_Dynamic/
# common.cmake provides OpenCL, CLI11, stb, and opencl_lab_target() / symlink_kernels().
include(${CMAKE_CURRENT_SOURCE_DIR}/../../common/common.cmake)

add_executable(ray_tracer_bvh_dynamic main.cpp)

opencl_lab_target(ray_tracer_bvh_dynamic)

# ── tinyobjloader ─────────────────────────────────────────────────────────────
opencl_lab_fetch_tinyobjloader(ray_tracer_bvh_dynamic)

# Headless-only: no GL interop needed for benchmark mode
target_compile_definitions(ray_tracer_bvh_dynamic PRIVATE NO_GL_INTEROP)

symlink_kernels(ray_tracer_bvh_dynamic)
symlink_assets(ray_tracer_bvh_dynamic)
