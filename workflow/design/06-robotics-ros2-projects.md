# Module 6: Path C — Robotics & ROS 2

**Version:** 1.0
**Status:** Active — implementation not started
**Module Path:** `02_Projects/C_Robotics_ROS2/`

---

## Goal

Accelerate a real ROS 2 perception pipeline without breaking the node contract. The core engineering problem across every step is latency: naive ROS 2 subscriber callbacks serialize data twice before the GPU sees a single byte, and context re-initialization inside callbacks wastes 2+ ms per message. Each step teaches a concrete technique to eliminate that overhead, culminating in a Lidar perception node with < 5 ms end-to-end latency for 100k points at 200 Hz.

## Non-goals

- OpenCV / camera capture interop (covered in Path A)
- Ray tracing or graphics rendering (covered in Path B)
- Model training or AI inference (covered in Path A)
- ROS 2 navigation stack internals (Nav2 planner/controller logic)
- Hardware-specific Lidar SDK integration (generic `sensor_msgs/PointCloud2` only)
- Audio processing
- NVDEC / VAAPI hardware decoding
- SVM fine-grained coherency theory (covered in Toolbox: SVM)

---

## Roadmap / Status

- [ ] Phase 1: C1 — Node Acceleration — Wire OpenCL context into a `rclcpp::Node` lifecycle; prove context init cost is paid once, not per callback.
  - *Context*: Executive Summary §Path C item C.1; `RoboticsROS2.md` §C1_Node_Acceleration.
- [ ] Phase 2: C1 Challenge — Lifecycle Node — Context creation in `on_activate()` / destruction in `on_deactivate()` via `rclcpp_lifecycle::LifecycleNode`.
- [ ] Phase 3: C2 — Costmap Inflation — 2D distance transform kernel for obstacle padding; GPU vs CPU timing comparison; visual `output_costmap.bmp` artifact.
  - *Context*: Executive Summary §Path C item C.2; `RoboticsROS2.md` §C2_Costmap_Inflation.
- [ ] Phase 4: C2 Challenge — LDS Tiled Kernel — Profile naive (global memory) vs tiled (local memory) inflation kernel; find peak tile size.
- [ ] Phase 5: C3 — Accelerated Perception Node (Flagship) — Full pipeline: PointCloud2 subscribe → GPU filter → feature extraction → publish; end-to-end < 5 ms gate.
  - *Context*: Executive Summary §Path C item C.3; `RoboticsROS2.md` §C3_Perception_Node.
- [ ] Phase 6: C3 Challenge — Double-Buffer Real-Time Guarantee — Non-blocking enqueue; buffer swap on event callback to prevent callback stalls under load.
- [ ] Phase 7: Module review and cleanup — extract common utils, align naming conventions, verify standalone build without ROS 2 for costmap demo.

---

## Specifications

> **Inherits**: `.claude/rules/00_master_specs.md`

**Additional constraints for this path:**

- **ROS 2 Version**: Humble or later (Jazzy recommended). Detected via `find_package(rclcpp REQUIRED)`.
- **ROS 2 Sourcing**: `source /opt/ros/humble/setup.bash` must precede every CMake configure step. The CMakeLists.txt must not attempt to locate ROS 2 if `$ENV{ROS_DISTRO}` is unset — emit a fatal error with a human-readable message.
- **ROS 2 Messages**: `sensor_msgs`, `nav_msgs`, `std_msgs` from the installed ROS 2 distribution only. No custom `.msg` files in this module.
- **Loaned Messages**: Requires `rmw_fastrtps_cpp` or Iceoryx middleware. CMake must detect `$ENV{RMW_IMPLEMENTATION}` and emit a warning (not error) if it is not set to a loaned-message-capable RMW.
- **C1 Standalone Mode**: `C1_Node_Acceleration` must compile and run without a live ROS 2 master — it simulates 10 callbacks internally without requiring `ros2 run`. This makes it testable in CI without a full ROS 2 installation.
- **C2 Standalone Mode**: `C2_Costmap_Inflation` must build and produce `output_costmap.bmp` without any ROS 2 dependency — it reads a `.pgm` map file via `stb_image`. ROS 2 dependency is optional (used only if `find_package(rclcpp)` succeeds).
- **C3 ROS 2 Required**: `C3_Perception_Node` has a hard ROS 2 dependency. CMake must fail with a clear error if `rclcpp` is not found.
- **CLI**: All binaries use CLI11. C2 exposes `--map`, `--radius`, `--resolution`. C3 exposes `--topic`, `--ground-z`, `--min-intensity`. GPU selection is always via the `GPU` env var — no `--device` index flag.
- **Image Formats for C2**: `.pgm` (grayscale occupancy grid) as input; `output_costmap.bmp` as output (BMP, RGBA, obstacles black, inflated zone red gradient, free space white).
- **Point Cloud Layout**: `sensor_msgs/PointCloud2` with `point_step` always read from the message header — never hard-coded. XYZ + intensity (float32) = 16 bytes/point.
- **Timing Reporting**: GPU stages via `cl::Event` (mandatory). CPU-bound stages (deserialization, publish) via `std::chrono::steady_clock`. All times in ms to 3 decimal places.

