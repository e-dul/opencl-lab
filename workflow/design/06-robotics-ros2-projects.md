# Module 6: Path C — Robotics & ROS 2

**Version:** 1.1
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
  - *Context*: Executive Summary §Path C item C.1 challenge; `RoboticsROS2.md` §C1_Lifecycle_Challenge.
- [ ] Phase 3: C2 — Costmap Inflation — 2D distance transform kernel for obstacle padding; GPU vs CPU timing comparison; visual `output_costmap.bmp` artifact.
  - *Context*: Executive Summary §Path C item C.2; `RoboticsROS2.md` §C2_Costmap_Inflation.
- [ ] Phase 4: C2 Challenge — LDS Tiled Kernel — Profile naive (global memory) vs tiled (local memory) inflation kernel; find peak tile size.
  - *Context*: Executive Summary §Path C item C.2 challenge; `RoboticsROS2.md` §C2_Tiled_Challenge.
- [ ] Phase 5: C3 — Accelerated Perception Node (Flagship) — Full pipeline: PointCloud2 subscribe → GPU filter → feature extraction → publish; end-to-end < 5 ms gate.
  - *Context*: Executive Summary §Path C item C.3; `RoboticsROS2.md` §C3_Perception_Node.
- [ ] Phase 6: C3 Challenge — Double-Buffer Real-Time Guarantee — Non-blocking enqueue; buffer swap on event callback to prevent callback stalls under load.
  - *Context*: Executive Summary §Path C item C.3 challenge; `RoboticsROS2.md` §C3_DoubleBuffer_Challenge.
- [ ] Phase 7: Module review and cleanup — extract common utils, align naming conventions, verify standalone build without ROS 2 for costmap demo.

---

## Specifications

> **Inherits**: `.claude/rules/00_master_specs.md`

**Additional constraints for this path:**

- **ROS 2 Version**: Jazzy (Ubuntu 24.04+). Detected via `find_package(rclcpp REQUIRED)`.
- **ROS 2 Sourcing**: `source /opt/ros/jazzy/setup.bash` must precede every CMake configure step. The CMakeLists.txt must not attempt to locate ROS 2 if `$ENV{ROS_DISTRO}` is unset — emit a fatal error with a human-readable message.
- **ROS 2 Messages**: `sensor_msgs`, `nav_msgs`, `std_msgs` from the installed ROS 2 distribution only. No custom `.msg` files in this module.
- **Loaned Messages**: Requires `rmw_fastrtps_cpp` or Iceoryx middleware. CMake must detect `$ENV{RMW_IMPLEMENTATION}` and emit a warning (not error) if it is not set to a loaned-message-capable RMW. At runtime, if the loaned path is unavailable, the node must print a descriptive message and fall back to copy-based transport — silent fallback or crash is forbidden.
- **C1 Standalone Mode**: `C1_Node_Acceleration` must compile and run without a ROS 2 daemon or active DDS network — it simulates callbacks internally without requiring `ros2 run`. This makes it testable in CI without a full ROS 2 installation.
- **C2 Standalone Mode**: `C2_Costmap_Inflation` must build and produce `output_costmap.bmp` without any ROS 2 dependency — it reads a `.pgm` map file via `stb_image`. ROS 2 dependency is optional (used only if `find_package(rclcpp)` succeeds).
- **C3 ROS 2 Required**: `C3_Perception_Node` has a hard ROS 2 dependency. CMake must fail with a clear error if `rclcpp` is not found.
- **CLI**: All binaries use CLI11 (master spec §1). Per-binary flags:
  - C1: `--iterations` (default 10, number of simulated callbacks).
  - C2: `--map`, `--radius`, `--resolution`.
  - C3 perception node: `--topic`, `--ground-z`, `--min-intensity`.
  - C3 synthetic publisher: `--topic`, `--hz`, `--points` (cloud size).
  - GPU selection always via `GPU` env var — no `--device` index flag.
