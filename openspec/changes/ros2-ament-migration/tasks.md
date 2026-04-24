## 1. common/common.cmake — opencl_lab_ros2_install macro

- [ ] 1.1 Add `opencl_lab_ros2_install(<target>)` macro to `common/common.cmake`: installs ARCHIVE/LIBRARY to `lib/`, RUNTIME to `lib/${PROJECT_NAME}`, kernels/ to `share/${PROJECT_NAME}/kernels/`, calls `ament_package()`
- [ ] 1.2 Add `find_package(ament_index_cpp REQUIRED)` guard inside `opencl_lab_ros2_guard()` so modules can use `ament_index_cpp::get_package_share_directory` as kernel path fallback
- [ ] 1.3 Update `get_binary_dir()` in `common/opencl_utils.hpp` (or equivalent kernel-path resolution) to fall back to `ament_index_cpp::get_package_share_directory(<pkg>) + "/kernels"` when the build-tree symlink is absent

## 2. C1 Node Acceleration — ament package

- [ ] 2.1 Create `04_Robotics/01_Node_Acceleration/package.xml`: name `node_acceleration`, deps `rclcpp`, `rclcpp_lifecycle`, `rclcpp_components`, `std_msgs`, `lifecycle_msgs`, `ament_cmake`
- [ ] 2.2 Rewrite `CMakeLists.txt`: `project(node_acceleration)`, `find_package(rclcpp_components REQUIRED)`, `add_library(node_acceleration SHARED main.cpp synthetic_publisher.cpp)`, `rclcpp_components_register_node(node_acceleration PLUGIN "node_acceleration::AccelNode" EXECUTABLE node_acceleration_exe)`, `rclcpp_components_register_node(... PLUGIN "node_acceleration::SyntheticPublisher" EXECUTABLE synthetic_publisher_exe)`, `opencl_lab_ros2_install(node_acceleration)`
- [ ] 2.3 Wrap `AccelNode` class in `namespace node_acceleration { ... }` in `main.cpp`; add `RCLCPP_COMPONENTS_REGISTER_NODE(node_acceleration::AccelNode)` at end of file; remove `main()`
- [ ] 2.4 Convert `SyntheticPublisher` companion: wrap in `namespace node_acceleration`; replace any CLI11 args with `declare_parameter`; add `RCLCPP_COMPONENTS_REGISTER_NODE`; remove `main()`
- [ ] 2.5 Add `auto_activate` parameter to `AccelNode::on_configure()` (D5 pattern)
- [ ] 2.6 Create `launch/node_acceleration.launch.py`: `ComposableNodeContainer` with `AccelNode` + `SyntheticPublisher`; `auto_activate`, `iterations`, `buffer_size` as launch args; `use_intra_process_comms: True`
- [ ] 2.7 Create `config/node_acceleration.yaml` with parameter defaults
- [ ] 2.8 Verify: `colcon build` from `04_Robotics/01_Node_Acceleration/` — zero errors, zero warnings
- [ ] 2.9 Verify: `source install/setup.bash && ros2 launch node_acceleration node_acceleration.launch.py` — node reaches ACTIVE state

## 3. C2 Costmap Inflation — ament package

- [ ] 3.1 Create `04_Robotics/02_Costmap_Inflation/package.xml`: name `costmap_inflation`, deps `rclcpp`, `rclcpp_lifecycle`, `rclcpp_components`, `nav_msgs`, `lifecycle_msgs`, `ament_cmake`
- [ ] 3.2 Rewrite `CMakeLists.txt`: `project(costmap_inflation)`, `add_library(costmap_inflation SHARED main.cpp map_publisher.cpp)`, register `CostmapNode` and `MapPublisher` components, `opencl_lab_ros2_install(costmap_inflation)`
- [ ] 3.3 Wrap `CostmapNode` in `namespace costmap_inflation`; add `RCLCPP_COMPONENTS_REGISTER_NODE`; remove `main()`
- [ ] 3.4 Convert `MapPublisher` companion: wrap in namespace; replace CLI11 with `declare_parameter` if applicable; add `RCLCPP_COMPONENTS_REGISTER_NODE`; remove `main()`
- [ ] 3.5 Add `auto_activate` parameter to `CostmapNode::on_configure()`
- [ ] 3.6 Create `launch/costmap_inflation.launch.py`: `ComposableNodeContainer` with both components; `auto_activate`, `inflation_radius`, `resolution`, `decay`, `map_path` as launch args
- [ ] 3.7 Create `config/costmap_inflation.yaml` with parameter defaults
- [ ] 3.8 Verify: `colcon build` — zero errors, zero warnings
- [ ] 3.9 Verify: `source install/setup.bash && ros2 launch costmap_inflation costmap_inflation.launch.py` — node processes map and publishes `/inflated_costmap`

