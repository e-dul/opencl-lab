cmake_minimum_required(VERSION 3.18)
project(SVM CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Enforce -std=c++17 (not -std=gnu++17) for stricter conformance.
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(OpenCL REQUIRED)

# CLI11 v2.4.2 — header-only, fetched directly to avoid inheriting the
# CL_HPP_TARGET_OPENCL_VERSION 120 define that common.cmake would propagate.
include(FetchContent)
FetchContent_Declare(
    CLI11
    GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
    GIT_TAG        v2.4.2
)
FetchContent_MakeAvailable(CLI11)

add_executable(svm_demo main.cpp)

target_compile_options(svm_demo PRIVATE -Wall -Wextra)

target_link_libraries(svm_demo PRIVATE OpenCL::OpenCL CLI11::CLI11)

# vendor/ holds cl.hpp / opencl.hpp if the system does not provide them.
target_include_directories(svm_demo PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../vendor
)

# Copy kernels/ next to the binary post-build so the executable can load them
# via a relative path regardless of the working directory at runtime.
add_custom_command(TARGET svm_demo POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/kernels
            $<TARGET_FILE_DIR:svm_demo>/kernels
    COMMENT "Copying kernels for svm_demo"
)
# POST_BUILD: symlink repo-root assets/ into binary dir so --input paths work.
add_custom_command(TARGET svm_demo POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E create_symlink
            "${CMAKE_CURRENT_SOURCE_DIR}/../../assets"
            "$<TARGET_FILE_DIR:svm_demo>/assets"
    COMMENT "Symlinking assets for svm_demo"
)