- **Image Formats for C2**: `.pgm` (grayscale occupancy grid) as input; `output_costmap.bmp` as output (BMP, RGBA, obstacles black, inflated zone red gradient, free space white). C2 is a numeric/visual benchmark — BMP is the required visual artifact (master spec §3).
- **C3 Visual Artifact**: C3 is a pure pipeline benchmark. No BMP output is required — the structured per-stage console timing table and the published `/filtered_points` topic are the verification artifacts (master spec §3 exception: "purely numeric tools must produce a structured console timing table").
- **Point Cloud Layout**: `sensor_msgs/PointCloud2` with `point_step` always read from the message header — never hard-coded. XYZ + intensity (float32) = 16 bytes/point is the default test layout.
- **Timing Reporting**: GPU stages via `cl::Event` with `CL_QUEUE_PROFILING_ENABLE` (mandatory on all command queues in C1, C2, C3). CPU-bound stages (deserialization, publish) via `std::chrono::steady_clock`. All times in ms to 3 decimal places.
- **CL_CHECK Coverage**: `cl::Kernel::setArg()`, `cl::CommandQueue::finish()`, `enqueueNDRangeKernel()`, `enqueueReadBuffer()`, `enqueueWriteBuffer()` do not throw — every call must be wrapped in `CL_CHECK` (master spec §7.2).
- **CMake Strictness**: Every `CMakeLists.txt` must set `set(CMAKE_CXX_EXTENSIONS OFF)` alongside `CMAKE_CXX_STANDARD 17` (master spec §7.3).
- **CPU Reference DT Algorithm (C2)**: The CPU reference distance transform uses a exact Euclidean distance transform (per-cell brute-force over obstacle pixels within `inflation_radius`). This is the correctness oracle; GPU output must match within ±1 cost unit (0–255 scale) per cell.
- **`/cluster_features` Message Type**: Published as `sensor_msgs/PointCloud2` (fields: centroid XYZ + intensity mean + point count as float32). No custom message types.

---

## Architecture (high-level)

### Components

- **OpenCL Node Base** (`C1_Node_Acceleration`): A `rclcpp::Node` subclass (`AccelNode`) that initializes `cl::Context`, `cl::CommandQueue` (with `CL_QUEUE_PROFILING_ENABLE`), and a passthrough kernel in the constructor. Passthrough kernel copies a fixed-size float buffer (1M elements) to prove dispatch overhead without driver no-op elision. Simulates `--iterations` subscriber callbacks internally, printing per-callback kernel dispatch time and proving context init is a one-time cost.
- **Lifecycle Node Extension** (C1 Challenge): `rclcpp_lifecycle::LifecycleNode` subclass. Context created in `on_activate()`, destroyed in `on_deactivate()`. Demonstrates RAII alignment between ROS 2 node state machine and OpenCL resource lifetime.
- **Costmap Inflation Kernel** (`C2_Costmap_Inflation`): 2D kernel — each work item owns one map cell, computes Euclidean distance to nearest obstacle within `inflation_radius`, and assigns a cost proportional to `exp(-decay * dist)`. Two variants: naive (global memory) and tiled (local memory, C2 Challenge). CPU reference implementation for correctness and timing comparison. Both queues use `CL_QUEUE_PROFILING_ENABLE`.
- **Perception Node** (`C3_Perception_Node`, binary: `perception_node`): Full ROS 2 node. Subscribes to `/points` (`sensor_msgs/PointCloud2`). Per-message GPU pipeline: upload → filter → compact → feature extract → download → publish `/filtered_points` + `/cluster_features`. Optional loaned message path for zero-copy upload from middleware. Queue uses `CL_QUEUE_PROFILING_ENABLE`.
- **Double-Buffer Extension** (C3 Challenge): Adds a second `cl::Buffer` pair to `PerceptionNode`. While GPU processes buffer N, CPU fills buffer N+1. Buffer swap triggered by `cl::Event` callback. Prevents callback stalls when processing exceeds inter-message interval under load.
- **Synthetic Publisher** (`C3_Perception_Node/point_cloud_publisher.cpp`, binary: `point_cloud_publisher`): Standalone ROS 2 node binary. Generates and publishes synthetic `sensor_msgs/PointCloud2` at configurable rate and size. CLI: `--topic`, `--hz`, `--points`. Used for reproducible benchmarking without a physical sensor or bag file. Separate `add_executable` target in `C3_Perception_Node/CMakeLists.txt`.

