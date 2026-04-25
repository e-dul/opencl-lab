## 1. C3 PerceptionNode — QoS Update

- [x] 1.1 Replace `create_publisher<PointCloud2>("/filtered_points", 10)` with `rclcpp::SensorDataQoS()`; add WHY comment + doc link above the call
- [x] 1.2 Replace `create_publisher<PointCloud2>("/cluster_features", 10)` with `rclcpp::SensorDataQoS()`; add WHY comment + doc link above the call
- [x] 1.3 Replace `create_subscription<PointCloud2>(..., 10, ...)` (both loaned and copy-based paths) with `rclcpp::SensorDataQoS()`; add WHY comment + doc link above each call
- [x] 1.4 Verify: `cmake -B build && cmake --build build` — zero errors, zero warnings

## 2. Voxel Mapping — QoS Update

- [x] 2.1 Replace `create_subscription<PointCloud2>(..., 10, ...)` with `rclcpp::SensorDataQoS()`; add WHY comment + doc link above the call
- [x] 2.2 Replace `create_publisher<sensor_msgs::msg::PointCloud2>("/voxel_map", 1)` with `rclcpp::QoS(rclcpp::KeepLast(1))`; add WHY comment + doc link above the call
- [x] 2.3 Replace `create_publisher<sensor_msgs::msg::Image>("/voxel_slice", 1)` with `rclcpp::QoS(rclcpp::KeepLast(1))`; add WHY comment + doc link above the call
- [x] 2.4 Verify: `cmake -B build && cmake --build build` — zero errors, zero warnings

## 3. Integration Verification

- [x] 3.1 Run C3: `ros2 launch ./launch/perception_node.launch.py use_rviz:=false` — confirm `/filtered_points` visible in `ros2 topic echo` and `ros2 topic info /filtered_points` shows `QoS profile: BEST_EFFORT`
- [x] 3.2 Run Voxel Mapping: `ros2 launch ./launch/voxel_mapping.launch.py use_rviz:=false` — confirm `/voxel_map` shows `RELIABLE` and `/voxel_slice` shows `RELIABLE` in `ros2 topic info`
