## Why

C3 PerceptionNode logs a timing table on every processed message via `RCLCPP_INFO`; Voxel Mapping prints one via `std::cout`. At 200 Hz input both terminals become unreadable within seconds, and the performance signal is buried in noise. There is no way to monitor either node's health without reading a scrolling log. `diagnostic_updater` is the standard ROS 2 mechanism for structured, 1 Hz node health reports — consumed by `rqt_robot_monitor` and the `/diagnostics` topic.

## What Changes

- **C3 PerceptionNode**: Remove per-message `RCLCPP_INFO` timing table. Add a rolling accumulator (100-message window) for GPU stage times. Register two diagnostic tasks: `GPU pipeline` (avg/min/max kernel ms, messages processed, avg points in/out) and `Config` (live parameter snapshot). Status bit encodes a soft performance gate.
- **Voxel Mapping**: Remove per-message `std::cout` timing block. Add the same rolling accumulator. Register two diagnostic tasks: `GPU pipeline` (avg/min/max kernel ms, messages processed, avg points in, voxels_occupied count) and `Config` (resolution, flip_filter, flip_threshold, topic). Status bit encodes the existing 5 ms gate.
- **CMakeLists.txt** (both modules): Add `find_package(diagnostic_updater REQUIRED)` and link `diagnostic_updater::diagnostic_updater`.
- **C1, C2**: No changes. C1 has no ROS topics; C2 is fire-once (a diagnostic would immediately go `STALE`).

## Capabilities

### New Capabilities

- `ros2-diagnostics`: Rolling GPU timing accumulators and `diagnostic_updater` integration in C3 and Voxel Mapping, replacing per-message terminal output with structured 1 Hz health reports.

### Modified Capabilities

*(none — no existing spec-level requirements change)*

## Impact

- **Files changed**: `04_Robotics/03_Perception_Node/main.cpp`, `04_Robotics/03_Perception_Node/CMakeLists.txt`, `06_Bonus/04_Voxel_Mapping/main.cpp`, `06_Bonus/04_Voxel_Mapping/CMakeLists.txt`
- **No kernel changes**, **no launch file changes**
- **New dependency**: `diagnostic_updater` (part of `ros-jazzy-ros-base`; no extra `apt install`)
- **Depends on**: `ros2-param-improvements` applied first — Config diagnostic snapshot reads the member variables that param callbacks update (`ground_z_`, `min_intensity_`, etc.)
- **Depends on**: `ros2-ament-migration` applied first — CMakeLists changes use `ament_target_dependencies(diagnostic_updater)` not standalone `find_package` + `target_link_libraries`