### Data Flow

#### C1 (Node Acceleration)
1. Node constructor: `create_context()` (from `common/ocl_wrapper.hpp`) → `cl::CommandQueue` (with `CL_QUEUE_PROFILING_ENABLE`) → build/load passthrough kernel (1M-element float copy). Log init time.
2. Simulated callback loop (`--iterations`, default 10):
   - Enqueue passthrough kernel → record `cl::Event`.
   - Log dispatch time per callback.
3. Node destructor: RAII releases `cl::Kernel`, `cl::CommandQueue`, `cl::Context`.

#### C2 (Costmap Inflation)
1. Load `.pgm` map → flat `cl::Buffer` (device, uchar, `CL_MEM_READ_ONLY`).
2. Allocate output `cl::Buffer` (uchar, same dimensions).
3. CPU path: run exact Euclidean DT reference → record `std::chrono` time.
4. GPU path (naive): dispatch `inflate` kernel (global: width × height) → record `cl::Event` time.
5. GPU path (tiled, C2 Challenge): dispatch `inflate_tiled` kernel with `__local` tile → record `cl::Event` time.
6. Read back GPU result → compare against CPU reference (max deviation ±1 cost unit per cell).
7. Colorize result buffer on CPU (RGBA: obstacles black, inflated gradient red, free white) → save `output_costmap.bmp`.
8. Print comparison table: CPU ms, GPU naive ms, GPU tiled ms, GPU-vs-CPU speedup, tiled-vs-naive speedup.

#### C3 (Perception Node — per message)
1. Subscriber callback receives `PointCloud2::SharedPtr` (standard) or `unique_ptr` (loaned).
2. CPU: record deserialization time via `steady_clock`. Compute buffer size from `point_step`.
3. GPU Upload: `CL_CHECK(queue_.enqueueWriteBuffer(point_buf_, CL_FALSE, ..., &upload_event_))`.
4. GPU Filter: `filter_kernel_` (ground removal + intensity threshold, single pass) → `cl::Event`.
5. GPU Compact: prefix-sum compaction kernel on filter mask → compacted point buffer → `cl::Event`.
6. GPU Feature Extract: per-cluster feature extraction kernel → `cl::Event`.
7. GPU Download: `CL_CHECK(queue_.enqueueReadBuffer(...))` (compacted points + features) → `cl::Event`.
8. CPU Publish: construct and publish `filtered_points` + `cluster_features` → `steady_clock`.
9. Log per-stage breakdown and running total. Assert total < 5 ms.

#### C3 Challenge (Double-Buffer)
1. Two buffer pairs (`point_buf_[2]`, `result_buf_[2]`); active index alternates each callback.
2. Callback N enqueues GPU work on buffer index `N % 2`.
3. `cl::Event` completion callback swaps active index — no blocking wait in the ROS 2 executor thread.
4. Callback N+1 fills the other buffer while GPU processes callback N's data.

---

## Key Decisions (and Rationale)

1. **OpenCL Context as Node Member, Not Callback-Local**
   - **Why**: `cl::Context` construction costs 1–3 ms (platform query, device selection, driver initialization). In a 200 Hz callback (5 ms budget), this alone exceeds the entire latency gate. C1 exists solely to measure and internalize this cost.

2. **C2 Standalone (No ROS 2 Required) for Costmap Demo**
   - **Why**: The distance transform kernel has no dependency on ROS 2 message types. Making C2 standalone maximizes the audience (embedded engineers without a ROS 2 install) and makes CI trivial. The ROS 2 integration is an optional extension, not the core lesson.

