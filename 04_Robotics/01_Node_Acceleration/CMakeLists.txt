cmake_minimum_required(VERSION 3.18)
project(NodeAcceleration)

# ── C++17, strict mode (no GNU extensions) ───────────────────────────────────
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# common.cmake brings: find_package(OpenCL REQUIRED), CLI11 via FetchContent,
# opencl_lab_target(), copy_kernels() helpers, and common/vendor include dirs.
# Module is 2 levels deep: 04_Robotics/C1_Node_Acceleration/
include(${CMAKE_CURRENT_SOURCE_DIR}/../../common/common.cmake)

# ── ROS 2 environment guard ───────────────────────────────────────────────────
opencl_lab_ros2_guard()

# ── Dependencies ─────────────────────────────────────────────────────────────
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rclcpp_lifecycle REQUIRED)
find_package(std_msgs REQUIRED)
find_package(lifecycle_msgs REQUIRED)

# ── Binary ───────────────────────────────────────────────────────────────────
# WHY renamed to node_acceleration: matches the naming convention (no abbreviated prefix).
# synthetic_publisher.cpp is compiled as a separate TU (design spec §Directory).
add_executable(node_acceleration main.cpp synthetic_publisher.cpp)

# ── Include directories ───────────────────────────────────────────────────────
target_include_directories(node_acceleration PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../common
    ${CMAKE_CURRENT_SOURCE_DIR}/../../vendor
    ${CMAKE_CURRENT_SOURCE_DIR}   # synthetic_publisher.hpp lives here
)

# ── ament+OpenCL+CLI11 linking ────────────────────────────────────────────────
opencl_lab_ros2_target(node_acceleration rclcpp rclcpp_lifecycle std_msgs lifecycle_msgs)

copy_kernels(node_acceleration)
symlink_assets(node_acceleration)