## 4. C3 Perception Node — ament package

- [ ] 4.1 Create `04_Robotics/03_Perception_Node/package.xml`: name `perception_node`, deps `rclcpp`, `rclcpp_lifecycle`, `rclcpp_components`, `sensor_msgs`, `lifecycle_msgs`, `ament_cmake`
- [ ] 4.2 Rewrite `CMakeLists.txt`: `project(perception_node)`, separate `add_library` targets for `perception_node` (from `main.cpp`) and `point_cloud_publisher` (from `point_cloud_publisher.cpp`); register both as components; `opencl_lab_ros2_install` for each
- [ ] 4.3 Wrap `PerceptionNode` in `namespace perception_node`; add `RCLCPP_COMPONENTS_REGISTER_NODE`; remove `main()`
- [ ] 4.4 Convert `PointCloudPublisher` companion: wrap in namespace; replace CLI11 (`--topic`, `--hz`, `--points`, `--scene`) with `declare_parameter`; add `RCLCPP_COMPONENTS_REGISTER_NODE`; remove `main()`
- [ ] 4.5 Add `auto_activate` parameter to `PerceptionNode::on_configure()`
- [ ] 4.6 Create `launch/perception_node.launch.py`: `ComposableNodeContainer` with both components + RViz2 as plain `Node`; all existing C3 launch args (`ground_z`, `min_intensity`, `max_points`, `use_double_buffer`, `topic`, `scene`, `hz`, `points`, `gpu`, `use_rviz`, `auto_activate`); load `config/perception_node.yaml`
- [ ] 4.7 Create `config/perception_node.rviz` (pre-configured: fixed frame `lidar_link`, `/filtered_points` PointCloud2, `/cluster_features` PointCloud2)
- [ ] 4.8 Create `config/perception_node.yaml` with parameter defaults
- [ ] 4.9 Verify: `colcon build` — zero errors, zero warnings
- [ ] 4.10 Verify: `source install/setup.bash && ros2 launch perception_node perception_node.launch.py use_rviz:=false` — `/filtered_points` visible in `ros2 topic echo`
- [ ] 4.11 Verify: `ros2 launch perception_node perception_node.launch.py auto_activate:=false` — node stays INACTIVE; `ros2 lifecycle set /perception_node activate` transitions it

## 5. Voxel Mapping — ament package

- [ ] 5.1 Create `06_Bonus/04_Voxel_Mapping/package.xml`: name `voxel_mapping`, deps `rclcpp`, `rclcpp_components`, `sensor_msgs`, `ament_cmake`
- [ ] 5.2 Rewrite `CMakeLists.txt`: `project(voxel_mapping)`, `add_library` for `voxel_mapping` and `voxel_point_cloud_publisher`; register both; `opencl_lab_ros2_install`
- [ ] 5.3 Wrap `VoxelMappingNode` in `namespace voxel_mapping`; add `RCLCPP_COMPONENTS_REGISTER_NODE`; remove `main()`
- [ ] 5.4 `VoxelPointCloudPublisher` companion: wrap in namespace; convert CLI11 (`--scene`, `--hz`, `--frames`) to `declare_parameter`; add `RCLCPP_COMPONENTS_REGISTER_NODE`; remove `main()`
- [ ] 5.5 Create `launch/voxel_mapping.launch.py`: `ComposableNodeContainer` with both components + RViz2; all args (`topic`, `resolution`, `output`, `enable_flip_filter`, `flip_threshold`, `scene`, `hz`, `frames`, `gpu`, `use_rviz`); load `config/voxel_mapping.yaml`
- [ ] 5.6 Create `config/voxel_mapping.yaml` with parameter defaults
- [ ] 5.7 Verify: `colcon build` — zero errors, zero warnings
- [ ] 5.8 Verify: `source install/setup.bash && ros2 launch voxel_mapping voxel_mapping.launch.py use_rviz:=false` — `/voxel_map` and `/voxel_slice` visible in `ros2 topic list`

## 6. README Updates

- [ ] 6.1 Update `NodeAcceleration.md`: replace `cmake -B build && cmake --build build` with `colcon build`; replace `./build/node_acceleration` with `ros2 launch node_acceleration node_acceleration.launch.py`; add `source install/setup.bash` step; add `auto_activate:=false` + `ros2 lifecycle` example
- [ ] 6.2 Update `CostmapInflation.md`: same pattern
- [ ] 6.3 Update `PerceptionNode.md`: same pattern; include `auto_activate`, `use_rviz`, `gpu` launch arg table
- [ ] 6.4 Update `VoxelMapping.md`: same pattern; note CLI arg → `ros2 param` change for former CLI11 flags
