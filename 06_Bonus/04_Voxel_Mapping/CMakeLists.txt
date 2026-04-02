cmake_minimum_required(VERSION 3.18)
project(VoxelMapping CXX)

# ── C++17, strict mode (no GNU extensions) ───────────────────────────────────
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── common.cmake: CLI11, stb, OpenCL (2 levels up from 04_Addons/4_5_...) ────
include(${CMAKE_CURRENT_SOURCE_DIR}/../../common/common.cmake)

# ── ROS 2 environment guard — must precede find_package ──────────────────────
# WHY FATAL_ERROR: downstream find_package(rclcpp) errors are cryptic; fail early.
opencl_lab_ros2_guard()

# ── ROS 2 packages ────────────────────────────────────────────────────────────
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(sensor_msgs REQUIRED)

# ─────────────────────────────────────────────────────────────────────────────
# Target 1: voxel_mapping (main.cpp) — subscribes to PointCloud2, runs DDA
# ─────────────────────────────────────────────────────────────────────────────
add_executable(voxel_mapping main.cpp)

target_include_directories(voxel_mapping PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../common
    ${CMAKE_CURRENT_SOURCE_DIR}/../../vendor
    ${stb_SOURCE_DIR}
)

# ament first (plain-form), then OpenCL + CLI11 via macro
opencl_lab_ros2_target(voxel_mapping rclcpp sensor_msgs)

# Kernels copied to binary dir so load_kernel_source() can find them at runtime.
symlink_kernels(voxel_mapping)

# ─────────────────────────────────────────────────────────────────────────────
# Target 2: voxel_point_cloud_publisher (point_cloud_publisher.cpp)
# ─────────────────────────────────────────────────────────────────────────────
add_executable(voxel_point_cloud_publisher point_cloud_publisher.cpp)

target_include_directories(voxel_point_cloud_publisher PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../common
    ${CMAKE_CURRENT_SOURCE_DIR}/../../vendor
)

# WHY opencl_lab_ros2_target: macro is the canonical way to link ROS2 binaries;
# this binary does not call CL API but linking OpenCL is harmless (lazy load).
opencl_lab_ros2_target(voxel_point_cloud_publisher rclcpp sensor_msgs)

message(STATUS "VoxelMapping configured. Source ROS 2 before building.")
