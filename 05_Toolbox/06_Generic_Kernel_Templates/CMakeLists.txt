cmake_minimum_required(VERSION 3.18)
project(GenericKernelTemplates CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# WHY ../../common: this module sits 2 levels deep (99_Toolbox/GenericKernelTemplates/).
# common.cmake provides: find_package(OpenCL), FetchContent for stb + CLI11,
# opencl_lab_target(), and symlink_kernels().
include(${CMAKE_CURRENT_SOURCE_DIR}/../../common/common.cmake)

# ── 01_Basic_MAD ──────────────────────────────────────────────────────────────
# Baseline: single hardcoded float MAD kernel. No -D flags.
add_executable(01_basic_mad 01_Basic_MAD/main.cpp)
opencl_lab_target(01_basic_mad)

# WHY explicit path: kernels/ lives at the GenericKernelTemplates root, not
# inside 01_Basic_MAD/, so we cannot use the symlink_kernels() helper which assumes
# ${CMAKE_CURRENT_SOURCE_DIR}/kernels. All three targets share one kernel file.
add_custom_command(TARGET 01_basic_mad POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E create_symlink
            "${CMAKE_CURRENT_SOURCE_DIR}/kernels"
            "$<TARGET_FILE_DIR:01_basic_mad>/kernels"
    COMMENT "Symlinking kernels for 01_basic_mad"
)
symlink_assets(01_basic_mad)

# ── 02_Generic_MAD ────────────────────────────────────────────────────────────
# Builds mad_kernel.cl twice: -D TYPE=uchar and -D TYPE=float.
# Prints a two-row timing table; output.bmp from float variant.
add_executable(02_generic_mad 02_Generic_MAD/main.cpp)
opencl_lab_target(02_generic_mad)

add_custom_command(TARGET 02_generic_mad POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E create_symlink
            "${CMAKE_CURRENT_SOURCE_DIR}/kernels"
            "$<TARGET_FILE_DIR:02_generic_mad>/kernels"
    COMMENT "Symlinking kernels for 02_generic_mad"
)
symlink_assets(02_generic_mad)

# ── 03_AutoTune ───────────────────────────────────────────────────────────────
# Builds uchar, float, and optionally half variants. Selects the fastest.
add_executable(03_autotune 03_AutoTune/main.cpp)
opencl_lab_target(03_autotune)

add_custom_command(TARGET 03_autotune POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E create_symlink
            "${CMAKE_CURRENT_SOURCE_DIR}/kernels"
            "$<TARGET_FILE_DIR:03_autotune>/kernels"
    COMMENT "Symlinking kernels for 03_autotune"
)
symlink_assets(03_autotune)