---

## Architecture (high-level)

### Components

- **OpenCL Node Base** (`C1_Node_Acceleration`): A `rclcpp::Node` subclass (`AccelNode`) that initializes `cl::Context`, `cl::CommandQueue` (with `CL_QUEUE_PROFILING_ENABLE`), and a passthrough kernel in the constructor. Simulates 10 subscriber callbacks internally, printing per-callback kernel dispatch time and proving context init is a one-time cost.
- **Lifecycle Node Extension** (C1 Challenge): `rclcpp_lifecycle::LifecycleNode` subclass. Context created in `on_activate()`, destroyed in `on_deactivate()`. Demonstrates RAII alignment between ROS 2 node state machine and OpenCL resource lifetime.
- **Costmap Inflation Kernel** (`C2_Costmap_Inflation`): 2D kernel — each work item owns one map cell, computes distance to nearest obstacle within `inflation_radius`, and assigns a cost proportional to `exp(-decay * dist)`. Two variants: naive (global memory) and tiled (local memory). CPU reference implementation for correctness and timing comparison.
- **Perception Node** (`C3_Perception_Node`): Full ROS 2 node. Subscribes to `/points` (`sensor_msgs/PointCloud2`). Per-message GPU pipeline: upload → filter → compact → feature extract → download → publish `/filtered_points` + `/cluster_features`. Optional loaned message path for zero-copy upload from middleware.
- **Synthetic Publisher** (`C3_Perception_Node/point_cloud_publisher`): Standalone binary that generates and publishes a synthetic 100k-point cloud at configurable Hz. Used for reproducible benchmarking without a physical sensor or bag file.

### Data Flow

#### C1 (Node Acceleration)
1. Node constructor: `create_context()` (from `common/ocl_wrapper.hpp`) → `cl::CommandQueue` → build/load passthrough kernel. Log init time.
2. Simulated callback loop (10 iterations):
   - Enqueue passthrough kernel (no-op or trivial image kernel) → record `cl::Event`.
   - Log dispatch time per callback.
3. Node destructor: RAII releases `cl::Kernel`, `cl::CommandQueue`, `cl::Context`.

#### C2 (Costmap Inflation)
1. Load `.pgm` map → flat `cl::Buffer` (device, uchar, `CL_MEM_READ_ONLY`).
2. Allocate output `cl::Buffer` (uchar, same dimensions).
3. CPU path: run reference distance transform → record `std::chrono` time.
4. GPU path (naive): dispatch `inflate` kernel (global: width × height) → record `cl::Event` time.
5. GPU path (tiled, C2 Challenge): dispatch `inflate_tiled` kernel with `__local` tile → record `cl::Event` time.
6. Read back GPU result → compare against CPU reference (pixel-exact within rounding tolerance).
7. Colorize result buffer (RGBA: obstacles black, inflated gradient red, free white) → save `output_costmap.bmp`.
8. Print comparison table: CPU ms, GPU naive ms, GPU tiled ms, speedups.

#### C3 (Perception Node — per message)
1. Subscriber callback receives `PointCloud2::SharedPtr` (standard) or `unique_ptr` (loaned).
2. CPU: record deserialization time via `steady_clock`. Compute buffer size from `point_step`.
3. GPU Upload: `queue_.enqueueWriteBuffer(point_buf_, CL_FALSE, ..., &upload_event_)`.
4. GPU Filter: `filter_kernel_` (ground removal + intensity threshold, single pass) → `cl::Event`.
5. GPU Compact: prefix-sum compaction kernel on filter mask → compacted point buffer → `cl::Event`.
6. GPU Feature Extract: per-cluster feature extraction kernel → `cl::Event`.
7. GPU Download: `enqueueReadBuffer` (compacted points + features) → `cl::Event`.
8. CPU Publish: construct and publish `filtered_points` + `cluster_features` → `steady_clock`.
9. Log per-stage breakdown and running total. Assert total < 5 ms.

