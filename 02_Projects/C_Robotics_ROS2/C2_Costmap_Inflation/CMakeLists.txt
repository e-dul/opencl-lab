cmake_minimum_required(VERSION 3.18)
project(c2_costmap_inflation)

# ── ROS 2 environment guard — must precede find_package ─────────────────────
# WHY guard: missing setup.bash causes cryptic find_package errors downstream.
if(NOT DEFINED ENV{ROS_DISTRO})
  message(FATAL_ERROR "ROS_DISTRO is not set. Run: source /opt/ros/jazzy/setup.bash")
endif()

# ── C++17, strict mode (no GNU extensions) ───────────────────────────────────
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── common.cmake: CLI11, stb, common INTERFACE target ────────────────────────
# WHY ../../../: module is 3 levels deep (02_Projects/C_Robotics_ROS2/C2_Costmap_Inflation/)
include(${CMAKE_CURRENT_SOURCE_DIR}/../../../common/common.cmake)

# ── ROS 2 packages ────────────────────────────────────────────────────────────
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rclcpp_lifecycle REQUIRED)
find_package(nav_msgs REQUIRED)
find_package(lifecycle_msgs REQUIRED)

# OpenCL is already found by common.cmake; this is for clarity and ament ordering.
find_package(OpenCL REQUIRED)

# ── Binary ────────────────────────────────────────────────────────────────────
add_executable(costmap_node main.cpp map_publisher.cpp)

# ── Include directories ───────────────────────────────────────────────────────
target_include_directories(costmap_node PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../../common
  ${CMAKE_CURRENT_SOURCE_DIR}/../../../vendor
  ${stb_SOURCE_DIR}
)

# ── Compile definitions ───────────────────────────────────────────────────────
target_compile_definitions(costmap_node PRIVATE
  CL_HPP_ENABLE_EXCEPTIONS
  CL_HPP_TARGET_OPENCL_VERSION=120
  CL_HPP_MINIMUM_OPENCL_VERSION=120
)

# ── ament link (ROS 2 packages) ───────────────────────────────────────────────
# WHY ament_target_dependencies before target_link_libraries: ament uses the
# CMake plain (non-keyword) signature internally; mixing keyword and plain
# forms triggers a CMake error.
ament_target_dependencies(costmap_node
  rclcpp rclcpp_lifecycle nav_msgs lifecycle_msgs
)

# ── Non-ament link (OpenCL, CLI11) ────────────────────────────────────────────
target_link_libraries(costmap_node OpenCL::OpenCL CLI11::CLI11)

# ── Post-build: copy kernels/ next to binary (master spec §1) ────────────────
add_custom_command(TARGET costmap_node POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          ${CMAKE_CURRENT_SOURCE_DIR}/kernels
          $<TARGET_FILE_DIR:costmap_node>/kernels
  COMMENT "Copying kernels"
)
