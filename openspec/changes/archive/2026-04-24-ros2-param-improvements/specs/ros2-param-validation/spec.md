## ADDED Requirements

### Requirement: Parameter descriptors on all declared params
Every `declare_parameter` call in C2 CostmapNode, C3 PerceptionNode, and VoxelMappingNode SHALL include a `rcl_interfaces::msg::ParameterDescriptor` with a non-empty `description` field. Numeric params SHALL include `floating_point_range` or `integer_range` constraints where a valid domain exists.

#### Scenario: ros2 param describe returns description
- **WHEN** `ros2 param describe /costmap_node inflation_radius` is called while the node is running
- **THEN** the output includes a human-readable description and the valid range (0.01–10.0)

#### Scenario: ros2 param describe on string param returns description
- **WHEN** `ros2 param describe /costmap_node map_path` is called
- **THEN** the output includes a description; no range constraint is shown

### Requirement: Parameter validation callback rejects out-of-range values
C2 CostmapNode and C3 PerceptionNode SHALL register `add_on_set_parameters_callback` in `on_configure()`. VoxelMappingNode SHALL register it in the constructor. The callback SHALL return `result.successful = false` with a descriptive `reason` string for any parameter value that violates its range constraint.

#### Scenario: C2 rejects negative inflation_radius
- **WHEN** `ros2 param set /costmap_node inflation_radius -0.5` is called
- **THEN** the call returns failure with reason containing "inflation_radius must be > 0"

#### Scenario: C2 rejects zero resolution
- **WHEN** `ros2 param set /costmap_node resolution 0.0` is called
- **THEN** the call returns failure with reason containing "resolution must be > 0"

#### Scenario: C2 accepts valid inflation_radius
- **WHEN** `ros2 param set /costmap_node inflation_radius 1.5` is called
- **THEN** the call returns success

#### Scenario: C3 rejects negative ground_z
- **WHEN** `ros2 param set /perception_node ground_z -1.0` is called
- **THEN** the call returns failure with a descriptive reason

#### Scenario: C3 rejects configure-time-only param at runtime
- **WHEN** `ros2 param set /perception_node max_points 50000` is called while the node is ACTIVE
- **THEN** the call returns failure with reason containing "only configurable at configure time"

### Requirement: C3 live-update params take effect on next message
`ground_z` and `min_intensity` in C3 PerceptionNode SHALL be updated in the member variables within the parameter callback. The updated value SHALL be used for all subsequent `run_pipeline` calls without node restart.

#### Scenario: ground_z change affects next callback
- **WHEN** `ros2 param set /perception_node ground_z 0.5` is called while the node is ACTIVE
- **THEN** the next received PointCloud2 message is filtered using `ground_z = 0.5`

### Requirement: VoxelMappingNode uses declare_parameter instead of CLI11
VoxelMappingNode SHALL declare all tunable values as ROS 2 parameters (`topic`, `resolution`, `output`, `enable_flip_filter`, `flip_threshold`). The node binary SHALL NOT require CLI11 parsing in `main()`. Parameters SHALL be set via `--ros-args -p <name>:=<value>` at the command line.

#### Scenario: topic parameter overridable via ros-args
- **WHEN** `./build/voxel_mapping --ros-args -p topic:=/lidar/points` is invoked
- **THEN** the node subscribes to `/lidar/points`

#### Scenario: flip_threshold guards against overflow
- **WHEN** `ros2 param set /voxel_mapping flip_threshold 9999999999` is called (> UINT32_MAX)
- **THEN** the call returns failure with reason containing "flip_threshold exceeds UINT32_MAX"
