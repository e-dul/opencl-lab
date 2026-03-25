cmake_minimum_required(VERSION 3.18)
project(RayTracerBasic CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)

# WHY add_definitions: opencl_utils.hpp guards these with #ifndef, so no
# double-definition warnings; explicit flags keep IDE tooling aware.
add_definitions(-DCL_HPP_ENABLE_EXCEPTIONS -DCL_HPP_TARGET_OPENCL_VERSION=120)

# Module is 2 levels deep: 03_GraphicsHPC/B2_Ray_Tracer_Basic/
# common.cmake already calls find_package(OpenCL REQUIRED) and FetchContent for CLI11.
include(${CMAKE_CURRENT_SOURCE_DIR}/../../common/common.cmake)

add_executable(ray_tracer main.cpp)

opencl_lab_target(ray_tracer)

# ── Optional GL interop ───────────────────────────────────────────────────────
opencl_lab_optional_gl_interop(ray_tracer)

symlink_kernels(ray_tracer)
symlink_assets(ray_tracer)
