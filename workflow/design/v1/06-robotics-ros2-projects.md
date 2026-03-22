# Module 6: Path C — Robotics & ROS 2

**Version:** 1.5
**Status:** Complete — C1,C2,C3 complete; C3-challenge complete; Phase 6 cleanup complete
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

- [x] Phase 1: C1 — Node Acceleration — `rclcpp_lifecycle::LifecycleNode`; OpenCL init in `on_configure()`; real pub/sub `/raw_floats` → kernel dispatch → `/processed_floats`; prove context init is paid once.
  - *Context*: Executive Summary §Path C item C.1; `RoboticsROS2.md` §C1_Node_Acceleration.
- [x] Phase 2: C2 — Costmap Inflation — 2D distance transform kernel for obstacle padding; GPU vs CPU timing comparison; visual `output_costmap.bmp` artifact.
  - *Context*: Executive Summary §Path C item C.2; `RoboticsROS2.md` §C2_Costmap_Inflation.
- [x] Phase 3: C2 Challenge — LDS Tiled Kernel — Profile naive (global memory) vs tiled (local memory) inflation kernel; find peak tile size.
  - *Context*: Executive Summary §Path C item C.2 challenge; `RoboticsROS2.md` §C2_Tiled_Challenge.
- [x] Phase 4: C3 — Accelerated Perception Node (Flagship) — Full pipeline: PointCloud2 subscribe → GPU filter → feature extraction → publish; end-to-end < 5 ms gate.
  - *Context*: Executive Summary §Path C item C.3; `RoboticsROS2.md` §C3_Perception_Node.
- [x] Phase 5: C3 Challenge — Double-Buffer Real-Time Guarantee — Non-blocking enqueue; buffer swap via `cl::Event` callback; contention guard logs `WARN` and falls back to `queue_.finish()` — silent drop forbidden. See §Verification Standard for the mixed-scene setup and expected outcomes. MANUAL verification required (RViz).
  - *Context*: Executive Summary §Path C item C.3 challenge; `RoboticsROS2.md` §C3_DoubleBuffer_Challenge.
- [x] Phase 6: Module review and cleanup — extract common utils, align naming conventions, verify all three binaries build and run cleanly from a sourced ROS 2 workspace. Extract CMake related ROS setup to common.cmake.

---

## Specifications

> **Inherits**: `.claude/rules/00_master_specs.md`

**Additional constraints for this path:**

- **ROS 2 Version**: Jazzy (Ubuntu 24.04+). Detected via `find_package(rclcpp REQUIRED)`.
- **ROS 2 Sourcing**: `source /opt/ros/jazzy/setup.bash` must precede every CMake configure step. The CMakeLists.txt must not attempt to locate ROS 2 if `$ENV{ROS_DISTRO}` is unset — emit a fatal error with a human-readable message.
- **ROS 2 Messages**: `sensor_msgs`, `nav_msgs`, `std_msgs` from the installed ROS 2 distribution only. No custom `.msg` files in this module.
- **Loaned Messages**: Requires `rmw_fastrtps_cpp` or Iceoryx middleware. CMake must detect `$ENV{RMW_IMPLEMENTATION}` and emit a warning (not error) if it is not set to a loaned-message-capable RMW. At runtime, if the loaned path is unavailable, the node must print a descriptive message and fall back to copy-based transport — silent fallback or crash is forbidden.
- **C1 Standalone Mode**: `C1_Node_Acceleration` must compile and run without external ROS 2 peers — the binary spins two nodes in the same process: `AccelNode` (LifecycleNode) and `SyntheticPublisher` (timer-driven). Intra-process communication is enabled so messages never leave the process. Testable in CI without a running DDS daemon or active network.
- **C2 Standalone Mode**: `C2_Costmap_Inflation` must compile and run without external ROS 2 peers — the binary spins two nodes in the same process: `CostmapNode` (LifecycleNode) and `MapPublisher` (reads `.pgm`, publishes once to `/map`). Intra-process communication enabled. ROS 2 is required; no optional dependency.
- **C3 ROS 2 Required**: `C3_Perception_Node` has a hard ROS 2 dependency. CMake must fail with a clear error if `rclcpp` is not found.
- **Node Parameters (C1, C2, C3)**: All ROS 2 nodes use `declare_parameter` — not CLI11. CLI11 is used only for the C3 synthetic publisher (standalone tool, not a node). GPU selection always via `GPU` env var — no `--device` index flag.
  - C1 parameters: `iterations` (int, default 10), `buffer_size` (int, default 1048576).
  - C2 parameters: `map_path` (string), `inflation_radius` (double, default 0.5), `resolution` (double, default 0.05), `decay` (double, default 3.0).
  - C3 perception node parameters: `topic` (string), `ground_z` (double), `min_intensity` (double), `max_points` (int, default 200000 — buffer capacity; messages exceeding this size trigger a `WARN` and blocking realloc), `use_double_buffer` (bool, default `false` — **C3 Challenge only**: when `true`, `on_configure()` allocates a second buffer pair and the callback uses the non-blocking enqueue path).
  - C3 synthetic publisher CLI flags: `--topic`, `--hz`, `--points` (cloud size).
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

