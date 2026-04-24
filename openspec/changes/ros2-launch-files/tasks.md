## 1. C3 PerceptionNode — RViz Config

- [ ] 1.1 Create `04_Robotics/03_Perception_Node/config/perception_node.rviz`: fixed frame `lidar_link`; PointCloud2 display on `/filtered_points` (Style: Points, Size: 0.01, Color: Flat/intensity); PointCloud2 display on `/cluster_features` (Style: Spheres, Size: 0.3, Color: red)
- [ ] 1.2 Verify `ros2 run rviz2 rviz2 -d config/perception_node.rviz` opens with both displays configured (no errors in RViz2 log)

## 2. C3 PerceptionNode — Launch File

- [ ] 2.1 Create `04_Robotics/03_Perception_Node/launch/` directory
- [ ] 2.2 Create `launch/perception_node.launch.py` with launch arguments: `ground_z` (default `0.2`), `min_intensity` (default `10.0`), `max_points` (default `100000`), `use_double_buffer` (default `false`), `topic` (default `/points`), `scene` (default `grid`), `hz` (default `200`), `points` (default `100000`), `gpu` (default `''`), `use_rviz` (default `true`)
- [ ] 2.3 Add `perception_node` `Node` action: executable path `../build/perception_node`; pass `ground_z`, `min_intensity`, `max_points`, `use_double_buffer`, `topic` as ROS parameters; pass `GPU` env var when `gpu` arg is non-empty via `additional_env`
- [ ] 2.4 Add `point_cloud_publisher` `Node` action: executable path `../build/point_cloud_publisher`; pass `--topic`, `--hz`, `--points`, `--scene` as `arguments` (CLI11 flags, not ROS params)
- [ ] 2.5 Add RViz2 `Node` action wrapped in `IfCondition(use_rviz)`: `ros2 run rviz2 rviz2 -d <path-to-config/perception_node.rviz>`
- [ ] 2.6 Test: `ros2 launch ./launch/perception_node.launch.py use_rviz:=false` — both nodes start, `/filtered_points` visible in `ros2 topic list`
- [ ] 2.7 Test: `ros2 launch ./launch/perception_node.launch.py ground_z:=0.5 scene:=mixed hz:=10` — node log shows correct ground_z; publisher uses mixed scene

## 3. C3 PerceptionNode — YAML Config

- [ ] 3.1 Create `04_Robotics/03_Perception_Node/config/perception_node.yaml` with all node parameters at their default values (matching `declare_parameter` defaults in C3)
- [ ] 3.2 Update `launch/perception_node.launch.py` to load `config/perception_node.yaml` via `parameters=[os.path.join(_dir, '..', 'config', 'perception_node.yaml')]` on the `perception_node` Node action; launch arg overrides remain functional

## 4. C3 PerceptionNode — Doc Update

- [ ] 3.1 Update `PerceptionNode.md` Build & Run section: add launch file as primary path above the two-terminal manual commands; document all launch arguments in a table; update Troubleshooting with RViz path note

## 4. Voxel Mapping — Launch File

- [ ] 4.1 Create `06_Bonus/04_Voxel_Mapping/launch/` directory
- [ ] 4.2 Create `launch/voxel_mapping.launch.py` with launch arguments: `topic` (default `/points`), `resolution` (default `0.1`), `output` (default `output_voxel_slice.bmp`), `enable_flip_filter` (default `false`), `flip_threshold` (default `5`), `scene` (default `static`), `hz` (default `10`), `frames` (default `10`), `gpu` (default `''`), `use_rviz` (default `true`)
- [ ] 4.3 Add `voxel_mapping` `Node` action: executable path `../build/voxel_mapping`; pass `topic`, `resolution`, `output`, `enable_flip_filter`, `flip_threshold` as ROS parameters; pass `GPU` env var when non-empty
- [ ] 4.4 Add `voxel_point_cloud_publisher` `Node` action: executable path `../build/voxel_point_cloud_publisher`; pass `--scene`, `--hz`, `--frames` as `arguments` (CLI11 flags)
- [ ] 4.5 Add RViz2 `Node` action wrapped in `IfCondition(use_rviz)`: load existing `../voxel_mapping.rviz`
- [ ] 4.6 Test: `ros2 launch ./launch/voxel_mapping.launch.py use_rviz:=false` — both nodes start, `/voxel_map` and `/voxel_slice` visible in `ros2 topic list`
- [ ] 4.7 Test: `ros2 launch ./launch/voxel_mapping.launch.py resolution:=0.05 scene:=dynamic` — node log shows correct resolution; publisher runs dynamic scene

## 5. Voxel Mapping — YAML Config

- [ ] 5.1 Create `06_Bonus/04_Voxel_Mapping/config/voxel_mapping.yaml` with all node parameters at their default values
- [ ] 5.2 Update `launch/voxel_mapping.launch.py` to load `config/voxel_mapping.yaml` on the `voxel_mapping` Node action

## 6. Voxel Mapping — Doc Update

- [ ] 6.1 Update `VoxelMapping.md` Build & Run section: add launch file as primary path; document all launch arguments; keep existing two-terminal commands as alternative; update the `--enable-flip-filter` example to use launch arg `enable_flip_filter:=true`
