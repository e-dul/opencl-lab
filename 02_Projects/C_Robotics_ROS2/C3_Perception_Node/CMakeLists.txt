cmake_minimum_required(VERSION 3.18)
project(c3_perception_node)

# ── C++17, strict mode (no GNU extensions) ───────────────────────────────────
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── common.cmake: CLI11, stb, OpenCL, include dirs ────────────────────────────
# WHY ../../../: module is 3 levels deep (02_Projects/C_Robotics_ROS2/C3_Perception_Node/)
include(${CMAKE_CURRENT_SOURCE_DIR}/../../../common/common.cmake)

# ── ROS 2 environment guard — must precede find_package ──────────────────────
# LOANED_MESSAGES: also emit the RMW warning since C3 uses zero-copy upload.
opencl_lab_ros2_guard(LOANED_MESSAGES)

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

# ── ament+OpenCL+CLI11 linking ────────────────────────────────────────────────
opencl_lab_ros2_target(perception_node rclcpp rclcpp_lifecycle sensor_msgs lifecycle_msgs)

copy_kernels(perception_node)

# ─────────────────────────────────────────────────────────────────────────────
# Target 2: point_cloud_publisher (point_cloud_publisher.cpp)
# ─────────────────────────────────────────────────────────────────────────────
add_executable(point_cloud_publisher point_cloud_publisher.cpp)

target_include_directories(point_cloud_publisher PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../common
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../vendor
)

# WHY opencl_lab_ros2_target here: the macro always adds OpenCL::OpenCL.
# point_cloud_publisher does not call any CL API, but linking is harmless —
# the shared library is loaded lazily and no CL symbols are referenced.
opencl_lab_ros2_target(point_cloud_publisher rclcpp sensor_msgs)

copy_kernels(point_cloud_publisher)