- **OpenCL Lifecycle Node** (`C1_Node_Acceleration`, binary: `accel_node`): `rclcpp_lifecycle::LifecycleNode` subclass (`AccelNode`). `on_configure()`: init `cl::Context`, `cl::CommandQueue` (with `CL_QUEUE_PROFILING_ENABLE`), build passthrough kernel, pre-allocate `cl::Buffer` (`buffer_size` floats). Log init time once. `on_activate()`: subscribe `/raw_floats` (`std_msgs/Float32MultiArray`), advertise `/processed_floats`. Per-callback: `enqueueWriteBuffer` → dispatch passthrough kernel → `enqueueReadBuffer` → publish result → log `cl::Event` dispatch time. After `iterations` callbacks, node requests deactivate. `on_deactivate()`: destroy subscription and publisher. `on_cleanup()`: RAII destroys all OpenCL resources.
- **Synthetic Publisher** (`C1_Node_Acceleration/synthetic_publisher.cpp`, same binary): `rclcpp::Node` subclass spun in the same executor as `AccelNode`. Timer fires `iterations` times, publishing a `Float32MultiArray` of `buffer_size` floats to `/raw_floats`. Intra-process comm ensures zero-copy delivery to `AccelNode`. Shuts down the process after all messages are acknowledged.
- **Costmap Node** (`C2_Costmap_Inflation`, binary: `costmap_node`): `rclcpp_lifecycle::LifecycleNode` subclass (`CostmapNode`). `on_configure()`: init `cl::Context`, `cl::CommandQueue` (CL_QUEUE_PROFILING_ENABLE), build `inflate` and `inflate_tiled` kernels, declare parameters. `on_activate()`: subscribe `/map` (`nav_msgs/OccupancyGrid`), advertise `/inflated_costmap`. Per-callback: run CPU reference DT + GPU naive + GPU tiled on the received grid; print timing table; save `output_costmap.bmp`; publish `/inflated_costmap`. `on_cleanup()`: RAII destroys OpenCL resources.
- **Map Publisher** (`C2_Costmap_Inflation/map_publisher.cpp`, same binary): `rclcpp::Node` subclass. Reads `map_path` `.pgm` via `stb_image`, converts to `nav_msgs/OccupancyGrid`, publishes once to `/map` via intra-process comm. Triggers `CostmapNode` shutdown after the single map message is processed.
- **Perception Node** (`C3_Perception_Node`, binary: `perception_node`): `rclcpp_lifecycle::LifecycleNode`. Subscribes to `/points` (`sensor_msgs/PointCloud2`). Per-message GPU pipeline: upload → filter → compact → feature extract → download → publish `/filtered_points` + `/cluster_features`. Optional loaned message path for zero-copy upload from middleware. Queue uses `CL_QUEUE_PROFILING_ENABLE`.
- **Double-Buffer Extension** (C3 Challenge): Extends `PerceptionNode` via the `use_double_buffer` parameter (bool, default `false`). When enabled, `on_configure()` allocates a second `cl::Buffer` pair and the callback switches to the non-blocking enqueue path. Buffer swap triggered by `cl::Event` callback. Prevents callback stalls when processing exceeds inter-message interval under load. No separate binary or folder — same `perception_node` binary, same `C3_Perception_Node/` directory.
- **Synthetic Publisher** (`C3_Perception_Node/point_cloud_publisher.cpp`, binary: `point_cloud_publisher`): Standalone ROS 2 node binary. Generates and publishes synthetic `sensor_msgs/PointCloud2` at configurable rate and size. CLI: `--topic`, `--hz`, `--points`. Used for reproducible benchmarking without a physical sensor or bag file. Separate `add_executable` target in `C3_Perception_Node/CMakeLists.txt`.

### Data Flow

