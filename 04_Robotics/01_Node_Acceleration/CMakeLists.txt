cmake_minimum_required(VERSION 3.18)
project(node_acceleration)

# ── C++17, strict mode (no GNU extensions) ───────────────────────────────────
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# common.cmake brings: find_package(OpenCL REQUIRED), CLI11 via FetchContent,
# opencl_lab_target(), symlink_kernels() helpers, and common/vendor include dirs.
# Module is 2 levels deep: 04_Robotics/01_Node_Acceleration/
include(${CMAKE_CURRENT_SOURCE_DIR}/../../common/common.cmake)

# ── ROS 2 environment guard ───────────────────────────────────────────────────
opencl_lab_ros2_guard()

# ── Dependencies ─────────────────────────────────────────────────────────────
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rclcpp_lifecycle REQUIRED)
find_package(rclcpp_components REQUIRED)
find_package(std_msgs REQUIRED)
find_package(lifecycle_msgs REQUIRED)

# ── Shared library (AccelNode + SyntheticPublisher as composable components) ─
# WHY SHARED: rclcpp_components_register_node generates a plugin registration
# table that the component container loads at runtime via dlopen().
add_library(node_acceleration SHARED main.cpp synthetic_publisher.cpp)
target_compile_definitions(node_acceleration PRIVATE "COMPOSITION_BUILDING_DLL")

target_include_directories(node_acceleration PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../common
    ${CMAKE_CURRENT_SOURCE_DIR}/../../vendor
    ${CMAKE_CURRENT_SOURCE_DIR}   # synthetic_publisher.hpp lives here
)

# ── ament+OpenCL+CLI11 linking ────────────────────────────────────────────────
opencl_lab_ros2_target(node_acceleration
    rclcpp rclcpp_lifecycle rclcpp_components std_msgs lifecycle_msgs)

# ── Register composable nodes ─────────────────────────────────────────────────
# EXECUTABLE: generates a thin standalone wrapper (single-node container) for
# debugging without a full launch file.
rclcpp_components_register_node(node_acceleration
    PLUGIN "node_acceleration::AccelNode"
    EXECUTABLE node_acceleration_exe)

rclcpp_components_register_node(node_acceleration
    PLUGIN "node_acceleration::SyntheticPublisher"
    EXECUTABLE synthetic_publisher_exe)

symlink_kernels(node_acceleration)
symlink_assets(node_acceleration)

# ── Install targets, kernels, launch, config; call ament_package() ───────────
opencl_lab_ros2_install(node_acceleration)
