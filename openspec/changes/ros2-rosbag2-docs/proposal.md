## Why

All three ROS 2 modules (C1, C3, Voxel Mapping) rely on a synthetic publisher binary to drive the node under test. There is no documentation showing how to capture a bag from that publisher and replay it — the standard development workflow for validating a node against recorded real-world or synthetic data without keeping the publisher running. This is a pure documentation gap; no code changes are needed.

## What Changes

- **NodeAcceleration.md**: Add "Recording & Replay" section — record `/raw_floats`, inspect the bag, replay it as a drop-in replacement for `synthetic_publisher`.
- **PerceptionNode.md**: Add "Recording & Replay" section — record `/points`, replay against `perception_node` running in isolation.
- **VoxelMapping.md**: Add "Recording & Replay" section — record `/points`, replay against `voxel_mapping` running in isolation.

Each section covers: `ros2 bag record`, `ros2 bag info`, `ros2 bag play`, the `--rate` flag, and the workflow of stopping the publisher and substituting the bag.

## Capabilities

### New Capabilities

*(none — documentation only)*

### Modified Capabilities

*(none)*

## Impact

- **Files changed**: `04_Robotics/01_Node_Acceleration/NodeAcceleration.md`, `04_Robotics/03_Perception_Node/PerceptionNode.md`, `06_Bonus/04_Voxel_Mapping/VoxelMapping.md`
- **No C++ changes**, **no CMakeLists changes**, **no kernel changes**, **no launch file changes**
- **Depends on**: `ros2-ament-migration` applied first — run commands in the sections use `ros2 launch <package>` and `source install/setup.bash`, not `./build/<binary>`
