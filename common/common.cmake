# common/common.cmake
# Include from any module: include(${CMAKE_CURRENT_SOURCE_DIR}/../../common/common.cmake)
#
# Provides:
#   - OpenCL + stb dependencies
#   - opencl_lab_target(<target>)  — wires include dirs and link libs
#   - copy_kernels(<target>)       — POST_BUILD copy of kernels/ next to binary
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