#### C1 (Node Acceleration)
1. `main()`: `rclcpp::init` → create `AccelNode` + `SyntheticPublisher` in a single-threaded executor with intra-process comm enabled.
2. Lifecycle manager (in `main()`) calls `configure` → `activate` transitions on `AccelNode`.
3. `AccelNode::on_configure()`: `create_context()` → `cl::CommandQueue` (CL_QUEUE_PROFILING_ENABLE) → build passthrough kernel → pre-allocate `cl::Buffer(buffer_size)`. Log init time **once**.
4. `AccelNode::on_activate()`: subscribe `/raw_floats`; advertise `/processed_floats`. `SyntheticPublisher` timer starts.
5. Per subscription callback: `enqueueWriteBuffer` → `enqueueNDRangeKernel` → `enqueueReadBuffer` → publish `/processed_floats`. Record `cl::Event` dispatch time. Log per-callback.
6. After `iterations` callbacks: `AccelNode` triggers `deactivate` → `cleanup` → `rclcpp::shutdown`.
7. `on_cleanup()`: RAII destroys `cl::Kernel`, `cl::CommandQueue`, `cl::Context`.

#### C2 (Costmap Inflation)
1. `main()`: `rclcpp::init` → create `CostmapNode` + `MapPublisher` in a single-threaded executor with intra-process comm enabled.
2. Lifecycle manager (in `main()`) calls `configure` → `activate` on `CostmapNode`.
3. `CostmapNode::on_configure()`: `create_context()` → `cl::CommandQueue` (CL_QUEUE_PROFILING_ENABLE) → build `inflate` + `inflate_tiled` kernels → declare parameters.
4. `MapPublisher`: reads `map_path` `.pgm` via `stb_image` → converts to `nav_msgs/OccupancyGrid` → publishes once to `/map`.
5. `CostmapNode` subscription callback receives `OccupancyGrid`:
   - Allocate `cl::Buffer` (uchar, width × height).
   - CPU path: run exact Euclidean DT reference → record `std::chrono` time.
   - GPU path (naive): dispatch `inflate` kernel → record `cl::Event` time.
   - GPU path (tiled, C2 Challenge): dispatch `inflate_tiled` kernel with `__local` tile → record `cl::Event` time.
   - Read back GPU result → compare against CPU reference (max deviation ±1 cost unit per cell).
   - Colorize result on CPU (RGBA: obstacles black, inflated gradient red, free white) → save `output_costmap.bmp`.
   - Print comparison table: CPU ms, GPU naive ms, GPU tiled ms, GPU-vs-CPU speedup, tiled-vs-naive speedup.
   - Publish `/inflated_costmap` (`nav_msgs/OccupancyGrid`).
6. After single map processed: `CostmapNode` requests `deactivate` → `cleanup` → `rclcpp::shutdown`.

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
Activated by `use_double_buffer:=true` at launch — same binary, same directory as C3.
1. Pre-allocate at `on_configure()` when `use_double_buffer` is `true`: three buffer pairs — `point_buf_[2]` (upload), `compact_buf_[2]` (compacted points), `feature_buf_[2]` (features); sized from `max_points` parameter (default 200 000 × 16 bytes/point). When `false`, allocate only index [0] and use the blocking path (base C3 behavior).
2. `std::atomic<int> active_buf_{0}` tracks the in-use index. Callback N reads `active_buf_.load()` to select its write-side buffers.
3. Non-blocking enqueue on active index: `enqueueWriteBuffer(CL_FALSE)` → filter → compact → feature kernels → `enqueueReadBuffer(CL_FALSE)`.
4. `cl::Event` set on the final `enqueueReadBuffer`; `setCallback(CL_COMPLETE, ...)` atomically swaps `active_buf_` via `store(1 - current)`. Callback executes in the OpenCL driver thread — only `std::atomic` operations are safe here.
5. Contention guard: if callback N+1 arrives and `active_buf_` still equals callback N's index (swap has not fired), call `queue_.finish()` and log `WARN: double-buffer contention — falling back to blocking wait`.
6. Callback N+1 fills the alternate buffer while GPU processes callback N's data — no blocking wait in the ROS 2 executor thread under normal load.

---

## Key Decisions (and Rationale)

1. **OpenCL Context as Node Member, Not Callback-Local**
   - **Why**: `cl::Context` construction costs 1–3 ms (platform query, device selection, driver initialization). In a 200 Hz callback (5 ms budget), this alone exceeds the entire latency gate. C1 exists solely to measure and internalize this cost.

