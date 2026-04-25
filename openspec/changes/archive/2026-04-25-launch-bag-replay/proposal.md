## Why

The Recording & Replay workflow currently requires two terminals: one for the launch file and one for `ros2 bag play`. A `bag` launch argument lets users drive any ROS 2 module entirely from one command, replacing the synthetic publisher with a pre-recorded bag.

## What Changes

- Add optional `bag` launch argument (default `''`) to three launch files:
  - `04_Robotics/01_Node_Acceleration/launch/node_acceleration.launch.py`
  - `04_Robotics/03_Perception_Node/launch/perception_node.launch.py`
  - `06_Bonus/04_Voxel_Mapping/launch/voxel_mapping.launch.py`
- When `bag` is non-empty: suppress the synthetic publisher node; add an `ExecuteProcess` action that runs `ros2 bag play <path>`.
- When `bag` is empty: behaviour unchanged (synthetic publisher starts as before).
- Update the Recording & Replay stub in each module README to show the one-command invocation.

## Capabilities

### New Capabilities

- none

### Modified Capabilities

- `ros2-launch-orchestration`: launch files gain a `bag` argument that conditionally replaces the synthetic publisher with `ros2 bag play`.

## Impact

- Three Python launch files (code change).
- Three module READMEs (doc-only update to Recording & Replay stub).
- No changes to node source, CMake, or RViz config.
- No breaking changes — omitting `bag` preserves existing behaviour exactly.
