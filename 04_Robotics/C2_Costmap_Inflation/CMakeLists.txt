cmake_minimum_required(VERSION 3.18)
project(c2_costmap_inflation)

# ── C++17, strict mode (no GNU extensions) ───────────────────────────────────
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── common.cmake: CLI11, stb, OpenCL, include dirs ────────────────────────────
# Module is 2 levels deep: 04_Robotics/C2_Costmap_Inflation/
include(${CMAKE_CURRENT_SOURCE_DIR}/../../common/common.cmake)

# ── ROS 2 environment guard — must precede find_package ─────────────────────
opencl_lab_ros2_guard()

# ── ROS 2 packages ────────────────────────────────────────────────────────────
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rclcpp_lifecycle REQUIRED)
find_package(nav_msgs REQUIRED)
find_package(lifecycle_msgs REQUIRED)

# ── Binary ────────────────────────────────────────────────────────────────────
add_executable(costmap_node main.cpp map_publisher.cpp)

# ── Include directories ───────────────────────────────────────────────────────
target_include_directories(costmap_node PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../common
  ${CMAKE_CURRENT_SOURCE_DIR}/../../vendor
  ${stb_SOURCE_DIR}
)

# ── ament+OpenCL+CLI11 linking ────────────────────────────────────────────────
opencl_lab_ros2_target(costmap_node rclcpp rclcpp_lifecycle nav_msgs lifecycle_msgs)

copy_kernels(costmap_node)
