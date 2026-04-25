## Why

C3 PerceptionNode and Voxel Mapping each require running two binaries in separate terminals with manually copied `--ros-args` flags and environment variables. There are no launch files anywhere in the lab. Launch files are the standard ROS 2 mechanism for orchestrating multi-node setups, passing parameters, and starting visualisation — their absence is the single largest gap between the lab and industry ROS 2 practice.

This change depends on `ros2-param-improvements` being applied first (Voxel Mapping must use `declare_parameter` before its launch file can pass parameters cleanly).

## What Changes

- **C3 PerceptionNode**: Add `launch/perception_node.launch.py` that starts `perception_node` and `point_cloud_publisher` with configurable parameters and starts RViz2 by default (opt-out via `use_rviz:=false`). Add `config/perception_node.rviz` pre-configured for `/filtered_points` PointCloud2 display with fixed frame `lidar_link`.
- **Voxel Mapping**: Add `launch/voxel_mapping.launch.py` that starts `voxel_mapping` and `voxel_point_cloud_publisher` with configurable parameters and starts RViz2 by default using the existing `voxel_mapping.rviz` (opt-out via `use_rviz:=false`).
- **Docs**: Update `PerceptionNode.md` and `VoxelMapping.md` with launch file usage as the primary run path; keep the two-terminal manual path as an alternative.

## Capabilities

### New Capabilities

- `ros2-launch-orchestration`: Launch files for C3 and Voxel Mapping that orchestrate multi-node startup with parameter passing and optional RViz2.

### Modified Capabilities

*(none — additive only; no existing spec requirements change)*

## Impact

- **New files**: `04_Robotics/03_Perception_Node/launch/perception_node.launch.py`, `04_Robotics/03_Perception_Node/config/perception_node.rviz`, `06_Bonus/04_Voxel_Mapping/launch/voxel_mapping.launch.py`
- **Updated files**: `04_Robotics/03_Perception_Node/PerceptionNode.md`, `06_Bonus/04_Voxel_Mapping/VoxelMapping.md`
- **No C++ changes**, **no CMakeLists.txt changes**, **no kernel changes**
- **Depends on**: `ros2-param-improvements` applied (Voxel Mapping `declare_parameter` conversion)
- **No C1 or C2 changes** — C1 and C2 are self-contained single-binary designs by intent