---

## Key Decisions (and Rationale)

1. **OpenCL Context as Node Member, Not Callback-Local**
   - **Why**: `cl::Context` construction costs 1–3 ms (platform query, device selection, driver initialization). In a 200 Hz callback (5 ms budget), this alone exceeds the entire latency gate. C1 exists solely to measure and internalize this cost.

2. **C2 Standalone (No ROS 2 Required) for Costmap Demo**
   - **Why**: The distance transform kernel has no dependency on ROS 2 message types. Making C2 standalone maximizes the audience (embedded engineers without a ROS 2 install) and makes CI trivial. The ROS 2 integration is a optional extension, not the core lesson.

3. **C2 CPU Reference Implementation Mandatory**
   - **Why**: The costmap is safety-critical in AMR navigation. Correctness must be verified pixel-by-pixel against a CPU reference before trusting GPU output. The CPU run also provides the speedup denominator for the performance gate.

4. **Single-Pass Filter Kernel (Ground Removal + Intensity in One Dispatch)**
   - **Why**: Two separate kernel dispatches would require two `enqueueNDRange` calls, two kernel launch overheads (~0.1–0.3 ms each), and an intermediate buffer. A combined predicate (`z >= ground_z && intensity >= min_intensity`) in one pass avoids both at the cost of a slightly more complex kernel — a clear win given the 5 ms gate.

5. **Compaction via Prefix-Sum (Not Stream Compaction to Host)**
   - **Why**: Transferring the raw filter mask to the CPU for compaction would add 0.8–1.5 ms per message. GPU prefix-sum compaction keeps the data on device until the final download. The compacted result is smaller than the original cloud, reducing download time.

6. **Loaned Messages as Optional Path, Not Default**
   - **Why**: Loaned messages require a specific RMW (fastrtps or Iceoryx) and may not be available in all environments. The standard path must work as a standalone baseline. The loaned path is the optimization target, enabled by a compile-time flag or runtime detection.

7. **Synthetic Publisher as Part of C3**
   - **Why**: Reproducible benchmarking requires a deterministic, parametric source. Bag files are large and environment-specific. A 100-line generator binary produces any desired cloud size at any Hz with zero external dependencies, making the performance gate verifiable on any machine.

8. **AoS Input, SoA as Mini-Challenge (Not Default)**
   - **Why**: `sensor_msgs/PointCloud2` natively stores points as AoS (XYZIXYZIXYZ…). The base implementation must mirror the real-world input format. The SoA conversion and its coalescing benefit is the mini-challenge, teaching the coalescing trade-off explicitly rather than silently pre-optimizing.

---

## Known Issues / Risks

- **`find_package(rclcpp)` Fails Without Sourced Workspace**: CMake silently reports "not found" if `setup.bash` was not sourced. The CMakeLists.txt must check `$ENV{ROS_DISTRO}` and emit `message(FATAL_ERROR "...")` with instructions before attempting `find_package`.
- **OpenCL Context on ROS 2 Executor Thread**: The ROS 2 executor thread may not share GPU context affinity with the main thread on some drivers. `create_context()` must be called from the same thread as the subscriber callbacks — the node constructor (which runs on the calling thread before the executor spins) is the correct location.
- **Loaned Messages Middleware Dependency**: `rmw_cyclonedds_cpp` does not support loaned messages. Users who have `RMW_IMPLEMENTATION=rmw_cyclonedds_cpp` will silently fall back to copy-based transport, and the 5 ms gate may not be achievable. The node must log a warning when the loaned path is unavailable.
- **`PointCloud2` `point_step` Variability**: Different Lidar drivers use different point formats (XYZ = 12 bytes, XYZI = 16 bytes, XYZRGB = 24 bytes). The filter kernel must be parameterized by `point_step` at dispatch time — hard-coding 16 bytes breaks with non-XYZI clouds.
- **`ros2 topic hz` Measurement Accuracy**: `ros2 topic hz` computes rate over a sliding window; it underreports rate if messages arrive in bursts. Use `ros2 topic bw` and per-message timestamps for accurate latency measurement.
- **Costmap BMP Color Mapping**: The RGBA colorization (obstacle black, inflated red, free white) must be done on the CPU after readback — not in the kernel — to keep the kernel output a simple uchar cost buffer reusable in other pipelines.
- **Prefix-Sum Kernel Complexity**: A correct parallel prefix-sum (scan) implementation is non-trivial. The initial implementation may use a simple two-phase Blelloch scan; correctness must be verified against a CPU scan on the same data before integrating into the pipeline.

