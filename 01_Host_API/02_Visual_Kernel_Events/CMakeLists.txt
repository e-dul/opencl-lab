cmake_minimum_required(VERSION 3.18)
project(VisualKernelEvents LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Dependencies ─────────────────────────────────────────────────────────────

find_package(OpenCL REQUIRED)

include(FetchContent)
FetchContent_Declare(
    stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG        master
)
FetchContent_MakeAvailable(stb)

# ── Common (header-only interface library) ────────────────────────────────────
# Referenced by relative path so each module is standalone.
add_subdirectory(${PROJECT_SOURCE_DIR}/../../common common_iface)

# ── Executable ────────────────────────────────────────────────────────────────

add_executable(visual_kernel_events src/main.cpp)

target_include_directories(visual_kernel_events PRIVATE
    ${stb_SOURCE_DIR}   # exposes stb_image.h / stb_image_write.h
)

target_link_libraries(visual_kernel_events PRIVATE
    OpenCL::OpenCL
    common   # header-only; pulls in common/ and vendor/ include paths
)

# Copy kernels next to the binary so the relative path "kernels/mad.cl" works.
install(DIRECTORY kernels/ DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/kernels)

message(STATUS "VisualKernelEvents configured. Build then run: ./build/visual_kernel_events --contrast 1.5 --brightness 20")
