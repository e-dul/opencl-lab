# Task 039: C3 — Accelerated Perception Node

## Context
- **Design Feature:** `workflow/design/06-robotics-ros2-projects.md`
- **Milestone:** Phase 4 — C3 Perception Node (Flagship)
- **Relevant Files:**
  - `workflow/design/06-robotics-ros2-projects.md` — architecture reference (read-only)
  - `02_Projects/C_Robotics_ROS2/C1_Node_Acceleration/` — LifecycleNode pattern reference (read-only)
  - `02_Projects/C_Robotics_ROS2/C2_Costmap_Inflation/` — two-node-same-binary pattern reference (read-only)
  - `common/ocl_wrapper.hpp` — `create_context()` (read-only)
  - `common/opencl_utils.hpp` — `CL_CHECK` macro (read-only)
  - `02_Projects/C_Robotics_ROS2/C3_Perception_Node/` — new directory (create)

## Objective
Implement `C3_Perception_Node`: a `rclcpp_lifecycle::LifecycleNode` that subscribes to `sensor_msgs/PointCloud2`, runs a four-stage GPU pipeline (upload → filter → compact → feature extract), publishes `/filtered_points` and `/cluster_features`, and achieves < 5 ms end-to-end latency at 100k points.

## Constraints & Rules
- Standard constraints from `.claude/rules/00_master_specs.md` apply (C++17, `cl.hpp`, `CL_CHECK`, `create_context()`, standalone CMake, `CL_QUEUE_PROFILING_ENABLE`).
- **Hard ROS 2 dependency**: CMake must emit `message(FATAL_ERROR ...)` with clear instructions if `$ENV{ROS_DISTRO}` is unset or `rclcpp` is not found.
- **Node parameters via `declare_parameter`** (not CLI11): `topic` (string, default `"/points"`), `ground_z` (double, default `0.2`), `min_intensity` (double, default `10.0`).
- **Synthetic publisher** (`point_cloud_publisher.cpp`) is a separate `add_executable` target. Uses CLI11 flags: `--topic`, `--hz`, `--points`.
- **`point_step` must be read from the message header** — never hard-coded. XYZ + intensity (float32, 16 bytes/point) is the default test layout.
- **`/cluster_features`** published as `sensor_msgs/PointCloud2` with fields: centroid XYZ + intensity mean + point count (all float32). No custom message types.
- **Loaned messages**: standard copy-based path is the baseline; loaned message path is optional. If the loaned path is unavailable at runtime, the node must print a descriptive warning and continue — crash or silent failure is forbidden.
- **No BMP output**: C3 is a numeric pipeline benchmark. Structured per-message console timing table is the required artifact (master spec §3 exception).
- **RMW warning**: CMake must check `$ENV{RMW_IMPLEMENTATION}` and emit `message(WARNING ...)` if not set to a loaned-message-capable RMW (not a fatal error).
- **Integer arithmetic safety**: buffer size from `point_step * num_points` must promote before multiply (master spec §7.1).

---

## Implementation

1. **Scaffold `C3_Perception_Node/` directory**
   - Create `CMakeLists.txt`, `main.cpp`, `point_cloud_publisher.cpp`, and `kernels/` directory.
   - `CMakeLists.txt` defines two targets: `perception_node` (main.cpp) and `point_cloud_publisher` (point_cloud_publisher.cpp).
   - Check `$ENV{ROS_DISTRO}` before `find_package`; emit fatal error if unset.
   - Check `$ENV{RMW_IMPLEMENTATION}` and emit warning if not a loaned-capable RMW.
   - Copy `kernels/` to binary dir via `add_custom_command` POST_BUILD for both targets.
   - Set `CMAKE_CXX_STANDARD 17` and `CMAKE_CXX_EXTENSIONS OFF`.

2. **Implement `PerceptionNode` (`main.cpp`)**
   - `rclcpp_lifecycle::LifecycleNode` subclass.
   - `on_configure()`: call `create_context()`, create `cl::CommandQueue` with `CL_QUEUE_PROFILING_ENABLE`, build kernels from `kernels/filter.cl`, `kernels/prefix_sum.cl`, `kernels/feature_extract.cl`; log init time via `std::chrono::steady_clock`. Pre-allocate `cl::Buffer` for `--points`-sized cloud (default 100k).
   - `on_activate()`: subscribe `/points` (`sensor_msgs/PointCloud2`) — topic name from `topic` parameter; advertise `/filtered_points` and `/cluster_features`.
   - **Subscription callback** (per-message pipeline):
     1. CPU: record deserialization/start via `steady_clock`. Read `point_step` from message header. Compute buffer size as `static_cast<size_t>(point_step) * num_points`.
     2. GPU Upload: `enqueueWriteBuffer` (non-blocking, `CL_FALSE`) with `cl::Event`; `CL_CHECK` wrapping.
     3. GPU Filter: dispatch `filter_kernel_` (ground removal `z >= ground_z` + intensity `>= min_intensity`, single pass, predicate mask output) → `cl::Event`.
     4. GPU Compact: dispatch `prefix_sum_kernel_` (Blelloch two-phase scan on predicate mask → scatter compacted points to output buffer) → `cl::Event`.
     5. GPU Feature Extract: dispatch `feature_extract_kernel_` (per-cluster centroid + intensity mean + count) → `cl::Event`.
     6. GPU Download: `enqueueReadBuffer` (compacted points + cluster features, blocking) → `cl::Event`; `CL_CHECK` wrapping.
     7. CPU Publish: construct and publish `filtered_points` + `cluster_features`. Record publish time via `steady_clock`.
     8. Log per-stage breakdown (upload ms, filter ms, compact ms, feature ms, download ms, publish ms, total ms) with `cl::Event` profiling for GPU stages and `steady_clock` for CPU stages. All times to 3 decimal places.
   - `on_deactivate()`: destroy subscription and publishers.
   - `on_cleanup()`: RAII destroys all `cl::` members.
   - `main()`: `rclcpp::init` → create `PerceptionNode` in a single-threaded executor → drive lifecycle transitions (`configure` → `activate`) programmatically.

