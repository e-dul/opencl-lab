## 1. node_acceleration.launch.py

- [x] 1.1 Add `DeclareLaunchArgument('bag', default_value='', description='Path to a rosbag2 bag directory; suppresses the synthetic publisher when set.')` before `OpaqueFunction`
- [x] 1.2 Refactor `generate_launch_description` to use `OpaqueFunction(function=launch_setup)` — move container construction into `launch_setup(context, *args, **kwargs)` with `# WHY OpaqueFunction` comment
- [x] 1.3 In `launch_setup`: resolve `bag = LaunchConfiguration('bag').perform(context)`; build `nodes = [accel_node]` if bag else `[accel_node, synthetic_publisher]`; append `ExecuteProcess(cmd=['ros2', 'bag', 'play', bag_path], output='screen')` when bag is set

## 2. perception_node.launch.py

- [x] 2.1 Add `DeclareLaunchArgument('bag', ...)` with same description
- [x] 2.2 Refactor to `OpaqueFunction` with `launch_setup`; move container + RViz `Node` into the function return list
- [x] 2.3 In `launch_setup`: exclude `PointCloudPublisher` composable node when `bag` is set; add `ExecuteProcess` for bag play; always include RViz `Node` (respects existing `use_rviz` arg via `IfCondition`)

## 3. voxel_mapping.launch.py

- [x] 3.1 Add `DeclareLaunchArgument('bag', ...)` with same description
- [x] 3.2 Refactor to `OpaqueFunction` with `launch_setup`
- [x] 3.3 In `launch_setup`: exclude `VoxelCloudPublisher` composable node when `bag` is set; add `ExecuteProcess` for bag play; always include RViz `Node`

## 4. Documentation

- [x] 4.1 Update `NodeAcceleration.md` Recording & Replay stub: remove intra-process constraint note; add `ros2 launch node_acceleration node_acceleration.launch.py bag:=accel_bag` one-liner; update topic to `/raw_floats` (record input, not output)
- [x] 4.2 Update `PerceptionNode.md` Recording & Replay stub: add `ros2 launch perception_node perception_node.launch.py bag:=perception_bag` one-liner
- [x] 4.3 Update `VoxelMapping.md` Recording & Replay stub: add `ros2 launch voxel_mapping voxel_mapping.launch.py bag:=voxel_bag` one-liner