3. **C2 CPU Reference Implementation Mandatory**
   - **Why**: The costmap is safety-critical in AMR navigation. Correctness must be verified pixel-by-pixel against a CPU reference before trusting GPU output. The CPU run also provides the speedup denominator for the performance gate.

4. **Single-Pass Filter Kernel (Ground Removal + Intensity in One Dispatch)**
   - **Why**: Two separate kernel dispatches would require two `enqueueNDRange` calls, two kernel launch overheads (~0.1–0.3 ms each), and an intermediate buffer. A combined predicate (`z >= ground_z && intensity >= min_intensity`) in one pass avoids both at the cost of a slightly more complex kernel — a clear win given the 5 ms gate.

5. **Compaction via Prefix-Sum (Not Stream Compaction to Host)**
   - **Why**: Transferring the raw filter mask to the CPU for compaction would add 0.8–1.5 ms per message. GPU prefix-sum compaction keeps the data on device until the final download. The compacted result is smaller than the original cloud, reducing download time.

6. **Loaned Messages as Optional Path, Not Default**
   - **Why**: Loaned messages require a specific RMW (fastrtps or Iceoryx) and may not be available in all environments. The standard path must work as a standalone baseline. The loaned path is the optimization target, enabled by a compile-time flag or runtime detection.

7. **Synthetic Publisher as Part of C3**
   - **Why**: Reproducible benchmarking requires a deterministic, parametric source. Bag files are large and environment-specific. A generator binary produces any desired cloud size at any Hz with zero external dependencies, making the performance gate verifiable on any machine.

8. **AoS Input, SoA as Mini-Challenge (Not Default)**
   - **Why**: `sensor_msgs/PointCloud2` natively stores points as AoS (XYZIXYZIXYZ…). The base implementation must mirror the real-world input format. The SoA conversion and its coalescing benefit is the mini-challenge, teaching the coalescing trade-off explicitly rather than silently pre-optimizing.

9. **Passthrough Kernel Copies 1M Floats (Not a No-Op)**
   - **Why**: A truly empty kernel body may be optimized away by drivers, producing artificially low dispatch times that misrepresent real workload cost. A 1M-element float copy is minimal but unambiguously real work that drivers cannot elide.

---

## Known Issues / Risks

- **`find_package(rclcpp)` Fails Without Sourced Workspace**: CMake silently reports "not found" if `setup.bash` was not sourced. The CMakeLists.txt must check `$ENV{ROS_DISTRO}` and emit `message(FATAL_ERROR "...")` with instructions before attempting `find_package`.
- **OpenCL Context on ROS 2 Executor Thread**: The ROS 2 executor thread may not share GPU context affinity with the main thread on some drivers. `create_context()` must be called from the same thread as the subscriber callbacks — the node constructor (which runs on the calling thread before the executor spins) is the correct location.
- **Loaned Messages Middleware Dependency**: `rmw_cyclonedds_cpp` does not support loaned messages. Users with `RMW_IMPLEMENTATION=rmw_cyclonedds_cpp` will fall back to copy-based transport; the node must print a warning and continue — the 5 ms gate may not be achievable on this path.
- **`PointCloud2` `point_step` Variability**: Different Lidar drivers use different point formats (XYZ = 12 bytes, XYZI = 16 bytes, XYZRGB = 24 bytes). The filter kernel must be parameterized by `point_step` at dispatch time — hard-coding 16 bytes breaks with non-XYZI clouds.
- **`ros2 topic hz` Measurement Accuracy**: `ros2 topic hz` underreports rate for bursty publishers. Authoritative latency measurement uses per-message `steady_clock` timestamps logged by the node, not `ros2 topic hz`. The 200 Hz gate is verified via the node's own per-message log.
- **Costmap BMP Color Mapping**: The RGBA colorization (obstacle black, inflated red, free white) must be done on the CPU after readback — not in the kernel — to keep the kernel output a simple uchar cost buffer reusable in other pipelines.
- **Prefix-Sum Kernel Complexity**: A correct parallel prefix-sum (scan) implementation is non-trivial. The initial implementation may use a two-phase Blelloch scan; correctness must be verified against a CPU scan on the same data before integrating into the pipeline.

