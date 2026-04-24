## 1. C2 CostmapInflation — Parameter Descriptors and Validation

- [x] 1.1 Add `#include <rcl_interfaces/msg/parameter_descriptor.hpp>` to `04_Robotics/02_Costmap_Inflation/main.cpp`
- [x] 1.2 Replace bare `declare_parameter("inflation_radius", 0.5)` with descriptor version: description "Obstacle inflation radius in metres", floating_point_range [0.01, 10.0]
- [x] 1.3 Replace bare `declare_parameter("resolution", 0.05)` with descriptor: description "Metres per cell", floating_point_range (0.0, 1.0] (exclusive lower)
- [x] 1.4 Replace bare `declare_parameter("decay", 3.0)` with descriptor: description "Exponential cost decay rate", floating_point_range (0.0, 100.0]
- [x] 1.5 Replace bare `declare_parameter("map_path", ...)` with descriptor: description "Path to .pgm occupancy grid file" (no range — string param)
- [x] 1.6 Add `add_on_set_parameters_callback` in `on_configure()` after all `declare_parameter` calls; validate `inflation_radius > 0`, `resolution > 0`, `decay > 0`; return descriptive reason strings on failure
- [x] 1.7 Build and verify `ros2 param describe /costmap_node inflation_radius` shows description and range; verify `ros2 param set /costmap_node inflation_radius -1.0` returns failure

## 2. C3 PerceptionNode — Live-Update Params and RMW Verification

- [x] 2.1 Add `#include <rcl_interfaces/msg/parameter_descriptor.hpp>` to `04_Robotics/03_Perception_Node/main.cpp`
- [x] 2.2 Replace bare `declare_parameter` calls with descriptor versions: `topic` (description only), `ground_z` (description + range [−10.0, 50.0]), `min_intensity` (description + range [0.0, 65535.0]), `max_points` (description + integer_range), `use_double_buffer` (description only)
- [x] 2.3 Add `add_on_set_parameters_callback` in `on_configure()`: allow live-update of `ground_z` and `min_intensity` (update member variables); reject `max_points` and `use_double_buffer` with reason "only configurable at configure time (restart required)"; reject `topic` at runtime with same reason
- [x] 2.4 Add WHY comment in the callback explaining that `SingleThreadedExecutor` serialises the param callback and subscription callback — no mutex needed
- [x] 2.5 In `on_activate()`, read `RMW_IMPLEMENTATION` env var via `std::getenv`; log the value with `RCLCPP_INFO`; emit `RCLCPP_WARN` if value is not `rmw_fastrtps_cpp` or `rmw_iceoryx_cpp` (or empty/unset)
- [x] 2.6 Update the loaned subscription try/catch block to log a distinct "Transport: loaned (zero-copy)" or "Transport: copy-based (reason: ...)" line after path selection
- [x] 2.7 Build and verify: `ros2 param set /perception_node ground_z 0.5` succeeds and next message log shows updated filter; `ros2 param set /perception_node max_points 1` returns failure; RMW warning appears with cyclonedds

## 3. VoxelMapping — CLI11 to declare_parameter Migration

- [x] 3.1 Remove `#include <CLI/CLI.hpp>` from `06_Bonus/04_Voxel_Mapping/main.cpp`
- [x] 3.2 Remove CLI11 app definition, `add_option`/`add_flag` calls, and `CLI11_PARSE` from `main()`
- [x] 3.3 Remove the 6-argument constructor signature from `VoxelMappingNode`; remove `kernel_dir` parameter and the `/proc/self/exe` resolution in `main()`
- [x] 3.4 Add `declare_parameter` calls in `VoxelMappingNode` constructor for: `topic` (string, `/points`), `resolution` (double, `0.1`), `output` (string, `output_voxel_slice.bmp`), `enable_flip_filter` (bool, `false`), `flip_threshold` (int64, `5`) — each with a `ParameterDescriptor`
- [x] 3.5 Add `floating_point_range` for `resolution` (0.01–5.0); add `integer_range` for `flip_threshold` (1–UINT32_MAX); add descriptions for all
- [x] 3.6 Add `add_on_set_parameters_callback` in constructor: reject `flip_threshold > UINT32_MAX`; reject `resolution <= 0`
- [x] 3.7 Read params via `get_parameter()` in constructor; assign to member variables; replace `kernel_dir` arg with `get_binary_dir() / "kernels"` using existing `opencl_utils.hpp` helper
- [x] 3.8 Move `rclcpp::init(argc, argv)` before node construction in `main()`; simplify `main()` to: init → make_shared<VoxelMappingNode>() → spin → save_slice → shutdown
- [x] 3.9 Update `VoxelMapping.md` run commands: replace `--topic`, `--resolution`, `--output`, `--enable-flip-filter`, `--flip-threshold` with `--ros-args -p <name>:=<value>` equivalents; update `--help` note to `ros2 param list /voxel_mapping`
- [x] 3.10 Build and verify: `./build/voxel_mapping --ros-args -p topic:=/points -p resolution:=0.1` runs correctly; `./build/voxel_mapping --ros-args -p resolution:=-1.0` is rejected at param set

## 4. Standard Definition of Done

- [x] 4.1 `cmake -B build && cmake --build build` succeeds with zero warnings for C2, C3, and VoxelMapping
- [x] 4.2 All three binaries run without arguments (using defaults) and complete without error
- [x] 4.3 `ros2 param describe` returns description and range for all numeric params in all three nodes
- [x] 4.4 `ros2 param set` with out-of-range value returns failure with descriptive reason for all three nodes
