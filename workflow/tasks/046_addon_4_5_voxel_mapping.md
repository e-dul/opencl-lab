# Task 046: 4.5 Voxel Mapping

## Context

- **Design Feature:** `workflow/design/08-addons.md`
- **Milestone:** Phase 5 — 4.5 Voxel Mapping
- **Relevant Files:**
  - `workflow/design/08-addons.md` — (read-only: architecture, data flow, specs)
  - `04_Addons/4_5_Voxel_Mapping/VoxelMapping.md` — (read-only: educational README / user-facing docs)
  - `common/ocl_wrapper.hpp` — (read-only: `create_context()`, `GPU` env var selection)
  - `common/common.cmake` — (read-only: `opencl_lab_ros2_guard()`, `opencl_lab_ros2_target()`, `copy_kernels()`)
  - `common/image_utils.hpp` — (read-only: BMP output helpers)
  - `02_Projects/C_Robotics_ROS2/C3_Perception_Node/point_cloud_publisher.cpp` — (read-only: reference for XYZI point layout and spiral parameterisation)
  - `04_Addons/4_5_Voxel_Mapping/CMakeLists.txt` — (new file)
  - `04_Addons/4_5_Voxel_Mapping/main.cpp` — (new file)
  - `04_Addons/4_5_Voxel_Mapping/point_cloud_publisher.cpp` — (new file)
  - `04_Addons/4_5_Voxel_Mapping/kernels/dda_cast.cl` — (new file)
  - `04_Addons/4_5_Voxel_Mapping/kernels/flip_count.cl` — (new file)

## Objective

Implement the 4.5 Voxel Mapping add-on: a ROS 2 subscriber node (`voxel_mapping`) that subscribes to a live `sensor_msgs/PointCloud2` topic, runs a per-point DDA ray-casting kernel on the GPU to mark FREE/OCCUPIED voxels via `atomic_or`, optionally applies a flip-count dynamic-object filter kernel, accumulates the grid across frames, extracts a top-down 2D slice on shutdown, and writes `output_voxel_slice.bmp`. A companion binary (`voxel_point_cloud_publisher`) generates synthetic static or dynamic point clouds for testing.

## Constraints & Rules

- Hard ROS 2 Jazzy dependency. Use `opencl_lab_ros2_guard()` from `common.cmake` — handles `$ENV{ROS_DISTRO}` check and `FATAL_ERROR` message.
- All other add-ons build without ROS 2; this one is the only exception.
- DDA kernel: one work-item per LiDAR point. Voxel occupancy grid is a `cl::Buffer` of `cl_uint`. FREE path cells use `atomic_or(cell, FREE_BIT)`; endpoint cell uses `atomic_or(cell, OCCUPIED_BIT)`.
- Flip-count kernel (`flip_count.cl`): increments a parallel `cl_uint` flip counter buffer on each occupancy state change. Post-processing zeroes occupancy for voxels where `flip_count > threshold`. Enabled by `--enable-flip-filter`. **Note:** only produces visible output with a dynamic scene (moving objects); in a fully static scene all flip counts remain 0 and the filter is a no-op.
- `cl::Event` profiling is mandatory for both kernels. Report ms to 3 decimal places.
- Sensor pose is assumed static (no odometry integration): all frames accumulate into the voxel grid in sensor frame — correct only when the sensor does not move. Document this constraint in console output.
- Per design §4.5: assert total end-to-end pipeline (upload + DDA + slice extraction) < 5 ms at 100k points. Print a `[WARN]` line if exceeded; do not terminate.
- Use `get_global_id(0)` as `size_t`; guard with `if (gid < (size_t)num_points)`.
- No hardcoded paths. All paths and topics via CLI args.
- `set(CMAKE_CXX_EXTENSIONS OFF)` must be set in `CMakeLists.txt`.

---

## Implementation

### 1. CMakeLists.txt

- Include `../../common/common.cmake` (`find_package(OpenCL REQUIRED)`, CLI11, stb already handled).
- Call `opencl_lab_ros2_guard()`.
- `find_package(rclcpp REQUIRED)`, `find_package(sensor_msgs REQUIRED)`.
- Two targets:
  - `voxel_mapping` (main.cpp): `opencl_lab_ros2_target(voxel_mapping rclcpp sensor_msgs)` + `copy_kernels(voxel_mapping)`.
  - `voxel_point_cloud_publisher` (point_cloud_publisher.cpp): `opencl_lab_ros2_target(voxel_point_cloud_publisher rclcpp sensor_msgs)`.
- No `rosbag2_cpp` dependency.

### 2. main.cpp (`voxel_mapping` binary)

- CLI11 args: `--topic` (string, default `/points`), `--resolution` (float, default `0.1`), `--output` (string, default `output_voxel_slice.bmp`), `--enable-flip-filter` (flag), `--flip-threshold` (uint, default `5`).
- `create_context()` from `common/ocl_wrapper.hpp`.
- Build kernels: `dda_cast.cl` (always), `flip_count.cl` (only when `--enable-flip-filter`).
- Initialise voxel grid `cl::Buffer` (zeroed `cl_uint` array, dimensions derived from `--resolution` and a fixed world extent e.g. 20 m × 20 m × 5 m).
- `rclcpp::Node` subscriber on `--topic`. Per `sensor_msgs/PointCloud2` callback:
  - On first message: print grid metadata and `[INFO] Sensor pose assumed static. Odometry integration not implemented.`
  - Parse XYZ float32 from raw byte buffer (read `point_step` from message header).
  - Upload points to `cl::Buffer` with `cl::Event` timing.
  - Dispatch `dda_cast` kernel (1 work-item per point) with `cl::Event`.
  - If `--enable-flip-filter`: dispatch `flip_count` kernel with `cl::Event`; host-side post-processing pass to zero occupancy where `flip_count > threshold`.
  - Print per-frame timing table: `[GPU] Upload`, `[GPU] DDA cast`, `[GPU] Flip filter` (if enabled), `Total pipeline`.
  - If total > 5 ms: print `[WARN] Pipeline exceeded 5 ms gate: X.XXX ms`.
