# Path C: Robotics & ROS 2

Accelerate a real ROS 2 perception pipeline without breaking the node contract. You start by wiring an OpenCL context into a LifecycleNode's state machine, apply it to a real AMR navigation problem (costmap inflation), then eliminate the serialization overhead that makes Lidar processing miss real-time deadlines.

## Prerequisites
See [main README](../../README.md) for base requirements (OpenCL, CMake, Docker setup).

**Additional:**
- ROS 2 Jazzy: see [SETUP.md](SETUP.md) for APT repo, sourcing, and RMW configuration. Required for C1, C2, and C3.
- Assets in repository root: `assets/warehouse.pgm` (512×512+ occupancy grid, required for C2). `assets/lidar_sample.bag` — **optional** for C3; the synthetic publisher (`./build/point_cloud_publisher`) is the easy path and requires no bag file.

> **Assumption**: You know ROS 2 basics — nodes, pub/sub, topics, `rclcpp`. This track focuses exclusively on GPU acceleration inside that model.

> **C1 uses `rclcpp_lifecycle::LifecycleNode`**, a step beyond basic pub/sub. New to lifecycle nodes? See the [ROS 2 Jazzy Lifecycle Tutorial](https://docs.ros.org/en/jazzy/Tutorials/Intermediate/Managed-Nodes.html) first. If you've already read a lifecycle tutorial, the [Managing-A-ROS-2-Node-Lifecycle](https://docs.ros.org/en/jazzy/Tutorials/Intermediate/Managing-A-ROS-2-Node-Lifecycle.html) walkthrough also covers the state machine in depth.

## Contents
```
C1_Node_Acceleration/   OpenCL context inside a LifecycleNode — init in on_configure(), dispatch in callbacks
C2_Costmap_Inflation/   2D costmap inflation kernel: distance transform for obstacle padding
C3_Perception_Node/     Flagship: Lidar filtering + feature extraction, < 5 ms end-to-end
                          use_double_buffer:=true — double-buffer real-time guarantee (C3 Challenge)
```

---

## C1_Node_Acceleration — OpenCL Inside a LifecycleNode

**Goal**: Wire an OpenCL context into a `rclcpp_lifecycle::LifecycleNode`. OpenCL resources are created in `on_configure()` — once — and destroyed in `on_cleanup()` by RAII. Per-callback dispatch time stays flat. Prove context init is paid exactly once, not per message.

### Build & run
```bash
source /opt/ros/jazzy/setup.bash
cd C1_Node_Acceleration
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/accel_node
# GPU selection: GPU=NVIDIA ./build/accel_node
# Override parameters at launch:
# ./build/accel_node --ros-args -p iterations:=20 -p buffer_size:=2097152
```

Parameters (set via `declare_parameter`, overridable at launch):
- `iterations` — number of callback cycles (int, default `10`)
- `buffer_size` — floats per message (int, default `1048576`)

### Verify
Console output:
```
[on_configure] OpenCL context initialized: NVIDIA GeForce RTX 3080 (2.1 ms)
[on_activate ] Subscribed to /raw_floats. Publishing to /processed_floats.
[callback  1 ] kernel dispatched in 0.310 ms
[callback  2 ] kernel dispatched in 0.290 ms
[callback  3 ] kernel dispatched in 0.330 ms
...
[on_cleanup  ] OpenCL resources released.
```

Context init appears once (in `on_configure`). Per-callback dispatch time stays flat. No re-initialization between callbacks.

### Core Concept: LifecycleNode and OpenCL Resource Lifetime

`rclcpp_lifecycle::LifecycleNode` gives OpenCL resources a natural home: the state machine maps directly onto resource lifetime.

```cpp
class AccelNode : public rclcpp_lifecycle::LifecycleNode {
public:
    AccelNode() : LifecycleNode("accel_node") {
        declare_parameter("iterations", 10);
        declare_parameter("buffer_size", 1048576);
    }

    rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn
    on_configure(const rclcpp_lifecycle::State &) override {
        int buf_size = get_parameter("buffer_size").as_int();
        // OpenCL init happens once, here — never inside a callback.
        ctx_    = create_context();   // GPU=<vendor> env var controls device selection
        queue_  = cl::CommandQueue(ctx_, device_, CL_QUEUE_PROFILING_ENABLE);
        kernel_ = build_kernel(ctx_, "passthrough.cl", "passthrough");
        buf_    = cl::Buffer(ctx_, CL_MEM_READ_WRITE,
                             static_cast<size_t>(buf_size) * sizeof(float));
        RCLCPP_INFO(get_logger(), "OpenCL context initialized.");
        return CallbackReturn::SUCCESS;
    }

    rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn
    on_activate(const rclcpp_lifecycle::State &) override {
        sub_ = create_subscription<std_msgs::msg::Float32MultiArray>(
            "/raw_floats", 10,
            std::bind(&AccelNode::on_message, this, std::placeholders::_1));
        pub_ = create_publisher<std_msgs::msg::Float32MultiArray>("/processed_floats", 10);
        return CallbackReturn::SUCCESS;
    }

private:
    cl::Context      ctx_;
    cl::CommandQueue queue_;
    cl::Kernel       kernel_;
    cl::Buffer       buf_;
    // Subscription and publisher live only between activate/deactivate.
};
```

`on_cleanup()` is implicit: RAII destroys `ctx_`, `queue_`, `kernel_`, and `buf_` when the node transitions to `unconfigured`. No manual `clRelease*` calls needed.

The wrong pattern — creating a context inside a callback:
```cpp
void on_message(const std_msgs::msg::Float32MultiArray::SharedPtr msg) {
    cl::Context ctx(device);  // 2 ms overhead — every callback!
    // ...
}
```
At 200 Hz (5 ms budget), that one line consumes 40% of your entire latency budget per message.

### Mini-challenge
Run the node with `buffer_size:=524288`, `buffer_size:=1048576`, and `buffer_size:=4194304`. Record the per-callback dispatch times reported by `cl::Event` profiling for each size. At what buffer size does dispatch latency become non-trivial relative to the 0.5 ms gate? Tabulate the results.

> **Note**: The node self-manages its lifecycle — configure, activate, run N iterations, then clean up automatically. No manual `ros2 lifecycle set` commands are needed. Dispatch time is already printed per-callback; no extra instrumentation is needed.

---

## C2_Costmap_Inflation — Distance Transform on GPU

**Goal**: Implement a 2D costmap inflation kernel — the algorithm that pads obstacles with a cost gradient so a robot's path planner steers clear of walls — and verify it runs fast enough to keep up with a 10 Hz map update rate.

A distance transform assigns to each free cell the Euclidean distance to the nearest obstacle cell. The inflation kernel computes this per-cell distance and maps it to a cost value.

### Build & run

Run from `C2_Costmap_Inflation/`:

```bash
source /opt/ros/jazzy/setup.bash
cd C2_Costmap_Inflation
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/costmap_node --ros-args -p map_path:=../../../assets/warehouse.pgm
# Full parameter override:
# GPU=NVIDIA ./build/costmap_node --ros-args \
#     -p map_path:=../../../assets/warehouse.pgm \
#     -p inflation_radius:=0.5 \
#     -p resolution:=0.05 \
#     -p decay:=3.0
```

Parameters (set via `declare_parameter`, overridable at launch):
- `map_path` — path to `.pgm` occupancy grid (string, required)
- `inflation_radius` — inflation radius in metres (double, default `0.5`)
- `resolution` — metres per cell (double, default `0.05`)
- `decay` — cost decay rate (double, default `3.0`)

### Verify
- `output_costmap.bmp` — obstacles are black, inflated zone is a red gradient, free space is white
- Console prints a three-row comparison table:
  ```
  Map: 512x512, inflation radius: 10 cells
  [CPU          ] Distance transform: 48.200 ms
  [GPU naive    ] Distance transform:  3.100 ms   Speedup vs CPU: 15.6x
  [GPU tiled    ] Distance transform:  1.800 ms   Speedup vs naive: 1.7x
  ```
  GPU naive must be < 10 ms and GPU tiled must be < 5 ms to pass the gate.

### Core Concept: Why Inflation?

A robot is not a point — it has a body. A path that passes 2 cm from a wall is geometrically valid but physically dangerous. The costmap inflation layer assigns high cost to cells within `inflation_radius` of any obstacle. The path planner then minimizes cost, naturally keeping the robot away from walls.

**The algorithm** is a distance transform: for every free cell, compute the distance to the nearest obstacle cell and assign `cost = f(distance)`. The naive O(N²) approach tests every free cell against every obstacle cell. The GPU version parallelizes the per-cell distance computation — each work item owns one cell.

```cl
__kernel void inflate(__global const uchar* obstacles,
                      __global uchar* costmap,
                      int width, int height,
                      int radius, float decay) {
    size_t x = get_global_id(0);
    size_t y = get_global_id(1);
    if (x >= (size_t)width || y >= (size_t)height) return;
    float min_dist = (float)radius + 1.0f;
    for (int dy = -radius; dy <= radius; dy++) {
        for (int dx = -radius; dx <= radius; dx++) {
            int nx = (int)x + dx, ny = (int)y + dy;
            if (nx >= 0 && ny >= 0 && nx < width && ny < height)
                if (obstacles[ny * width + nx] > 0)
                    min_dist = fmin(min_dist, sqrt((float)(dx*dx + dy*dy)));
        }
    }
    costmap[y * width + x] = (min_dist <= (float)radius)
        ? (uchar)(255.0f * exp(-decay * min_dist)) : 0;
}
```

**Memory access pattern**: the inner loop reads `obstacles` at offsets scattered across a `2×radius` window. Run the kernel, look at the GPU memory bandwidth utilization — then ask why a neighborhood read pattern from global memory might be inefficient.

### Mini-challenge
Write a second version of the kernel that loads a tile of the obstacle map into `__local` memory before the inner loop. Profile naive vs tiled — at what tile size does the local-memory version peak? Does the crossover radius (below which local memory does not help) match your expectation?

> **Local memory primer**: `__local` declares per-workgroup shared memory — e.g., `__local float tile[16]`. All work-items in the group can read and write it. Use `barrier(CLK_LOCAL_MEM_FENCE)` to synchronize threads within the group before reading shared data filled by other work-items. See [Toolbox: Local Memory](../../99_Toolbox/LocalMemory/LocalMemory.md) for the full tiling pattern.

---

## C3_Perception_Node — Flagship Project

**Goal**: Build a ROS 2 node that receives a Lidar point cloud, filters it on GPU (ground removal + intensity threshold), extracts per-cluster features, and publishes the result — end-to-end latency < 5 ms for 100k points.

### Build & run

> Both terminals must source `/opt/ros/jazzy/setup.bash` and export `RMW_IMPLEMENTATION=rmw_fastrtps_cpp` before running.

```bash
source /opt/ros/jazzy/setup.bash
export RMW_IMPLEMENTATION=rmw_fastrtps_cpp
cd C3_Perception_Node
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build

# Terminal 1 — run the perception node (parameters via declare_parameter, GPU via env var)
GPU=NVIDIA ./build/perception_node --ros-args \
    -p topic:=/points \
    -p ground_z:=0.2 \
    -p min_intensity:=10.0

# Terminal 2 — publish synthetic data (CLI11 flags, standalone tool binary)
./build/point_cloud_publisher --topic /points --hz 200 --points 100000

# Or replay a recorded bag (optional — synthetic publisher above is the easy path):
ros2 bag play ../../../assets/lidar_sample.bag
```

Node parameters (via `declare_parameter`):
- `topic` — input point cloud topic (string)
- `ground_z` — ground plane Z threshold in metres (double)
- `min_intensity` — minimum intensity to pass filter (double)
- `max_points` — pre-allocated buffer capacity in points (int, default `200000`); messages exceeding this size log a WARN and trigger a blocking realloc
- `use_double_buffer` — enable double-buffer real-time mode (bool, default `false`; see C3 Challenge)

Synthetic publisher flags (CLI11, standalone binary `point_cloud_publisher`):
- `--topic` — topic to publish on
- `--hz` — publish rate in Hz
- `--points` — number of points per cloud
- `--scene` — scene type: `grid` (default, throughput benchmark) or `mixed` (visual validation)

### Verify
- Node publishes `/filtered_points` and `/cluster_features` topics
- Terminal 3 (can be run after both nodes are running): `ros2 topic hz /filtered_points` — reports approximately 200 Hz
- Console prints per-message stage breakdown:
  ```
  [RECV ] PointCloud2 deserialized:  0.400 ms
  [GPU  ] Upload (100k pts):         0.800 ms
  [GPU  ] Filter kernel:             1.100 ms
  [GPU  ] Feature extraction:        1.600 ms
  [GPU  ] Download results:          0.500 ms
  [SEND ] Publish filtered cloud:    0.300 ms
  Total:                             4.700 ms  <- must be < 5 ms to pass
  ```

### Scene types

- `grid` (default): uniform XYZ grid, z∈[0.5, 5.4] m, intensity∈[50, 249]. All points pass the default filter. Use for throughput benchmarking.
- `mixed`: design-doc validation scene. Point budget: 30% valid clusters, 40% ground band, 30% low-intensity blob:
  - 3 valid clusters at (2, 0, 1), (−2, 0, 1), (0, 3, 1), r = 0.3 m, intensity 150 — pass filter
  - Ground band z∈[−0.1, 0.05] m, intensity 200 — removed by `ground_z:=0.1`
  - Low-intensity blob at (0, 0, 2), intensity 10 — removed by `min_intensity:=50`

### Core Concept: The Serialization Problem (Loaned Messages)

A `sensor_msgs/PointCloud2` message with 100k points (XYZ + intensity, float32) is 1.6 MB. The standard ROS 2 subscriber deserializes this from shared memory into a `PointCloud2` struct, which you then copy to a `cl::Buffer`. That is two copies before the GPU sees a single point.

**Loaned Messages** (ROS 2 Jazzy with compatible RMW) eliminate the first copy. The middleware loans you a pre-allocated message buffer directly in the publisher's shared memory region. If the publisher and subscriber are in the same process — or use a zero-copy transport like Iceoryx — no serialization occurs at all. The underlying GPU-side principle is the same as [Toolbox: Zero-Copy](../../99_Toolbox/ZeroCopy/ZeroCopy.md) — pinned or shared memory avoids the pageable-copy overhead on every transfer.

Iceoryx pre-allocates a fixed shared-memory pool at startup. When you release a `unique_ptr<Message>`, the slot returns to the pool — no heap allocation per message. See the [Iceoryx documentation](https://iceoryx.io/latest/) for pool configuration and publisher/subscriber setup.

```cpp
// Standard (two copies: shm -> ROS msg -> cl::Buffer)
sub_ = create_subscription<PointCloud2>("points", 10,
    [this](PointCloud2::SharedPtr msg) {
        CL_CHECK(queue_.enqueueWriteBuffer(buf_, CL_FALSE, 0,
            msg->data.size(), msg->data.data(), nullptr, &upload_event_));
    });

// Loaned (one copy: shm -> cl::Buffer directly)
sub_ = create_subscription<PointCloud2>("points", rclcpp::QoS(10),
    [this](std::unique_ptr<PointCloud2> msg) {  // loaned unique_ptr
        CL_CHECK(queue_.enqueueWriteBuffer(buf_, CL_FALSE, 0,
            msg->data.size(), msg->data.data(), nullptr, &upload_event_));
        // msg released back to middleware — no deallocation by us
    });
```

To reach < 5 ms you will likely need the loaned message path. The standard path typically adds 0.8–1.5 ms on a 1.6 MB message.

### Core Concept: Filtering Kernel Design

The filter must produce a dense output array — feature extraction expects contiguous points with no gaps. A naive approach that marks rejected points in-place with a sentinel leaves holes; a second pass is needed to close them. Think about how many kernel launches and buffer round-trips that requires, and whether any of them can be merged or overlapped.

The filter predicate combines two conditions in a single dispatch:
```cl
bool keep = (pt.z >= ground_z) && (pt.intensity >= min_intensity);
```
Two separate kernel dispatches would each add ~0.1–0.3 ms of launch overhead. A single combined predicate avoids both at the cost of a slightly more complex kernel.

### Challenge: Real-Time Guarantee
See [§C3_DoubleBuffer_Challenge](#c3_doublebuffer_challenge--double-buffer-real-time-guarantee) below for the full build and verification steps. The short version: make the node miss-proof by pre-allocating two buffer pairs at `on_configure()`. While the GPU processes buffer N, the CPU fills buffer N+1. A `cl::Event` callback atomically swaps the active index when the GPU finishes. Contention (swap not yet fired when the next message arrives) is logged as `WARN: double-buffer contention` and falls back to a blocking wait — silent drop is forbidden.

### Mini-challenge
Replace the point cloud data layout from Array-of-Structs (AoS: `XYZIXYZIXYZ...`) to Structure-of-Arrays (SoA: `XXX...YYY...ZZZ...III...`). Profile the filter kernel before and after. Which layout wins, and why? See [Toolbox: Memory Coalescing](../../99_Toolbox/Toolbox.md) if you need a starting point.

---

## C3_DoubleBuffer_Challenge — Double-Buffer Real-Time Guarantee

**Goal**: Extend `C3_Perception_Node` so the subscriber thread is never blocked waiting for the GPU. While the GPU processes buffer N, the CPU fills buffer N+1. Verify zero `WARN: double-buffer contention` log entries over a sustained 10-second run at 200 Hz.

The base C3 node calls `queue_.finish()` (or equivalent) inside the subscriber callback. At 200 Hz (5 ms inter-message interval), any GPU stage that spills past 5 ms stalls the executor and drops the next message. The double-buffer pattern decouples arrival from dispatch: the callback always writes into the idle buffer and returns immediately.

### Core Concept: Double-Buffer Pattern

Pre-allocate two identical buffer sets at `on_configure()` using the `max_points` parameter (default 200 000 points):

```cpp
// Each buffer pair covers one in-flight message. Sized at configure-time — never reallocated per-message.
for (int i = 0; i < 2; ++i) {
    point_buf_[i]   = cl::Buffer(ctx_, CL_MEM_READ_WRITE,
                                 static_cast<size_t>(max_points_) * point_step_);
    compact_buf_[i] = cl::Buffer(ctx_, CL_MEM_READ_WRITE,
                                 static_cast<size_t>(max_points_) * point_step_);
    feature_buf_[i] = cl::Buffer(ctx_, CL_MEM_READ_WRITE,
                                 static_cast<size_t>(max_points_) * sizeof(float) * 5);
}
```

`std::atomic<int> active_buf_{0}` tracks which index the GPU currently owns. Each subscriber callback:

1. Reads `active_buf_.load()` — this is its write index.
2. Checks for contention: if the GPU has not yet fired the swap (index unchanged from previous callback), logs `WARN: double-buffer contention — falling back to blocking wait` and calls `CL_CHECK(queue_.finish())`.
3. Issues all enqueue calls (`enqueueWriteBuffer`, `enqueueNDRangeKernel` ×3, `enqueueReadBuffer`) as non-blocking (`CL_FALSE`).
4. Sets a `cl::Event` callback on the final `enqueueReadBuffer`. When `CL_COMPLETE` fires (in the OpenCL driver thread), the callback calls `active_buf_.store(1 - current)`. Only `std::atomic` operations are safe in an OpenCL event callback — no ROS 2 API calls, no logging, no heap allocation.

   > **Thread-safety**: OpenCL fires event callbacks from an internal driver thread — not the ROS 2 executor thread. Only thread-safe primitives like `std::atomic` are safe to use in those callbacks. Calling `rclcpp` logging, publishers, or any heap allocator from an event callback is undefined behaviour.

5. Returns immediately — the subscriber thread is free before the GPU finishes.

### Build & run

> Both terminals must source `/opt/ros/jazzy/setup.bash` and export `RMW_IMPLEMENTATION=rmw_fastrtps_cpp`.

```bash
source /opt/ros/jazzy/setup.bash
export RMW_IMPLEMENTATION=rmw_fastrtps_cpp
cd C3_Perception_Node
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build

# Terminal 1 — perception node with double-buffer enabled
GPU=NVIDIA ./build/perception_node --ros-args \
    -p topic:=/points \
    -p ground_z:=0.1 \
    -p min_intensity:=50.0 \
    -p max_points:=200000 \
    -p use_double_buffer:=true

# Terminal 2 — synthetic publisher at 200 Hz
./build/point_cloud_publisher --topic /points --hz 200 --points 100000
```

### Verify

**Console (10-second run, pass condition):**
```
[callback  1 ] upload dispatched (non-blocking). GPU owns buf[0].
[callback  2 ] upload dispatched (non-blocking). GPU owns buf[1].
[callback  3 ] upload dispatched (non-blocking). GPU owns buf[0].
...
[SUMMARY] 2000 messages in 10.001 s — 0 contention events.
```

Zero `WARN: double-buffer contention` entries over 10 seconds at 200 Hz, 100k points is the pass condition.

> **Note**: The 5 ms latency gate may not be achievable on all hardware. Check the Performance Gate table for hardware-specific expectations (gates marked † apply a hardware waiver for CPU-fallback and integrated GPU devices).

**MANUAL — RViz verification:** Open RViz, add a PointCloud2 display on `/filtered_points`. With the publisher running at `--hz 200 --points 100000` and the node launched with `ground_z:=0.1 min_intensity:=50`, the display must show only the three valid clusters — no ground or low-intensity points — with no visible frame drops or stuttering over 10 seconds.

Mixed-scene layout for verification:
- 3 valid clusters at (2, 0, 1), (−2, 0, 1), (0, 3, 1), radius 0.3 m, intensity 150 — must appear in `/filtered_points`
- Ground band z ∈ [−0.1, 0.05] m, intensity 200 — removed by `ground_z:=0.1`
- Low-intensity cloud at (0, 0, 2), intensity 10 — removed by `min_intensity:=50`
- `/cluster_features` centroids must be within 0.05 m of the three cluster positions above

### Testing hints

- Use `--scene mixed` for the double-buffer challenge run (not `grid`), so you can visually confirm correct filtering in RViz while checking for zero contention.
- RViz setup: Fixed Frame `lidar_link`; add PointCloud2 on `/filtered_points`; add PointCloud2 on `/cluster_features` with **Size >= 0.2 m** (sphere style) — the centroid is a single point and invisible at default pixel size.
- Quick spot-check:
  ```bash
  ros2 topic echo /cluster_features --once
  ```
  Centroids must be within 0.05 m of the three cluster positions: (2, 0, 1), (−2, 0, 1), (0, 3, 1).
- Use `--scene grid` for throughput benchmarking only (Hz and latency gates).

### Mini-challenge

Two parts, in order:

1. **AoS to SoA inside the double-buffer**: Modify the upload path to convert AoS (`XYZIXYZIXYZ...`) to SoA (`XXX...YYY...ZZZ...III...`) on the CPU before `enqueueWriteBuffer`. Profile the filter kernel before and after. Which layout wins, and why? See [Toolbox: Memory Coalescing](../../99_Toolbox/Toolbox.md).

2. **Measure the real-time ceiling**: Run `--hz 400 --points 100000` (double the nominal rate). Count `WARN: double-buffer contention` entries over 10 seconds. At what Hz does contention first appear on your hardware? That is your pipeline's true real-time ceiling under this buffer strategy.

---

## Performance Gate

This track is complete when all gates below are met. Gates marked with † may not be achievable on CPU-fallback or integrated GPU devices — if the primary GPU target passes but a fallback does not, the gate is considered passed with a note.

| Project | Metric | Target |
|:--------|:-------|:-------|
| C1 Node Acceleration | Per-callback kernel dispatch time | <= 0.5 ms flat across all callbacks † |
| C1 Node Acceleration | Context init overhead | Logged once in `on_configure()`; must not appear in per-callback log |
| C2 Costmap Inflation (naive) | GPU distance transform | < 10 ms @ 512×512 map † |
| C2 Costmap Inflation (tiled) | GPU distance transform | < 5 ms @ 512×512 map † |
| C2 tiled vs naive | Speedup | >= 1.5× reported in console † |
| C2 GPU vs CPU | Speedup | >= 5× (GPU naive over CPU) reported in console † |
| C3 Perception Node | End-to-end latency | < 5 ms @ 100k points (all stages summed) † |
| C3 Perception Node | Publish rate | >= 200 Hz sustained (measured via per-message node log) † |
| C3 Double-Buffer Challenge | Contention-free rate | Zero `WARN: double-buffer contention` log entries during 10 s @ 200 Hz, 100k pts † |

**Measure with `cl::Event` profiling** on every GPU stage. CPU-bound stages (deserialization, publish) use `std::chrono::steady_clock`. All times reported in ms to 3 decimal places.

**If you are over budget**: break down the per-stage times from the console output and identify which stage dominates. Then consult the [Optimization Toolbox](../../99_Toolbox/Toolbox.md) for the technique that matches your bottleneck.

---

## Known Issues

- **C2 LDS tiling yields ~1.0x on RTX 4060 / Radeon 680M**: Dense 2D neighbourhood scans are not LDS-bandwidth-bound on these architectures. A 128-byte L1 cache line covers 128 `uchar` cells; a warp scanning the same search-window row generates at most one cache miss per row — the same reuse LDS would provide, without the barrier overhead. Tiled was ~5% slower than naive across all tested map sizes (512²–2048²) and radii (r=10–60). The hardware-waiver † applies to the C2 tiled ≥ 1.5× speedup gate on these devices. The correct optimisation for large radii is algorithmic: a separable 1D distance transform (Meijster/Saito) reduces O(r²) per-cell work to O(1) regardless of memory hierarchy.

- **C1 `SyntheticPublisher` merged into `main.cpp`**: The design spec lists `synthetic_publisher.cpp` as a separate source file compiled into `accel_node`. The current implementation merged `SyntheticPublisher` directly into `main.cpp`. Functionally identical; the file split is cosmetic and will be aligned in the Module Cleanup phase.

---

## Troubleshooting

- **`source /opt/ros/jazzy/setup.bash` must be run before CMake**: Without it, `find_package(rclcpp REQUIRED)` fails. Add it to your `.bashrc` or use a ROS 2 dev container. The `CMakeLists.txt` checks `$ENV{ROS_DISTRO}` and emits a fatal error with instructions if unset.
- **Loaned Messages not available**: Requires `rmw_fastrtps_cpp` or Iceoryx middleware. Check: `echo $RMW_IMPLEMENTATION`. Set `export RMW_IMPLEMENTATION=rmw_fastrtps_cpp` if using the default. The node prints a warning and falls back to copy-based transport automatically.
- **OpenCL context fails inside the node**: Ensure `on_configure()` — not the constructor — creates the context. On some drivers, GPU context affinity is thread-local; the single-threaded executor guarantees all callbacks (including `on_configure`) run on the same thread.
- **`ros2 topic hz` shows half the expected rate**: The node is blocking on `clFinish()` inside the callback. Replace with non-blocking enqueue + event callback to release the subscriber thread immediately.
- **PointCloud2 data size unexpected**: Always compute buffer size as `msg->width * msg->height * msg->point_step` — `point_step` is read from the message header and varies by Lidar driver. Never hard-code 16 bytes.
- **Double-buffer contention at nominal rate (C3 Challenge)**: If `WARN: double-buffer contention` appears at 200 Hz, the GPU pipeline is taking longer than 5 ms. Break down per-stage times from the console log and identify the bottleneck stage. The contention guard is a safety valve, not a normal operating mode.
- **Wrong GPU selected**: `GPU=NVIDIA ./build/accel_node`, `GPU=AMD ./build/costmap_node`, `GPU=INTEL ./build/perception_node`.

---

## What's Next

[Track A: Multimedia](../A_Multimedia/Multimedia.md) — zero-copy video pipelines and edge AI inference.

[Track B: Graphics/HPC](../B_Graphics_HPC/GraphicsHPC.md) — ray tracing, CLBlast, and BVH acceleration structures.

[Optimization Toolbox](../../99_Toolbox/Toolbox.md) — Local Memory, Async Pipelines, Zero-Copy, Multi-GPU.

[Module 4 Add-ons](../../04_Addons/Addons.md) — 3D Voxel Mapping grand finale (combines B ray casting with C Lidar data).
