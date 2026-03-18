cmake_minimum_required(VERSION 3.18)
project(c3_perception_node)

# ── C++17, strict mode (no GNU extensions) ───────────────────────────────────
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── ROS 2 environment guard — must precede find_package ──────────────────────
# WHY guard: missing setup.bash causes cryptic find_package errors downstream.
if(NOT DEFINED ENV{ROS_DISTRO})
    message(FATAL_ERROR
        "ROS_DISTRO is not set.\n"
        "Run: source /opt/ros/jazzy/setup.bash\n"
        "Then re-run cmake.")
endif()

# ── RMW loaned-message warning ────────────────────────────────────────────────
# Loaned messages require rmw_fastrtps_cpp or Iceoryx.
# This is a warning (not fatal) — the node falls back to copy-based transport.
if(NOT DEFINED ENV{RMW_IMPLEMENTATION} OR
   (NOT "$ENV{RMW_IMPLEMENTATION}" STREQUAL "rmw_fastrtps_cpp" AND
    NOT "$ENV{RMW_IMPLEMENTATION}" STREQUAL "rmw_iceoryx_cpp"))
    message(WARNING
        "RMW_IMPLEMENTATION is not set to a loaned-message-capable RMW.\n"
        "Loaned messages (zero-copy upload) are unavailable.\n"
        "Set: export RMW_IMPLEMENTATION=rmw_fastrtps_cpp for optimal C3 latency.")
endif()

# ── common.cmake: CLI11, stb, OpenCL, include dirs ────────────────────────────
# WHY ../../../: module is 3 levels deep (02_Projects/C_Robotics_ROS2/C3_Perception_Node/)
include(${CMAKE_CURRENT_SOURCE_DIR}/../../../common/common.cmake)

# ── ROS 2 packages ────────────────────────────────────────────────────────────
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rclcpp_lifecycle REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(lifecycle_msgs REQUIRED)

# ─────────────────────────────────────────────────────────────────────────────
# Target 1: perception_node (main.cpp)
# ─────────────────────────────────────────────────────────────────────────────
add_executable(perception_node main.cpp)

target_include_directories(perception_node PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../common
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../vendor
)

target_compile_definitions(perception_node PRIVATE
    CL_HPP_ENABLE_EXCEPTIONS
    CL_HPP_TARGET_OPENCL_VERSION=120
    CL_HPP_MINIMUM_OPENCL_VERSION=120
)

# WHY ament_target_dependencies before target_link_libraries: ament uses the
# CMake plain (non-keyword) signature internally; mixing keyword and plain
# forms triggers a CMake mixed-signature error.
ament_target_dependencies(perception_node
    rclcpp
    rclcpp_lifecycle
    sensor_msgs
    lifecycle_msgs
)

target_link_libraries(perception_node
    OpenCL::OpenCL
    CLI11::CLI11
)

# Post-build: copy kernels/ next to binary (master spec §1)
add_custom_command(TARGET perception_node POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/kernels
            $<TARGET_FILE_DIR:perception_node>/kernels
    COMMENT "Copying kernels for perception_node"
)

# ─────────────────────────────────────────────────────────────────────────────
# Target 2: point_cloud_publisher (point_cloud_publisher.cpp)
# ─────────────────────────────────────────────────────────────────────────────
add_executable(point_cloud_publisher point_cloud_publisher.cpp)

target_include_directories(point_cloud_publisher PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../common
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../vendor
)

ament_target_dependencies(point_cloud_publisher
    rclcpp
    sensor_msgs
)

target_link_libraries(point_cloud_publisher
    CLI11::CLI11
)

# Post-build: copy kernels/ next to binary (master spec §1)
add_custom_command(TARGET point_cloud_publisher POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/kernels
            $<TARGET_FILE_DIR:point_cloud_publisher>/kernels
    COMMENT "Copying kernels for point_cloud_publisher"
)
