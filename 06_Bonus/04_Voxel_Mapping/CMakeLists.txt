cmake_minimum_required(VERSION 3.18)
project(voxel_mapping CXX)

# ── C++17, strict mode (no GNU extensions) ───────────────────────────────────
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── common.cmake: stb, OpenCL, ROS 2 macros ──────────────────────────────────
include(${CMAKE_CURRENT_SOURCE_DIR}/../../common/common.cmake)

# ── ROS 2 environment guard — must precede find_package ──────────────────────
opencl_lab_ros2_guard()

# ── ROS 2 packages ────────────────────────────────────────────────────────────
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rclcpp_components REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(diagnostic_updater REQUIRED)

# ─────────────────────────────────────────────────────────────────────────────
# Target 1: voxel_mapping — subscribes to PointCloud2, runs DDA ray-casting
# ─────────────────────────────────────────────────────────────────────────────
add_library(voxel_mapping SHARED main.cpp)

target_include_directories(voxel_mapping PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../common
    ${CMAKE_CURRENT_SOURCE_DIR}/../../vendor
    ${stb_SOURCE_DIR}
)

opencl_lab_ros2_target(voxel_mapping rclcpp rclcpp_components sensor_msgs diagnostic_updater)

rclcpp_components_register_node(voxel_mapping
    PLUGIN "voxel_mapping::VoxelMappingNode"
    EXECUTABLE voxel_mapping_exe
)

# Kernels symlinked to binary dir for standalone cmake builds.
symlink_kernels(voxel_mapping)

# ─────────────────────────────────────────────────────────────────────────────
# Target 2: voxel_point_cloud_publisher — synthetic PointCloud2 source
# ─────────────────────────────────────────────────────────────────────────────
add_library(voxel_point_cloud_publisher SHARED point_cloud_publisher.cpp)

target_include_directories(voxel_point_cloud_publisher PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../common
    ${CMAKE_CURRENT_SOURCE_DIR}/../../vendor
)

opencl_lab_ros2_target(voxel_point_cloud_publisher rclcpp rclcpp_components sensor_msgs)

rclcpp_components_register_node(voxel_point_cloud_publisher
    PLUGIN "voxel_mapping::VoxelCloudPublisher"
    EXECUTABLE voxel_point_cloud_publisher_exe
)

# ─────────────────────────────────────────────────────────────────────────────
# Install both targets, kernels, launch/, config/
# ─────────────────────────────────────────────────────────────────────────────
opencl_lab_ros2_install(voxel_mapping)
opencl_lab_ros2_install(voxel_point_cloud_publisher)
