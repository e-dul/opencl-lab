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

# ── tinyobjloader ─────────────────────────────────────────────────────────────
# WHY FetchContent: keeps the module self-contained; no system install required.
FetchContent_Declare(
    tinyobjloader
    GIT_REPOSITORY https://github.com/tinyobjloader/tinyobjloader.git
    GIT_TAG        v2.0.0rc13
)
FetchContent_MakeAvailable(tinyobjloader)

add_executable(b3_ray_tracer_bvh main.cpp)

opencl_lab_target(b3_ray_tracer_bvh)
target_link_libraries(b3_ray_tracer_bvh PRIVATE tinyobjloader)

# ── Optional GL interop ───────────────────────────────────────────────────────
# Default OFF → auto-detect. User can force with -DNO_GL_INTEROP=ON.
# CMake emits WARNING and sets NO_GL_INTEROP when glfw3/OpenGL are absent.
option(NO_GL_INTEROP "Disable OpenGL interop (headless only)" OFF)

if(NOT NO_GL_INTEROP)
    find_package(glfw3 QUIET)
    find_package(OpenGL QUIET)

    if(glfw3_FOUND AND OpenGL_FOUND)
        target_link_libraries(b3_ray_tracer_bvh PRIVATE glfw OpenGL::GL)
        message(STATUS "GL interop: enabled (GLFW + OpenGL found)")

        # WHY conditional EGL: Intel NEO (intel-opencl-icd) only supports
        # cl_khr_gl_sharing for EGL-backed GL contexts, not GLX. Detecting EGL
        # at configure time lets us compile the EGL fallback only when available.
        find_package(OpenGL COMPONENTS EGL)
        if(OpenGL_EGL_FOUND)
            target_link_libraries(b3_ray_tracer_bvh PRIVATE OpenGL::EGL)
            target_compile_definitions(b3_ray_tracer_bvh PRIVATE HAS_EGL)
            message(STATUS "EGL found — Intel GL interop fallback enabled")
        else()
            message(STATUS "EGL not found — Intel GL interop fallback disabled")
        endif()
    else()
        message(WARNING "glfw3/OpenGL not found — building without GL interop (NO_GL_INTEROP). Install libglfw3-dev to enable live mode.")
        target_compile_definitions(b3_ray_tracer_bvh PRIVATE NO_GL_INTEROP)
    endif()
else()
    message(STATUS "GL interop: disabled by -DNO_GL_INTEROP=ON")
    target_compile_definitions(b3_ray_tracer_bvh PRIVATE NO_GL_INTEROP)
endif()

copy_kernels(b3_ray_tracer_bvh)