2. **LifecycleNode as C1 Foundation (Not a Challenge)**
   - **Why**: `rclcpp::Node` with simulated callbacks teaches OpenCL context lifetime but is not a representative ROS 2 pattern. Making C1 itself a `LifecycleNode` aligns OpenCL resource lifetime (create/destroy) with the ROS 2 state machine (`on_configure`/`on_cleanup`) from the first example — the exact pattern students will replicate in C3. Teaching it once in C1 avoids re-explaining the same concept in later phases.

3. **Node Parameters (`declare_parameter`) for All ROS 2 Nodes; CLI11 Only for the C3 Synthetic Publisher**
   - **Why**: `declare_parameter` is idiomatic ROS 2 — parameters are introspectable via `ros2 param list`, overridable via launch files, and consistent with the rest of the ecosystem. C1, C2, and C3 nodes all use it. CLI11 applies only to the C3 synthetic publisher, which is a standalone tool binary, not a node.

4. **Intra-Process Communication for C1 Self-Containment**
   - **Why**: Real pub/sub requires a publisher. Running `SyntheticPublisher` in the same executor with intra-process comm enabled means messages are delivered as shared pointers without serialization or DDS — the binary is fully self-contained for CI while still exercising the real subscription callback path.

5. **C2 Uses Custom `MapPublisher` to Feed `/map`, Not `nav2_map_server`**
   - **Why**: `nav2_map_server` would require Nav2 installed and a `.yaml` sidecar file, turning C2 into a Nav2 integration exercise. C2's lesson is the distance transform kernel and GPU vs CPU speedup — not middleware setup. A small `MapPublisher` node (reads `.pgm` via `stb_image`, publishes `OccupancyGrid` intra-process) keeps C2 self-contained and consistent with the same-binary-two-nodes pattern established in C1.

6. **C2 CPU Reference Implementation Mandatory**
   - **Why**: The costmap is safety-critical in AMR navigation. Correctness must be verified pixel-by-pixel against a CPU reference before trusting GPU output. The CPU run also provides the speedup denominator for the performance gate.

7. **Single-Pass Filter Kernel (Ground Removal + Intensity in One Dispatch)**
   - **Why**: Two separate kernel dispatches would require two `enqueueNDRange` calls, two kernel launch overheads (~0.1–0.3 ms each), and an intermediate buffer. A combined predicate (`z >= ground_z && intensity >= min_intensity`) in one pass avoids both at the cost of a slightly more complex kernel — a clear win given the 5 ms gate.

8. **Compaction via Prefix-Sum (Not Stream Compaction to Host)**
   - **Why**: Transferring the raw filter mask to the CPU for compaction would add 0.8–1.5 ms per message. GPU prefix-sum compaction keeps the data on device until the final download. The compacted result is smaller than the original cloud, reducing download time.

9. **Loaned Messages as Optional Path, Not Default**
   - **Why**: Loaned messages require a specific RMW (fastrtps or Iceoryx) and may not be available in all environments. The standard path must work as a standalone baseline. The loaned path is the optimization target, enabled by a compile-time flag or runtime detection.

10. **Synthetic Publisher as Part of C3**
    - **Why**: Reproducible benchmarking requires a deterministic, parametric source. Bag files are large and environment-specific. A generator binary produces any desired cloud size at any Hz with zero external dependencies, making the performance gate verifiable on any machine.

11. **AoS Input, SoA as Mini-Challenge (Not Default)**
    - **Why**: `sensor_msgs/PointCloud2` natively stores points as AoS (XYZIXYZIXYZ…). The base implementation must mirror the real-world input format. The SoA conversion and its coalescing benefit is the mini-challenge, teaching the coalescing trade-off explicitly rather than silently pre-optimizing.

12. **Passthrough Kernel Copies 1M Floats (Not a No-Op)**
    - **Why**: A truly empty kernel body may be optimized away by drivers, producing artificially low dispatch times that misrepresent real workload cost. A 1M-element float copy is minimal but unambiguously real work that drivers cannot elide.

---

## Known Issues / Risks

- ~~**C1 `SyntheticPublisher` in `main.cpp` (not a separate file)**~~: Resolved in Task 041 — `synthetic_publisher.cpp` and `synthetic_publisher.hpp` were extracted; `CMakeLists.txt` updated to `add_executable(accel_node main.cpp synthetic_publisher.cpp)`.

