cmake_minimum_required(VERSION 3.18)
project(B2_Ray_Tracer_Basic CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)

# WHY add_definitions: opencl_utils.hpp guards these with #ifndef, so no
# double-definition warnings; explicit flags keep IDE tooling aware.
add_definitions(-DCL_HPP_ENABLE_EXCEPTIONS -DCL_HPP_TARGET_OPENCL_VERSION=120)

# Module is 3 levels deep: 02_Projects/B_Graphics_HPC/B2_Ray_Tracer_Basic/
# common.cmake already calls find_package(OpenCL REQUIRED) and FetchContent for CLI11.
include(${CMAKE_CURRENT_SOURCE_DIR}/../../../common/common.cmake)

add_executable(b2_ray_tracer main.cpp)

opencl_lab_target(b2_ray_tracer)

# ── Optional GL interop ───────────────────────────────────────────────────────
# WHY find_package optional: GL interop requires both GLFW and OpenGL; if either
# is absent the binary still works in headless (--output) mode. User can also
# force-disable with -DNO_GL_INTEROP=ON for CI or CPU-only environments.
option(NO_GL_INTEROP "Disable OpenGL interop (always headless)" OFF)

if(NOT NO_GL_INTEROP)
    find_package(glfw3 QUIET)
    find_package(OpenGL QUIET)

    if(glfw3_FOUND AND OpenGL_FOUND)
        target_link_libraries(b2_ray_tracer PRIVATE glfw OpenGL::GL)
        message(STATUS "GL interop: enabled (GLFW + OpenGL found)")
    else()
        message(WARNING "glfw3/OpenGL not found — building without GL interop (NO_GL_INTEROP). Install libglfw3-dev to enable live mode.")
        target_compile_definitions(b2_ray_tracer PRIVATE NO_GL_INTEROP)
    endif()
else()
    message(STATUS "GL interop: disabled by -DNO_GL_INTEROP=ON")
    target_compile_definitions(b2_ray_tracer PRIVATE NO_GL_INTEROP)
endif()

copy_kernels(b2_ray_tracer)
