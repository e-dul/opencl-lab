cmake_minimum_required(VERSION 3.18)
project(c1_node_acceleration)

# ── C++17, strict mode (no GNU extensions) ───────────────────────────────────
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── ROS 2 environment guard ───────────────────────────────────────────────────
# WHY guard: missing setup.bash causes cryptic find_package errors downstream.
# Failing early with a clear message saves debugging time.
if(NOT DEFINED ENV{ROS_DISTRO})
    message(FATAL_ERROR
        "ROS_DISTRO is not set. Run: source /opt/ros/jazzy/setup.bash")
endif()

# ── Dependencies ─────────────────────────────────────────────────────────────
find_package(rclcpp REQUIRED)
find_package(rclcpp_lifecycle REQUIRED)
find_package(std_msgs REQUIRED)
find_package(lifecycle_msgs REQUIRED)

# common.cmake brings: find_package(OpenCL REQUIRED), CLI11 via FetchContent,
# opencl_lab_target(), copy_kernels() helpers, and common/vendor include dirs.
# WHY ../../../: module is 3 levels deep (02_Projects/C_Robotics_ROS2/C1_Node_Acceleration/).
include(${CMAKE_CURRENT_SOURCE_DIR}/../../../common/common.cmake)

# ── Binary ───────────────────────────────────────────────────────────────────
# WHY renamed to accel_node: matches the binary name in the design doc and DoD.
add_executable(accel_node main.cpp)

# ── Include directories ───────────────────────────────────────────────────────
target_include_directories(accel_node PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../common
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../vendor
)

# ── Compile definitions ───────────────────────────────────────────────────────
# WHY all three: opencl_utils.hpp already guards with #ifndef, but setting
# them here makes the intent explicit and avoids relying on include order.
target_compile_definitions(accel_node PRIVATE
    CL_HPP_ENABLE_EXCEPTIONS
    CL_HPP_TARGET_OPENCL_VERSION=120
    CL_HPP_MINIMUM_OPENCL_VERSION=120
)

# ── ament link (ROS 2 packages) ───────────────────────────────────────────────
# WHY ament_target_dependencies before target_link_libraries: ament uses the
# CMake plain (non-keyword) signature internally. Calling target_link_libraries
# with PRIVATE first and then ament_target_dependencies triggers CMake's mixed-
# signature error. Calling ament_target_dependencies first is safe because the
# subsequent target_link_libraries call must also use the plain form.
ament_target_dependencies(accel_node
    rclcpp
    rclcpp_lifecycle
    std_msgs
    lifecycle_msgs
)

# ── Non-ament link (OpenCL, CLI11) ────────────────────────────────────────────
# WHY plain form: after ament_target_dependencies we must stay consistent with
# the plain (non-keyword) CMake signature to avoid the mixed-signature error.
target_link_libraries(accel_node
    OpenCL::OpenCL
    CLI11::CLI11
)

# ── Post-build: copy kernels/ next to binary ─────────────────────────────────
add_custom_command(TARGET accel_node POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/kernels
            $<TARGET_FILE_DIR:accel_node>/kernels
    COMMENT "Copying kernels"
)