---

## Performance Gates (Path Completion)

| Project | Metric | Target |
| :--- | :--- | :--- |
| C1 Node Acceleration | Per-callback kernel dispatch time | ≤ 0.5 ms (flat across 10 callbacks) |
| C1 Node Acceleration | Context init overhead | Logged once; must not appear in callback log |
| C2 Costmap Inflation (naive) | GPU distance transform | < 10 ms @ 512×512 map (≤ 100 ms budget for 10 Hz update) |
| C2 Costmap Inflation (tiled) | GPU distance transform | < 5 ms @ 512×512 map |
| C2 Costmap vs CPU | Speedup | ≥ 5× reported in console |
| C3 Perception Node | End-to-end latency | < 5 ms @ 100k points (all stages summed) |
| C3 Perception Node | Publish rate | ≥ 200 Hz sustained |

GPU stages timed via `cl::Event` profiling in milliseconds to 3 decimal places. CPU stages timed via `std::chrono::steady_clock`. Wall-clock estimates do not satisfy the gate.

---

## Specifications & Standards

- **Directory Structure**:
  ```
  02_Projects/C_Robotics_ROS2/
  ├── RoboticsROS2.md                   (user-facing README, existing)
  ├── C1_Node_Acceleration/
  │   ├── CMakeLists.txt
  │   ├── main.cpp
  │   └── kernels/passthrough.cl
  ├── C2_Costmap_Inflation/
  │   ├── CMakeLists.txt
  │   ├── main.cpp
  │   └── kernels/inflate.cl
  │   └── kernels/inflate_tiled.cl
  └── C3_Perception_Node/
      ├── CMakeLists.txt
      ├── main.cpp                      (perception node)
      ├── point_cloud_publisher.cpp     (synthetic publisher binary)
      └── kernels/filter.cl
      └── kernels/prefix_sum.cl
      └── kernels/feature_extract.cl
  ```
- **Verification Standard**:
  - C1: Console log showing context init time (once) and per-callback dispatch times (flat, ≤ 0.5 ms each). No BMP required.
  - C2: `output_costmap.bmp` (correct colorization — obstacles black, inflation red gradient, free white). Console table with CPU / GPU naive / GPU tiled times and speedups.
  - C3: Console per-message stage breakdown summing to < 5 ms. `ros2 topic hz /filtered_points` shows ≥ 200 Hz. No BMP required (point cloud topic is the visual artifact).
- **Tooling**:
  - `cl.hpp` (C++ bindings, OpenCL 1.2 baseline).
  - `stb_image` / `stb_image_write` for `.pgm` input and `output_costmap.bmp` output (C2 only).
  - `CLI11` via `common/common.cmake` for all argument parsing.
  - `find_package(OpenCL REQUIRED)` in every `CMakeLists.txt`.
  - `common/ocl_wrapper.hpp` → `create_context()` for GPU selection.
  - `CL_CHECK(err)` macro from `common/opencl_utils.hpp` for all error handling.
  - ROS 2 packages: `rclcpp`, `sensor_msgs`, `nav_msgs` via `find_package(... REQUIRED)`.
  - No third-party Lidar SDK. No PCL dependency.
- **OpenCL 2.0+ Gating**: Any SVM usage must be wrapped in `#ifdef CL_VERSION_2_0`. No OpenCL 2.0 features are required by this path.

---

## Prerequisites

- Module 1 completed (`01_Host_API/`): `cl.hpp` usage, `cl::Event` profiling, `CL_CHECK` error handling.
- ROS 2 Humble or later installed: `sudo apt install ros-humble-desktop`.
  - Required packages: `ros-humble-rclcpp`, `ros-humble-sensor-msgs`, `ros-humble-nav-msgs`.
  - Workspace sourced before CMake: `source /opt/ros/humble/setup.bash`.
  - C2 standalone demo has no ROS 2 requirement.
- `assets/warehouse.pgm` (512×512 or larger occupancy grid) for C2.
- `assets/lidar_sample.bag` (optional, for C3 realistic replay). Synthetic publisher covers the no-bag case.
- RMW set to a loaned-message-capable implementation for optimal C3 performance:
  `export RMW_IMPLEMENTATION=rmw_fastrtps_cpp`

See [main README](../../README.md) for base requirements (OpenCL 1.2+, CMake 3.18+, Docker setup).