- On SIGINT / node shutdown: extract top-down 2D slice (Z = grid_dims.z / 2), read occupancy buffer, colorize (OCCUPIED=black, FREE=white, UNKNOWN=grey), write BMP via `common/image_utils.hpp`.

### 3. point_cloud_publisher.cpp (`voxel_point_cloud_publisher` binary)

- CLI11 args: `--topic` (string, default `/points`), `--hz` (float, default `10.0`), `--points` (uint, default `10000`), `--frames` (uint, default `0` = infinite), `--scene` (string, default `static`), `--move-speed` (float, default `0.05` rad/frame).
- `rclcpp::Node` with wall timer.
- **`static` scene**: fixed sphere clusters at `(2,0,1)`, `(-2,0,1)`, `(0,3,1)` r=0.3 m intensity 150; ground band z∈[-0.1, 0.05] intensity 200; noise blob at `(0,0,2)` intensity 10. Built once in constructor.
- **`dynamic` scene**: same layout but cluster centres orbit `(0,0,1)` at r=1 m; angle += `move_speed` × `frame_index_` each callback. Ground and noise remain static. Rebuilt each callback.
- Point layout: XYZI, point_step=16, frame_id `lidar_link`. Spiral parameterisation (no RNG, deterministic).
- When `--frames > 0`: exit after N published frames.

### 4. kernels/dda_cast.cl

- Signature: `__kernel void dda_cast(__global const float* points, __global uint* grid, int4 grid_dims, float resolution, float3 origin)`
- One work-item per point. Guard bounds.
- Read endpoint `(x, y, z)` from `points[gid*3 .. gid*3+2]`.
- Convert to voxel index. Clamp to grid bounds.
- DDA traverse from `origin_voxel` to `endpoint_voxel`: `atomic_or(&grid[idx], FREE_BIT)` per intermediate voxel.
- At endpoint: `atomic_or(&grid[idx], OCCUPIED_BIT)`.
- `#define FREE_BIT 0x1u`, `#define OCCUPIED_BIT 0x2u`.

### 5. kernels/flip_count.cl

- Signature: `__kernel void count_flips(__global const uint* prev_grid, __global const uint* curr_grid, __global uint* flip_counts, int total_voxels)`
- One work-item per voxel. Guard bounds.
- If `(prev_grid[gid] & OCCUPIED_BIT) != (curr_grid[gid] & OCCUPIED_BIT)`: `atomic_add(&flip_counts[gid], 1u)`.

---

## Definition of Done (DoD)

Standard items from `.claude/rules/00_master_specs.md §8` apply.

- [ ] `cmake -B build && cmake --build build` (with ROS 2 sourced) succeeds with zero errors and zero warnings.
- [ ] `./build/voxel_mapping --help` prints CLI11-generated usage including all defined flags.
- [ ] `./build/voxel_point_cloud_publisher --help` prints CLI11-generated usage including all defined flags.
- [ ] `GPU=<vendor> ./build/voxel_mapping` selects the correct device without crashing.
- [ ] Running without `$ROS_DISTRO` set at CMake configure time emits `FATAL_ERROR` with setup instructions.
- [ ] Console prints the voxel grid dimensions line and the sensor-pose disclaimer on first message.
- [ ] Console prints per-frame timing table with `[GPU] Upload`, `[GPU] DDA cast`, `Total pipeline` columns in ms to 3 decimal places.
- [ ] `[WARN]` line is printed when total pipeline exceeds 5 ms (verified by code inspection confirming the threshold check exists, or by running with a large grid).
- [ ] `--enable-flip-filter` flag causes `flip_count.cl` to be built and dispatched; console includes `[GPU] Flip filter` timing row.
- [ ] MANUAL (sanity — static scene): Run `voxel_mapping --topic /points` alongside `voxel_point_cloud_publisher --scene static --frames 5`; Ctrl-C voxel_mapping; confirm `output_voxel_slice.bmp` shows black occupied cells, white free cells, grey unknown cells.
- [ ] MANUAL (flip filter — dynamic scene): Re-run `voxel_mapping --enable-flip-filter` alongside `voxel_point_cloud_publisher --scene dynamic --frames 30`; confirm dynamic-object voxels are absent in BMP compared to no-filter run.
- [ ] MANUAL: Confirm total pipeline ms < 5 ms at `--resolution 0.1` on target hardware (hardware waiver applies on CPU-only or low-end iGPU — record actual value).

---

## Execution Report

<!-- Filled by @coder after implementation. -->

- **Status:** PENDING
- **Session:** [YYYY-MM-DD]

### Validation

```
[output here]
```

### Changed Files

| File | Change |
|------|--------|
| `04_Addons/4_5_Voxel_Mapping/CMakeLists.txt` | Created |
| `04_Addons/4_5_Voxel_Mapping/main.cpp` | Created |
| `04_Addons/4_5_Voxel_Mapping/point_cloud_publisher.cpp` | Created |
| `04_Addons/4_5_Voxel_Mapping/kernels/dda_cast.cl` | Created |
| `04_Addons/4_5_Voxel_Mapping/kernels/flip_count.cl` | Created |

### Remaining

- [ ] All DoD items above
