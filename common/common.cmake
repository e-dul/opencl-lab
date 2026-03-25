# common/common.cmake
# Include from any module: include(${CMAKE_CURRENT_SOURCE_DIR}/../../common/common.cmake)
#
# Provides:
#   - OpenCL + stb dependencies
#   - opencl_lab_target(<target>)  — wires include dirs and link libs
#   - symlink_kernels(<target>)    — POST_BUILD symlink of kernels/ next to binary
#   - symlink_assets(<target>)     — POST_BUILD symlink of repo-root assets/ into binary dir
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

# ── symlink_kernels(<target>) ────────────────────────────────────────────────
# POST_BUILD: symlinks <binary_dir>/kernels → <module>/kernels/ so edits to
# .cl source files are immediately visible without a rebuild.
# Uses generator expression so it works with multi-config generators (MSVC/Xcode).
# WHY create_symlink (not copy_directory): editing a .cl file after the first
# build is reflected instantly because the binary dir points at the source dir.
# LIMITATION: cmake -E create_symlink requires source and build directory to
# reside on the same filesystem. For cross-filesystem / container builds,
# replace create_symlink with copy_directory as a fallback.
function(symlink_kernels TARGET_NAME)
    add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E create_symlink
            "${CMAKE_CURRENT_SOURCE_DIR}/kernels"
            "$<TARGET_FILE_DIR:${TARGET_NAME}>/kernels"
        COMMENT "Symlinking kernels for ${TARGET_NAME}"
    )
endfunction()

# ── symlink_assets(<target>) ──────────────────────────────────────────────────
# POST_BUILD: creates <binary_dir>/assets → <repo_root>/assets/ symlink.
# WHY explicit target arg: mirrors symlink_kernels() convention; avoids PROJECT_NAME
# vs executable-name mismatches across modules. Repo layout fixed at 2 levels deep.
function(symlink_assets TARGET_NAME)
    add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E create_symlink
            "${CMAKE_CURRENT_SOURCE_DIR}/../../assets"
            "$<TARGET_FILE_DIR:${TARGET_NAME}>/assets"
        COMMENT "Symlinking assets for ${TARGET_NAME}"
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

# ── opencl_lab_ros2_guard([LOANED_MESSAGES]) ──────────────────────────────────
# WHY macro (not function): cmake_parse_arguments and message() both affect the
# caller's scope. Using a macro avoids creating an isolated scope so the guard
# terminates configuration in the calling CMakeLists, not inside a child scope.
#
# Usage:
#   opencl_lab_ros2_guard()                 — ROS_DISTRO guard only
#   opencl_lab_ros2_guard(LOANED_MESSAGES)  — also warns about RMW capability
macro(opencl_lab_ros2_guard)
    cmake_parse_arguments(_ROS2_GUARD "LOANED_MESSAGES" "" "" ${ARGN})

    # WHY fatal for missing ROS_DISTRO: downstream find_package(rclcpp) errors
    # are cryptic without this guard. Fail early with a clear instruction.
    if(NOT DEFINED ENV{ROS_DISTRO})
        message(FATAL_ERROR
            "ROS_DISTRO is not set. Run: source /opt/ros/jazzy/setup.bash")
    endif()

    if(_ROS2_GUARD_LOANED_MESSAGES)
        # Loaned messages require rmw_fastrtps_cpp or Iceoryx.
        # This is a warning (not fatal) — the node falls back to copy-based transport.
        if(NOT DEFINED ENV{RMW_IMPLEMENTATION} OR
           (NOT "$ENV{RMW_IMPLEMENTATION}" STREQUAL "rmw_fastrtps_cpp" AND
            NOT "$ENV{RMW_IMPLEMENTATION}" STREQUAL "rmw_iceoryx_cpp"))
            message(WARNING
                "RMW_IMPLEMENTATION is not set to a loaned-message-capable RMW.\n"
                "Loaned messages (zero-copy upload) are unavailable.\n"
                "Set: export RMW_IMPLEMENTATION=rmw_fastrtps_cpp for optimal latency.")
        endif()
    endif()
endmacro()

# ── opencl_lab_ros2_target(<target> <ament_pkg...>) ───────────────────────────
# WHY macro (not function): ament_target_dependencies uses the CMake plain
# (non-keyword) signature internally. Calling it from inside a function() causes
# CMake's mixed-signature detection to fire when the caller has previously used
# keyword form on the same target. A macro executes in the caller's scope,
# keeping all calls to target_link_libraries consistent.
#
# Ordering: ament_target_dependencies MUST precede target_link_libraries to
# satisfy ament's requirement that its plain-form call comes first.
#
# Does NOT add target_include_directories — callers keep their own because
# the relative depth to common/ and vendor/ differs per module.
macro(opencl_lab_ros2_target TARGET_NAME)
    # WHY variadic: the ament packages differ per module (rclcpp, nav_msgs, etc.).
    set(_ROS2_TARGET_AMENT_PKGS ${ARGN})

    # Step 1: ament linking (plain form — must precede target_link_libraries).
    ament_target_dependencies(${TARGET_NAME} ${_ROS2_TARGET_AMENT_PKGS})

    # Step 2: OpenCL version flags — must be set explicitly so opencl_utils.hpp
    # guards work regardless of include order.
    target_compile_definitions(${TARGET_NAME} PRIVATE
        CL_HPP_ENABLE_EXCEPTIONS
        CL_HPP_TARGET_OPENCL_VERSION=120
        CL_HPP_MINIMUM_OPENCL_VERSION=120
    )


    # Step 3: Non-ament libs (plain form — must stay consistent with ament's
    # plain-form call above; mixing keyword and plain signatures on the same
    # target causes a CMake error). OpenCL/CLI11 do not propagate as interface
    # deps because this target is an executable, not a library.
    target_link_libraries(${TARGET_NAME}
        OpenCL::OpenCL
        CLI11::CLI11
    )
endmacro()
