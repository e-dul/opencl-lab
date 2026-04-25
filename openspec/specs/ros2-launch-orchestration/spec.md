## ADDED Requirements

### Requirement: C3 launch file starts both nodes in one command
A Python launch file at `04_Robotics/03_Perception_Node/launch/perception_node.launch.py` SHALL start `perception_node` and `point_cloud_publisher` as separate processes when invoked with `ros2 launch ./launch/perception_node.launch.py`.

#### Scenario: Both processes start
- **WHEN** `ros2 launch ./launch/perception_node.launch.py` is run from `03_Perception_Node/`
- **THEN** both `perception_node` and `point_cloud_publisher` processes appear in `ros2 node list`

#### Scenario: Parameters overridable via launch args
- **WHEN** `ros2 launch ./launch/perception_node.launch.py ground_z:=0.5 scene:=mixed` is run
- **THEN** `perception_node` uses `ground_z=0.5` and `point_cloud_publisher` uses `--scene mixed`

#### Scenario: GPU selection via launch arg
- **WHEN** `ros2 launch ./launch/perception_node.launch.py gpu:=NVIDIA` is run
- **THEN** `perception_node` receives `GPU=NVIDIA` in its environment

### Requirement: C3 launch file starts RViz2 by default with pre-configured display
The C3 launch file SHALL start RViz2 using `config/perception_node.rviz` by default. A `use_rviz` launch argument (default `true`) SHALL control whether RViz2 is started.

#### Scenario: RViz starts by default
- **WHEN** `ros2 launch ./launch/perception_node.launch.py` is run
- **THEN** an RViz2 process starts displaying `/filtered_points` PointCloud2 with fixed frame `lidar_link`

#### Scenario: RViz suppressed with use_rviz:=false
- **WHEN** `ros2 launch ./launch/perception_node.launch.py use_rviz:=false` is run
- **THEN** no RViz2 process is started

### Requirement: C3 RViz config pre-configures key displays
`config/perception_node.rviz` SHALL pre-configure: fixed frame `lidar_link`, PointCloud2 display on `/filtered_points` (Points style), PointCloud2 display on `/cluster_features` (Spheres style, size ≥ 0.2m).

#### Scenario: filtered_points visible without manual setup
- **WHEN** RViz opens with `perception_node.rviz` and point_cloud_publisher is running
- **THEN** filtered points are visible without any manual display configuration

### Requirement: Voxel Mapping launch file starts both nodes in one command
A Python launch file at `06_Bonus/04_Voxel_Mapping/launch/voxel_mapping.launch.py` SHALL start `voxel_mapping` and `voxel_point_cloud_publisher` as separate processes.

#### Scenario: Both processes start
- **WHEN** `ros2 launch ./launch/voxel_mapping.launch.py` is run from `04_Voxel_Mapping/`
- **THEN** both `voxel_mapping` and `voxel_point_cloud_publisher` appear in `ros2 node list`

#### Scenario: Parameters overridable via launch args
- **WHEN** `ros2 launch ./launch/voxel_mapping.launch.py resolution:=0.05 scene:=dynamic` is run
- **THEN** `voxel_mapping` uses `resolution=0.05` and `voxel_point_cloud_publisher` uses `--scene dynamic`

### Requirement: Voxel Mapping launch file starts RViz2 by default using existing rviz config
The Voxel Mapping launch file SHALL start RViz2 using the existing `voxel_mapping.rviz` by default. A `use_rviz` launch argument (default `true`) SHALL control whether RViz2 starts.

#### Scenario: RViz starts with existing config
- **WHEN** `ros2 launch ./launch/voxel_mapping.launch.py` is run
- **THEN** RViz2 starts with `voxel_mapping.rviz` loaded

#### Scenario: RViz suppressed with use_rviz:=false
- **WHEN** `ros2 launch ./launch/voxel_mapping.launch.py use_rviz:=false` is run
- **THEN** no RViz2 process is started

### Requirement: Docs updated to show launch file as primary run path
`PerceptionNode.md` and `VoxelMapping.md` SHALL show `ros2 launch` as the primary run command. The existing two-terminal manual invocation SHALL be retained as an alternative.

#### Scenario: User can follow launch path in docs
- **WHEN** a user follows the Build & Run section of PerceptionNode.md
- **THEN** the first run command shown is `ros2 launch ./launch/perception_node.launch.py`
