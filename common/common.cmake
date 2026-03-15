# common/common.cmake
# Include from any module: include(${CMAKE_CURRENT_SOURCE_DIR}/../../common/common.cmake)
#
# Provides:
#   - OpenCL + stb dependencies
#   - opencl_lab_target(<target>)  — wires include dirs and link libs
#   - copy_kernels(<target>)       — POST_BUILD copy of kernels/ next to binary
#   - opencl_lab_optional_gl_interop(<target>) — optional GLFW+OpenGL+EGL detection
#   - opencl_lab_fetch_tinyobjloader(<target>) — FetchContent tinyobjloader + link
#
# Depth note: path assumes module is 2 levels deep (01_HostAPI/01_Visual_Kernel/).
# Adjust relative path for modules at other depths.

# ── OpenCL ────────────────────────────────────────────────────────────────────
find_package(OpenCL REQUIRED)

# ── FetchContent dependencies (all idempotent) ────────────────────────────────
include(FetchContent)

FetchContent_Declare(
    stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG        master
)
FetchContent_MakeAvailable(stb)

FetchContent_Declare(
    CLI11
    GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
    GIT_TAG        v2.4.2
)
FetchContent_MakeAvailable(CLI11)

# ── Common INTERFACE target ───────────────────────────────────────────────────
# Guard: root build calls add_subdirectory(common) before modules are added,
# so the target already exists in that context.
if(NOT TARGET common)
    add_subdirectory(${CMAKE_CURRENT_LIST_DIR} ${CMAKE_CURRENT_BINARY_DIR}/common_iface)
endif()

# ── opencl_lab_target(<target>) ───────────────────────────────────────────────
function(opencl_lab_target TARGET_NAME)
    target_include_directories(${TARGET_NAME} PRIVATE
        ${stb_SOURCE_DIR}           # stb_image.h / stb_image_write.h
    )
    target_link_libraries(${TARGET_NAME} PRIVATE
        OpenCL::OpenCL
        common                      # header-only; pulls in common/ and vendor/ paths
        CLI11::CLI11
    )
endfunction()

# ── copy_kernels(<target>) ────────────────────────────────────────────────────
# POST_BUILD: copies <module>/kernels/ → <binary_dir>/kernels/ at build time.
# Uses generator expression so it works with multi-config generators (MSVC/Xcode).
function(copy_kernels TARGET_NAME)
    add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/kernels"
            "$<TARGET_FILE_DIR:${TARGET_NAME}>/kernels"
        COMMENT "Copying kernels for ${TARGET_NAME}"
    )
endfunction()

# ── opencl_lab_optional_gl_interop(<target>) ──────────────────────────────────
# WHY macro (not function): macros share the caller's scope so find_package()
# results (glfw3_FOUND, OpenGL_EGL_FOUND) are visible to the calling CMakeLists,
# which simplifies debugging. The target name is passed explicitly so the macro
# can apply target_compile_definitions to the correct binary.
macro(opencl_lab_optional_gl_interop TARGET_NAME)
    # WHY option: CI and CPU-only environments can pass -DNO_GL_INTEROP=ON to
    # skip GLFW/OpenGL detection entirely without editing CMakeLists.
    option(NO_GL_INTEROP "Disable OpenGL interop (always headless)" OFF)

    if(NOT NO_GL_INTEROP)
        find_package(glfw3 QUIET)
        find_package(OpenGL QUIET)

        if(glfw3_FOUND AND OpenGL_FOUND)
            target_link_libraries(${TARGET_NAME} PRIVATE glfw OpenGL::GL)
            message(STATUS "GL interop: enabled (GLFW + OpenGL found)")

            # WHY conditional EGL: Intel NEO (intel-opencl-icd) only supports
            # cl_khr_gl_sharing for EGL-backed GL contexts, not GLX. Detecting EGL
            # at configure time lets us compile the fallback path only when available,
            # without affecting NVIDIA/GLX builds that lack EGL headers.
            find_package(OpenGL COMPONENTS EGL)
            if(OpenGL_EGL_FOUND)
                target_link_libraries(${TARGET_NAME} PRIVATE OpenGL::EGL)
                target_compile_definitions(${TARGET_NAME} PRIVATE HAS_EGL)
                message(STATUS "EGL found — Intel GL interop fallback enabled")
            else()
                message(STATUS "EGL not found — Intel GL interop fallback disabled")
            endif()
        else()
            message(WARNING "glfw3/OpenGL not found — building without GL interop (NO_GL_INTEROP). Install libglfw3-dev to enable live mode.")
            target_compile_definitions(${TARGET_NAME} PRIVATE NO_GL_INTEROP)
        endif()
    else()
        message(STATUS "GL interop: disabled by -DNO_GL_INTEROP=ON")
        target_compile_definitions(${TARGET_NAME} PRIVATE NO_GL_INTEROP)
    endif()
endmacro()

# ── opencl_lab_fetch_tinyobjloader(<target>) ──────────────────────────────────
# WHY macro: FetchContent_Declare and FetchContent_MakeAvailable must run at
# the top-level CMake scope to be idempotent across multiple subprojects in a
# root build. Using a macro (not function) ensures they execute in the caller's
# scope rather than an isolated function scope.
macro(opencl_lab_fetch_tinyobjloader TARGET_NAME)
    # WHY FetchContent: keeps each module self-contained — no system install required.
    FetchContent_Declare(
        tinyobjloader
        GIT_REPOSITORY https://github.com/tinyobjloader/tinyobjloader.git
        GIT_TAG        v2.0.0rc13
    )
    FetchContent_MakeAvailable(tinyobjloader)
    target_link_libraries(${TARGET_NAME} PRIVATE tinyobjloader)
endmacro()
