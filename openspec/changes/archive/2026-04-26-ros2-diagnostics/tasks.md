## 1. C3 PerceptionNode — CMakeLists

- [x] 1.1 Add `find_package(diagnostic_updater REQUIRED)` to `04_Robotics/03_Perception_Node/CMakeLists.txt`
- [x] 1.2 Add `diagnostic_updater::diagnostic_updater` to the `target_link_libraries` call for `perception_node`

## 2. C3 PerceptionNode — Rolling Accumulator

- [x] 2.1 Add `#include <diagnostic_updater/diagnostic_updater.hpp>` to includes
- [x] 2.2 Add private members: `diagnostic_updater::Updater updater_`; circular buffer `std::array<double, 100> timing_buf_`; `size_t timing_idx_ = 0`; `size_t timing_count_ = 0`; `uint64_t msg_count_ = 0`; `double points_in_acc_ = 0.0`; `double points_out_acc_ = 0.0`
- [x] 2.3 In `on_configure()`: initialise `updater_` with `updater_.setHardwareID("none")`; register `"GPU pipeline"` task and `"Config"` task via `updater_.add()`
- [x] 2.4 In `run_pipeline()` (or equivalent): after computing `total_ms`, write into `timing_buf_[timing_idx_++ % 100]`; increment `timing_count_`; accumulate `points_in_acc_` and `points_out_acc_`
- [x] 2.5 Remove the per-message `RCLCPP_INFO` timing table (lines ~615–623); replace with the accumulator writes from 2.4
- [x] 2.6 Implement `gpu_pipeline_diag()` callback: compute `min/max/avg` over `std::min(timing_count_, 100UL)` samples; set status OK/WARN/ERROR per spec thresholds (10/25 ms); populate all key-value pairs
- [x] 2.7 Implement `config_diag()` callback: `stat.summary(OK, "")`, `stat.add("ground_z", ground_z_)`, `stat.add("min_intensity", min_intensity_)`, `stat.add("max_points", max_points_)`, `stat.add("double_buffer", use_double_buffer_)`, `stat.add("rmw", rmw_impl_)`
- [x] 2.8 Add WHY comment above updater init explaining SingleThreadedExecutor serialisation guarantee (no mutex needed)
- [x] 2.9 Verify: `cmake -B build && cmake --build build` — zero errors, zero warnings

## 3. C3 PerceptionNode — Integration Check

- [x] 3.1 Run `ros2 launch ./launch/perception_node.launch.py use_rviz:=false`
- [x] 3.2 In a second terminal: `ros2 topic echo /diagnostics` — confirm both `"GPU pipeline"` and `"Config"` status messages appear at ~1 Hz
- [x] 3.3 Confirm terminal running the node shows no per-message timing output

## 4. Voxel Mapping — CMakeLists

- [x] 4.1 Add `find_package(diagnostic_updater REQUIRED)` to `06_Bonus/04_Voxel_Mapping/CMakeLists.txt`
- [x] 4.2 Add `diagnostic_updater::diagnostic_updater` to the `target_link_libraries` call for `voxel_mapping`

## 5. Voxel Mapping — Rolling Accumulator

- [x] 5.1 Add `#include <diagnostic_updater/diagnostic_updater.hpp>` to includes
- [x] 5.2 Add private members: `diagnostic_updater::Updater updater_`; `std::array<double, 100> timing_buf_`; `size_t timing_idx_ = 0`; `size_t timing_count_ = 0`; `uint64_t msg_count_ = 0`; `double points_in_acc_ = 0.0`; `uint64_t voxels_occupied_ = 0`
- [x] 5.3 In constructor: initialise `updater_` with `updater_.setHardwareID("none")`; register `"GPU pipeline"` and `"Config"` tasks
- [x] 5.4 In the subscription callback: after computing `total_ms`, write into accumulator; increment counters; count occupied voxels from the host-side grid (iterate grid and count non-zero cells, or reuse existing shutdown BMP logic)
- [x] 5.5 Remove the per-message `std::cout` timing block (lines ~460–471); replace with accumulator writes from 5.4
- [x] 5.6 Implement `gpu_pipeline_diag()` callback: OK/WARN/ERROR at 5/15 ms thresholds; populate `avg_kernel_ms`, `min_kernel_ms`, `max_kernel_ms`, `messages_processed`, `points_in_avg`, `voxels_occupied`, `total_voxels`
- [x] 5.7 Implement `config_diag()` callback: `stat.add("resolution", resolution_)`, `stat.add("flip_filter", enable_flip_)`, `stat.add("flip_threshold", flip_threshold_)`, `stat.add("topic", topic_)`
- [x] 5.8 Add WHY comment above updater init (same SingleThreadedExecutor note as C3)
- [x] 5.9 Verify: `cmake -B build && cmake --build build` — zero errors, zero warnings

## 6. Voxel Mapping — Integration Check

- [x] 6.1 Run `ros2 launch ./launch/voxel_mapping.launch.py use_rviz:=false`
- [x] 6.2 In a second terminal: `ros2 topic echo /diagnostics` — confirm both tasks appear at ~1 Hz
- [x] 6.3 Confirm terminal shows no per-message `std::cout` timing output
