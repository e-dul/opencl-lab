cmake_minimum_required(VERSION 3.18)
project(costmap_inflation)

# ── C++17, strict mode (no GNU extensions) ───────────────────────────────────
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── common.cmake: CLI11, stb, OpenCL, include dirs ────────────────────────────
# Module is 2 levels deep: 04_Robotics/02_Costmap_Inflation/
include(${CMAKE_CURRENT_SOURCE_DIR}/../../common/common.cmake)

# ── ROS 2 environment guard — must precede find_package ─────────────────────
opencl_lab_ros2_guard()

# ── ROS 2 packages ────────────────────────────────────────────────────────────
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rclcpp_lifecycle REQUIRED)
find_package(rclcpp_components REQUIRED)
find_package(nav_msgs REQUIRED)
find_package(lifecycle_msgs REQUIRED)

# ── Shared library (CostmapNode + MapPublisher as composable components) ──────
add_library(costmap_inflation SHARED main.cpp map_publisher.cpp)
target_compile_definitions(costmap_inflation PRIVATE "COMPOSITION_BUILDING_DLL")

target_include_directories(costmap_inflation PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../common
    ${CMAKE_CURRENT_SOURCE_DIR}/../../vendor
    ${stb_SOURCE_DIR}
)

# ── ament+OpenCL+CLI11 linking ────────────────────────────────────────────────
opencl_lab_ros2_target(costmap_inflation
    rclcpp rclcpp_lifecycle rclcpp_components nav_msgs lifecycle_msgs)

rclcpp_components_register_node(costmap_inflation
    PLUGIN "costmap_inflation::CostmapNode"
    EXECUTABLE costmap_inflation_exe)

rclcpp_components_register_node(costmap_inflation
    PLUGIN "costmap_inflation::MapPublisher"
    EXECUTABLE map_publisher_exe)

symlink_kernels(costmap_inflation)
symlink_assets(costmap_inflation)

opencl_lab_ros2_install(costmap_inflation)
