## Why

ROS 2 nodes in C2, C3, and Voxel Mapping declare parameters but lack validation callbacks, descriptors, and runtime feedback — `ros2 param set` silently accepts invalid values, `ros2 param describe` returns nothing useful, and the loaned-message path selection in C3 is invisible. Voxel Mapping uses CLI11 for its main node, violating the lab spec that reserves CLI11 for standalone tool binaries.

## What Changes

- **C2 CostmapInflation**: Add `ParameterDescriptor` with description and floating-point range constraints for `inflation_radius`, `resolution`, `decay`, `map_path`. Add `add_on_set_parameters_callback` that validates ranges and rejects invalid values.
- **C3 PerceptionNode**: Add `add_on_set_parameters_callback` for `ground_z` and `min_intensity` (live-update into member variables; next message uses new values). `max_points` and `use_double_buffer` rejected at runtime with reason "only configurable at configure time". Add RMW detection at `on_activate()`: read `RMW_IMPLEMENTATION` env var, print it, `RCLCPP_WARN` if not a loaned-message-capable RMW. Make loaned vs copy-based path selection explicitly visible in the log.
- **Voxel Mapping**: Convert all CLI11 flags (`--topic`, `--resolution`, `--output`, `--enable-flip-filter`, `--flip-threshold`) to `declare_parameter` in the node constructor. Remove CLI11 include and parsing from `main()`. Replace `/proc/self/exe` kernel-dir hack with `get_binary_dir()`. Add `ParameterDescriptor` for all params.

## Capabilities

### New Capabilities

- `ros2-param-validation`: Parameter descriptors with range constraints and rejection callbacks across C2, C3, and Voxel Mapping nodes.
- `ros2-rmw-verification`: RMW detection and loaned-message path visibility in C3 PerceptionNode.

### Modified Capabilities

*(none — no existing spec-level requirements change; these are additive improvements to implementation quality)*

## Impact

- **Files changed**: `04_Robotics/02_Costmap_Inflation/main.cpp`, `04_Robotics/03_Perception_Node/main.cpp`, `06_Bonus/04_Voxel_Mapping/main.cpp`
- **No CMakeLists.txt changes**: no new ROS 2 packages required (`rcl_interfaces` types are part of `rclcpp`)
- **No kernel changes**
- **No C1 changes**
- **Voxel Mapping CLI change**: `--ros-args -p topic:=/points` replaces `--topic /points` at the command line — documented in `VoxelMapping.md`