- **`copy_kernels()` helper scope gap (Task 041 post-review)**: The `copy_kernels()` CMake helper in `common/common.cmake` was not in the original Task 041 scope. It was applied post-review to ensure kernel directories are copied correctly for all three targets via the standard post-build command. Noted here for audit trail; no further action required.

- **`find_package(rclcpp)` Fails Without Sourced Workspace**: CMake silently reports "not found" if `setup.bash` was not sourced. The CMakeLists.txt must check `$ENV{ROS_DISTRO}` and emit `message(FATAL_ERROR "...")` with instructions before attempting `find_package`.
- **OpenCL Context Thread Affinity**: `on_configure()` is called from the executor thread. On some drivers, GPU context affinity is thread-local — all subsequent enqueue calls (in subscription callbacks) must originate from the same thread. Single-threaded executor guarantees this; multi-threaded executor would require explicit thread pinning.
- **Loaned Messages Middleware Dependency**: `rmw_cyclonedds_cpp` does not support loaned messages. Users with `RMW_IMPLEMENTATION=rmw_cyclonedds_cpp` will fall back to copy-based transport; the node must print a warning and continue — the 5 ms gate may not be achievable on this path.

> **Note:** Loaned message support is version-dependent and no specific version of `rmw_cyclonedds_cpp` is cited. Verify against the current [rmw_cyclonedds release notes](https://github.com/ros2/rmw_cyclonedds) for the Jazzy-targeted release before treating this as a hard constraint.
- **`PointCloud2` `point_step` Variability**: Different Lidar drivers use different point formats (XYZ = 12 bytes, XYZI = 16 bytes, XYZRGB = 24 bytes). The filter kernel must be parameterized by `point_step` at dispatch time — hard-coding 16 bytes breaks with non-XYZI clouds.
- **`ros2 topic hz` Measurement Accuracy**: `ros2 topic hz` underreports rate for bursty publishers. Authoritative latency measurement uses per-message `steady_clock` timestamps logged by the node, not `ros2 topic hz`. The 200 Hz gate is verified via the node's own per-message log.
- **Costmap BMP Color Mapping**: The RGBA colorization (obstacle black, inflated red, free white) must be done on the CPU after readback — not in the kernel — to keep the kernel output a simple uchar cost buffer reusable in other pipelines.
- **Prefix-Sum Kernel Complexity**: A correct parallel prefix-sum (scan) implementation is non-trivial. The initial implementation may use a two-phase Blelloch scan; correctness must be verified against a CPU scan on the same data before integrating into the pipeline.

- **C2 LDS Tiling Yields ~1.0x on RTX 4060 / Radeon 680M (Task 038)**: Dense 2D neighbourhood scans are not LDS-bandwidth-bound. A 128-byte L1 cache line covers 128 `uchar` cells; a warp scanning the same search-window row generates at most one cache miss per row — the same reuse LDS would provide, without barrier overhead. Tiled was ~5% slower due to barrier cost across all tested map sizes (512²–2048²) and radii (r=10–60). Hardware-waiver † applies to the ≥ 1.5× speedup gate. The correct optimisation is algorithmic: separable 1D distance transform (Meijster/Saito) reduces O(r²) per-cell work to O(1) regardless of memory hierarchy.

- **`/cluster_features` Publishes Global Centroid Only**: The feature extraction publishes a single PointCloud2 point representing the combined centroid of all filtered points — not per-cluster centroids. Downstream consumers expecting per-cluster features must be aware of this limitation.
- **`/cluster_features` RViz Visibility**: The centroid point is invisible at RViz default PointCloud2 size (pixel style). Display requires Size ≥ 0.2 m (sphere style) in the PointCloud2 display settings.

---

## Performance Gates (Path Completion)

Hardware-waiver: gates marked with † may not be achievable on CPU-fallback or integrated GPU devices. If the hardware target is met for the primary GPU but not a fallback, the gate is considered passed with a note.

| Project | Metric | Target |
| :--- | :--- | :--- |
| C1 Node Acceleration | Per-callback kernel dispatch time | ≤ 0.5 ms (flat across all callbacks) † |
| C1 Node Acceleration | Context init overhead | Logged once in `on_configure()`; must not appear in per-callback log |
| C2 Costmap Inflation (naive) | GPU distance transform | < 10 ms @ 512×512 map † |
| C2 Costmap Inflation (tiled) | GPU distance transform | < 5 ms @ 512×512 map † |
| C2 Costmap tiled vs naive | Speedup | ≥ 1.5× (tiled over naive) reported in console † |
| C2 Costmap GPU vs CPU | Speedup | ≥ 5× (GPU naive over CPU) reported in console † |
| C3 Perception Node | End-to-end latency | < 5 ms @ 100k points (all stages summed) † |
| C3 Perception Node | Publish rate | ≥ 200 Hz sustained (measured via per-message node log) † |
| C3 Double-Buffer Challenge | Contention-free rate | Zero `WARN: double-buffer contention` log entries during 10 s @ 200 Hz, 100k pts † |

---

## Specifications & Standards

- **Directory Structure**:
  ```
  02_Projects/C_Robotics_ROS2/
  ├── RoboticsROS2.md                   (user-facing README, existing)
  ├── C1_Node_Acceleration/
  │   ├── CMakeLists.txt
  │   ├── main.cpp                      (binary: accel_node)
  │   ├── synthetic_publisher.cpp       (compiled into accel_node, not a separate binary)
  │   └── kernels/
  │       └── passthrough.cl
  ├── C2_Costmap_Inflation/
  │   ├── CMakeLists.txt
  │   ├── main.cpp                      (binary: costmap_node)
  │   ├── map_publisher.cpp             (compiled into costmap_node, not a separate binary)
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
  - C1: Console log showing `on_configure()` init time (once), then per-callback dispatch times (flat, ≤ 0.5 ms each) from real subscription callbacks. No BMP required.
  - C2: `output_costmap.bmp` (correct colorization — obstacles black, inflation red gradient, free white). Console table with CPU / GPU naive / GPU tiled times, GPU-vs-CPU speedup, tiled-vs-naive speedup.
  - C3: Console per-message stage breakdown summing to < 5 ms. Node log showing ≥ 200 Hz inter-message rate. No BMP required (structured timing table satisfies master spec §3 numeric-tool exception).
  - C3 Challenge (Double-Buffer): Build and launch from `C3_Perception_Node/`:
    ```bash
    cmake -B build && cmake --build build
    GPU=NVIDIA ./build/perception_node --ros-args -p topic:=/points -p ground_z:=0.1 -p min_intensity:=50.0 -p max_points:=200000 -p use_double_buffer:=true
    ./build/point_cloud_publisher --scene mixed --topic /points --hz 200 --points 100000
    ```
    Mixed scene (`--scene mixed`): 3 valid clusters at (2,0,1), (−2,0,1), (0,3,1) r=0.3 m intensity 150; ground band z∈[−0.1, 0.05] m intensity 200 (filtered by `ground_z:=0.1`); low-intensity blob at (0,0,2) intensity 10 (filtered by `min_intensity:=50`). Expected: `/filtered_points` contains only the 3 spherical clusters (~30% of points); `/cluster_features` publishes one centroid feature (global centroid of all filtered points); zero `WARN: double-buffer contention` log entries over 10 s at 200 Hz. RViz setup: Fixed Frame `lidar_link`; add PointCloud2 on `/filtered_points`; add PointCloud2 on `/cluster_features` with **Size ≥ 0.2 m** (sphere style) — the centroid is a single point and invisible at default pixel size. Use `--scene grid` (default) for throughput benchmarking only.
- **Tooling** (module-specific additions to master_specs):
  - `stb_image` / `stb_image_write` for `.pgm` input and `output_costmap.bmp` output (C2 `MapPublisher` + colorization).
  - ROS 2 packages: `rclcpp`, `rclcpp_lifecycle`, `sensor_msgs`, `nav_msgs`, `std_msgs` via `find_package(... REQUIRED)`. Required by C1, C2, and C3.
  - No third-party Lidar SDK. No PCL dependency.
- **Note**: No OpenCL 2.0 features are required by this path.

---

## Prerequisites

- Module 1 completed (`01_Host_API/`): `cl.hpp` usage, `cl::Event` profiling, `CL_CHECK` error handling.
- ROS 2 Jazzy: see [`02_Projects/C_Robotics_ROS2/SETUP.md`](../../02_Projects/C_Robotics_ROS2/SETUP.md). Required by C1, C2, and C3.
- `assets/warehouse.pgm` (512×512 or larger occupancy grid) for C2.
- `assets/lidar_sample.bag` (optional, for C3 realistic replay). Synthetic publisher covers the no-bag case.
- RMW set to a loaned-message-capable implementation for optimal C3 performance:
  `export RMW_IMPLEMENTATION=rmw_fastrtps_cpp`

See [main README](../../README.md) for base requirements (OpenCL 1.2+, CMake 3.18+, Docker setup).