3. **Write kernels**
   - `kernels/filter.cl`: input `__global float* points` (AoS, stride = `point_step/4` floats), `__global uchar* mask`, `float ground_z`, `float min_intensity`, `int point_step_floats`, `int num_points`. Each work-item reads one point's z and intensity fields; writes 1/0 to `mask[gid]`. Guard: `if (gid >= (size_t)num_points) return;`.
   - `kernels/prefix_sum.cl`: two-phase Blelloch exclusive scan on `__global int* data`, `int n`. Phase 1 (up-sweep): reduce tree. Phase 2 (down-sweep): propagate. Result is exclusive prefix sum used as scatter indices for compaction scatter kernel. Correctness must match CPU scan on same data.
   - `kernels/feature_extract.cl`: input compacted `__global float* compact_points`, count from prefix sum result. Each work-item handles one point; atomic adds to per-cluster accumulators for centroid XYZ, intensity sum, count. Use `atomic_add` on `__global int*` accumulators (fixed-point scaled by 1000). Output: `__global float* features` (centroid XYZ, intensity mean, count per cluster as flat float array).

4. **Implement `point_cloud_publisher.cpp` (separate binary)**
   - Standalone `rclcpp::Node`; not a LifecycleNode.
   - CLI11 flags: `--topic` (string, default `"/points"`), `--hz` (int, default `200`), `--points` (int, default `100000`).
   - Timer at `1000/hz` ms interval: generate synthetic `PointCloud2` (XYZ + intensity float32, 16 bytes/point; random or grid layout); publish to `--topic`.
   - Runs until Ctrl-C.

5. **Verify correctness of prefix-sum compaction**
   - Add a debug/assert path (active only in Debug builds, guarded by `#ifndef NDEBUG`): after GPU compact, read back the compact count, run CPU exclusive scan on same mask, compare scatter output. Log `COMPACTION OK` or `COMPACTION MISMATCH: GPU=N CPU=M`.

---

## Definition of Done (DoD)

Standard items from `.claude/rules/00_master_specs.md` §8 apply.

- [ ] `cmake -B build && cmake --build build` succeeds with zero errors and zero warnings from `C3_Perception_Node/`.
- [ ] Both binaries (`perception_node`, `point_cloud_publisher`) exist in the build output.
- [ ] `./build/perception_node --help` prints CLI11-style usage from underlying node parameters.
- [ ] Running `./build/point_cloud_publisher` in one terminal and `./build/perception_node` in another produces per-message timing logs in `perception_node` console output.
- [ ] Per-message timing table printed to console: columns for upload, filter, compact, feature extract, download, publish (ms, 3 decimal places), and total.
- [ ] `GPU=<vendor> ./build/perception_node` selects the correct device without crashing.
- [ ] CMake emits `FATAL_ERROR` when `ROS_DISTRO` env var is unset (verified by running `cmake -B build` without sourcing ROS 2).
- [ ] CMake emits `WARNING` when `RMW_IMPLEMENTATION` is not set to a loaned-capable RMW.
- [ ] Debug build: compaction correctness log prints `COMPACTION OK` on the first processed message.
- [ ] MANUAL: Source ROS 2 Jazzy, run `point_cloud_publisher` (100k points, 200 Hz) and `perception_node` side-by-side; confirm per-message total latency < 5 ms on at least 10 consecutive messages on a discrete GPU. †
- [ ] MANUAL: Confirm `/filtered_points` and `/cluster_features` topics are visible via `ros2 topic list` during a live run.
- [ ] MANUAL: Confirm node lifecycle transitions (`configure` → `activate` → `deactivate` → `cleanup`) complete without error in the console log.

---

## Execution Report
<!-- Filled by @coder after implementation. -->

- **Status:** PENDING
- **Session:** —

### Validation
```
[output here]
```

### Changed Files
| File | Change |
|------|--------|
| `02_Projects/C_Robotics_ROS2/C3_Perception_Node/CMakeLists.txt` | Created |
| `02_Projects/C_Robotics_ROS2/C3_Perception_Node/main.cpp` | Created |
| `02_Projects/C_Robotics_ROS2/C3_Perception_Node/point_cloud_publisher.cpp` | Created |
| `02_Projects/C_Robotics_ROS2/C3_Perception_Node/kernels/filter.cl` | Created |
| `02_Projects/C_Robotics_ROS2/C3_Perception_Node/kernels/prefix_sum.cl` | Created |
| `02_Projects/C_Robotics_ROS2/C3_Perception_Node/kernels/feature_extract.cl` | Created |
