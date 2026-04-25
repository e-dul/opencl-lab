cmake_minimum_required(VERSION 3.18)
project(perception_node)

# ── C++17, strict mode (no GNU extensions) ───────────────────────────────────
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── common.cmake: CLI11, stb, OpenCL, include dirs ────────────────────────────
# Module is 2 levels deep: 04_Robotics/03_Perception_Node/
include(${CMAKE_CURRENT_SOURCE_DIR}/../../common/common.cmake)

# ── ROS 2 environment guard — must precede find_package ──────────────────────
# LOANED_MESSAGES: also emit the RMW warning since C3 uses zero-copy upload.
opencl_lab_ros2_guard(LOANED_MESSAGES)

# ── ROS 2 packages ────────────────────────────────────────────────────────────
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rclcpp_lifecycle REQUIRED)
find_package(rclcpp_components REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(lifecycle_msgs REQUIRED)

# ─────────────────────────────────────────────────────────────────────────────
# Target 1: perception_node (main.cpp — LifecycleNode with OpenCL pipeline)
# WHY separate library: PerceptionNode is heavyweight (OpenCL context, 5 kernels,
# double-buffered device memory); PointCloudPublisher is a lightweight synthetic
# publisher with no OpenCL dependency.
# ─────────────────────────────────────────────────────────────────────────────
add_library(perception_node SHARED main.cpp)
target_compile_definitions(perception_node PRIVATE "COMPOSITION_BUILDING_DLL")

target_include_directories(perception_node PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../common
    ${CMAKE_CURRENT_SOURCE_DIR}/../../vendor
)

opencl_lab_ros2_target(perception_node
    rclcpp rclcpp_lifecycle rclcpp_components sensor_msgs lifecycle_msgs)

rclcpp_components_register_node(perception_node
    PLUGIN "perception_node::PerceptionNode"
    EXECUTABLE perception_node_exe)

symlink_kernels(perception_node)
symlink_assets(perception_node)

# ─────────────────────────────────────────────────────────────────────────────
# Target 2: point_cloud_publisher (point_cloud_publisher.cpp)
# ─────────────────────────────────────────────────────────────────────────────
add_library(point_cloud_publisher SHARED point_cloud_publisher.cpp)
target_compile_definitions(point_cloud_publisher PRIVATE "COMPOSITION_BUILDING_DLL")

target_include_directories(point_cloud_publisher PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../common
    ${CMAKE_CURRENT_SOURCE_DIR}/../../vendor
)

# WHY opencl_lab_ros2_target: macro adds ament_index_cpp + HAVE_AMENT_INDEX_CPP
# even though this target does not call CL API — harmless linkage.
opencl_lab_ros2_target(point_cloud_publisher rclcpp rclcpp_components sensor_msgs)

rclcpp_components_register_node(point_cloud_publisher
    PLUGIN "perception_node::PointCloudPublisher"
    EXECUTABLE point_cloud_publisher_exe)

# Install both targets; second call skips kernels+ament_package (already done).
opencl_lab_ros2_install(perception_node)
opencl_lab_ros2_install(point_cloud_publisher)
