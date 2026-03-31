cmake_minimum_required(VERSION 3.18)
project(SmokeTest LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Find OpenCL
find_package(OpenCL REQUIRED)

# Executable
add_executable(smoke_test src/main.cpp)

# Link OpenCL
target_link_libraries(smoke_test PRIVATE OpenCL::OpenCL)

# WHY create_symlink (not file(COPY)): a symlink means edits to .cl source
# files are visible immediately without re-running CMake or copying again.
# file(COPY) was a one-shot snapshot that went stale after any kernel edit.
add_custom_command(TARGET smoke_test POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E create_symlink
            "${CMAKE_CURRENT_SOURCE_DIR}/kernels"
            "$<TARGET_FILE_DIR:smoke_test>/kernels"
    COMMENT "Symlinking kernels for smoke_test"
)

message(STATUS "Smoke Test configured. Run 'make' and then './smoke_test' to verify.")
