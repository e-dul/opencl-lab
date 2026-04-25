# Spec: ros2-qos-profiles

## Purpose
Defines QoS profile requirements for ROS 2 publishers and subscriptions in the C3 PerceptionNode and Voxel Mapping modules, ensuring correct reliability/durability settings for sensor stream and map output topics.

## Requirements

### Requirement: Sensor stream topics use SensorDataQoS
All ROS 2 publishers and subscriptions on PointCloud2 sensor stream topics (input and filtered output) in C3 PerceptionNode and Voxel Mapping SHALL use `rclcpp::SensorDataQoS()` (`BestEffort`, `Volatile`, `KeepLast(5)`).

#### Scenario: C3 publishers use SensorDataQoS
- **WHEN** C3 PerceptionNode creates `/filtered_points` and `/cluster_features` publishers
- **THEN** both SHALL be constructed with `rclcpp::SensorDataQoS()`

#### Scenario: C3 subscription uses SensorDataQoS
- **WHEN** C3 PerceptionNode subscribes to the input point cloud topic
- **THEN** the subscription SHALL be constructed with `rclcpp::SensorDataQoS()`

#### Scenario: Voxel Mapping subscription uses SensorDataQoS
- **WHEN** Voxel Mapping subscribes to the input point cloud topic
- **THEN** the subscription SHALL be constructed with `rclcpp::SensorDataQoS()`

### Requirement: Voxel Mapping output topics use explicit KeepLast(1)
Voxel Mapping publishers on `/voxel_map` and `/voxel_slice` SHALL use `rclcpp::QoS(rclcpp::KeepLast(1))` (Reliable, Volatile).

#### Scenario: Voxel output publishers use KeepLast(1)
- **WHEN** Voxel Mapping creates `/voxel_map` and `/voxel_slice` publishers
- **THEN** both SHALL be constructed with `rclcpp::QoS(rclcpp::KeepLast(1))`

### Requirement: QoS rationale comment on every pub/sub call
Every `create_publisher` and `create_subscription` call in C3 PerceptionNode and Voxel Mapping that specifies a QoS profile SHALL have a one-line WHY comment and a reference link to the ROS 2 QoS documentation.

#### Scenario: Comment present at call site
- **WHEN** a developer reads the `create_publisher` or `create_subscription` call
- **THEN** a comment SHALL explain the reliability policy choice and link to https://docs.ros.org/en/jazzy/Concepts/Intermediate/About-Quality-of-Service-Settings.html