---

## Performance Gates (Path Completion)

Hardware-waiver: gates marked with † may not be achievable on CPU-fallback or integrated GPU devices. If the hardware target is met for the primary GPU but not a fallback, the gate is considered passed with a note.

| Project | Metric | Target |
| :--- | :--- | :--- |
| C1 Node Acceleration | Per-callback kernel dispatch time | ≤ 0.5 ms (flat across all callbacks) † |
| C1 Node Acceleration | Context init overhead | Logged once; must not appear in per-callback log |
| C2 Costmap Inflation (naive) | GPU distance transform | < 10 ms @ 512×512 map † |
| C2 Costmap Inflation (tiled) | GPU distance transform | < 5 ms @ 512×512 map † |
| C2 Costmap tiled vs naive | Speedup | ≥ 1.5× (tiled over naive) reported in console † |
| C2 Costmap GPU vs CPU | Speedup | ≥ 5× (GPU naive over CPU) reported in console † |
| C3 Perception Node | End-to-end latency | < 5 ms @ 100k points (all stages summed) † |
| C3 Perception Node | Publish rate | ≥ 200 Hz sustained (measured via per-message node log) † |

---

## Specifications & Standards

- **Directory Structure**:
  ```
  02_Projects/C_Robotics_ROS2/
  ├── RoboticsROS2.md                   (user-facing README, existing)
  ├── C1_Node_Acceleration/
  │   ├── CMakeLists.txt
  │   ├── main.cpp
  │   └── kernels/
  │       └── passthrough.cl
  ├── C2_Costmap_Inflation/
  │   ├── CMakeLists.txt
  │   ├── main.cpp
  │   └── kernels/
  │       ├── inflate.cl
  │       └── inflate_tiled.cl
  └── C3_Perception_Node/
      ├── CMakeLists.txt
      ├── main.cpp                      (binary: perception_node)
      ├── point_cloud_publisher.cpp     (binary: point_cloud_publisher — separate add_executable target)
      └── kernels/
          ├── filter.cl
          ├── prefix_sum.cl
          └── feature_extract.cl
  ```
- **Verification Standard**:
  - C1: Console log showing context init time (once) and per-callback dispatch times (flat, ≤ 0.5 ms each). No BMP required.
  - C2: `output_costmap.bmp` (correct colorization — obstacles black, inflation red gradient, free white). Console table with CPU / GPU naive / GPU tiled times, GPU-vs-CPU speedup, tiled-vs-naive speedup.
  - C3: Console per-message stage breakdown summing to < 5 ms. Node log showing ≥ 200 Hz inter-message rate. No BMP required (structured timing table satisfies master spec §3 numeric-tool exception).
- **Tooling** (module-specific additions to master_specs):
  - `stb_image` / `stb_image_write` for `.pgm` input and `output_costmap.bmp` output (C2 only).
  - ROS 2 packages: `rclcpp`, `sensor_msgs`, `nav_msgs` via `find_package(... REQUIRED)`.
  - No third-party Lidar SDK. No PCL dependency.
- **Note**: No OpenCL 2.0 features are required by this path.

---

## Prerequisites

- Module 1 completed (`01_Host_API/`): `cl.hpp` usage, `cl::Event` profiling, `CL_CHECK` error handling.
- ROS 2 Jazzy: see [`02_Projects/C_Robotics_ROS2/SETUP.md`](../../02_Projects/C_Robotics_ROS2/SETUP.md).
  - C2 standalone demo has no ROS 2 requirement.
- `assets/warehouse.pgm` (512×512 or larger occupancy grid) for C2.
- `assets/lidar_sample.bag` (optional, for C3 realistic replay). Synthetic publisher covers the no-bag case.
- RMW set to a loaned-message-capable implementation for optimal C3 performance:
  `export RMW_IMPLEMENTATION=rmw_fastrtps_cpp`

See [main README](../../README.md) for base requirements (OpenCL 1.2+, CMake 3.18+, Docker setup).
