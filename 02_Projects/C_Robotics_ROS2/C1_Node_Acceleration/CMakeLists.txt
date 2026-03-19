cmake_minimum_required(VERSION 3.18)
project(c1_node_acceleration)

# ── C++17, strict mode (no GNU extensions) ───────────────────────────────────
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# common.cmake brings: find_package(OpenCL REQUIRED), CLI11 via FetchContent,
# opencl_lab_target(), copy_kernels() helpers, and common/vendor include dirs.
# WHY ../../../: module is 3 levels deep (02_Projects/C_Robotics_ROS2/C1_Node_Acceleration/).
include(${CMAKE_CURRENT_SOURCE_DIR}/../../../common/common.cmake)

# ── ROS 2 environment guard ───────────────────────────────────────────────────
opencl_lab_ros2_guard()

# ── Dependencies ─────────────────────────────────────────────────────────────
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rclcpp_lifecycle REQUIRED)
find_package(std_msgs REQUIRED)
find_package(lifecycle_msgs REQUIRED)

# ── Binary ───────────────────────────────────────────────────────────────────
# WHY renamed to accel_node: matches the binary name in the design doc and DoD.
# synthetic_publisher.cpp is compiled as a separate TU (design spec §Directory).
add_executable(accel_node main.cpp synthetic_publisher.cpp)

# ── Include directories ───────────────────────────────────────────────────────
target_include_directories(accel_node PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../common
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../vendor
    ${CMAKE_CURRENT_SOURCE_DIR}   # synthetic_publisher.hpp lives here
)

# ── ament+OpenCL+CLI11 linking ────────────────────────────────────────────────
opencl_lab_ros2_target(accel_node rclcpp rclcpp_lifecycle std_msgs lifecycle_msgs)

copy_kernels(accel_node)
