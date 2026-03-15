cmake_minimum_required(VERSION 3.18)
project(b4_device_enqueue CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)

# WHY add_definitions: opencl_utils.hpp guards with #ifndef so no double-definition
# warnings; explicit flags keep IDE tooling aware. Target 1.2 for the C++ wrapper,
# device enqueue is guarded at runtime.
add_definitions(-DCL_HPP_ENABLE_EXCEPTIONS -DCL_HPP_TARGET_OPENCL_VERSION=120)

# WHY explicit CL_VERSION_2_0: system OpenCL headers may be 1.2-era and never
# define CL_VERSION_2_0, even when the device runtime supports 2.0. CMake's
# OpenCL_VERSION_MAJOR reflects what the headers expose, so if headers report
# >= 2.0 we define the guard; otherwise the device-enqueue block compiles out.
# The runtime parse_opencl_c_major() check is still the actual gate.
find_package(OpenCL REQUIRED)
if(OpenCL_VERSION_MAJOR GREATER_EQUAL 2)
    add_definitions(-DCL_VERSION_2_0)
    message(STATUS "OpenCL headers >= 2.0 — CL_VERSION_2_0 defined")
else()
    message(STATUS "OpenCL headers < 2.0 (${OpenCL_VERSION_STRING}) — device enqueue block compiled out")
endif()

# Module is 3 levels deep: 02_Projects/B_Graphics_HPC/B4_Device_Enqueue/
include(${CMAKE_CURRENT_SOURCE_DIR}/../../../common/common.cmake)

# ── tinyobjloader ─────────────────────────────────────────────────────────────
FetchContent_Declare(
    tinyobjloader
    GIT_REPOSITORY https://github.com/tinyobjloader/tinyobjloader.git
    GIT_TAG        v2.0.0rc13
)
FetchContent_MakeAvailable(tinyobjloader)

add_executable(b4_device_enqueue main.cpp)

opencl_lab_target(b4_device_enqueue)
target_link_libraries(b4_device_enqueue PRIVATE tinyobjloader)

# ── Optional GL interop ───────────────────────────────────────────────────────
option(NO_GL_INTEROP "Disable OpenGL interop (headless only)" OFF)

if(NOT NO_GL_INTEROP)
    find_package(glfw3 QUIET)
    find_package(OpenGL QUIET)

    if(glfw3_FOUND AND OpenGL_FOUND)
        target_link_libraries(b4_device_enqueue PRIVATE glfw OpenGL::GL)
        message(STATUS "GL interop: enabled (GLFW + OpenGL found)")

        find_package(OpenGL COMPONENTS EGL)
        if(OpenGL_EGL_FOUND)
            target_link_libraries(b4_device_enqueue PRIVATE OpenGL::EGL)
            target_compile_definitions(b4_device_enqueue PRIVATE HAS_EGL)
            message(STATUS "EGL found — Intel GL interop fallback enabled")
        else()
            message(STATUS "EGL not found — Intel GL interop fallback disabled")
        endif()
    else()
        message(WARNING "glfw3/OpenGL not found — building without GL interop (NO_GL_INTEROP). Install libglfw3-dev to enable live mode.")
        target_compile_definitions(b4_device_enqueue PRIVATE NO_GL_INTEROP)
    endif()
else()
    message(STATUS "GL interop: disabled by -DNO_GL_INTEROP=ON")
    target_compile_definitions(b4_device_enqueue PRIVATE NO_GL_INTEROP)
endif()

copy_kernels(b4_device_enqueue)
