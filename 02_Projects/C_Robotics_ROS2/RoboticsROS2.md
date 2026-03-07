# Path C: Robotics & ROS 2

Accelerate a real ROS 2 perception pipeline without breaking the node contract. You start by wiring an OpenCL context into a node lifecycle, apply it to a real AMR navigation problem (costmap inflation), then eliminate the serialization overhead that makes Lidar processing miss real-time deadlines.

## Prerequisites
See [main README](../../README.md) for base requirements (OpenCL, CMake, Docker setup).

**Additional:**
- ROS 2 Humble or later: `sudo apt install ros-humble-desktop`
  - Required packages: `ros-humble-rclcpp`, `ros-humble-sensor-msgs`, `ros-humble-nav-msgs`
  - Source workspace before every build: `source /opt/ros/humble/setup.bash`
  - Verify: `ros2 --version`
- C2 standalone demo (`C2_Costmap_Inflation`) has no ROS 2 dependency — builds without sourcing.
- Assets in repository root: `assets/warehouse.pgm` (512×512+ occupancy grid, required for C2). `assets/lidar_sample.bag` (optional — synthetic publisher covers the no-bag case for C3).
- C3 optimal performance: `export RMW_IMPLEMENTATION=rmw_fastrtps_cpp` (enables loaned messages). Standard path works without it.

> **Assumption**: You know ROS 2 basics — nodes, pub/sub, topics, `rclcpp`. This track focuses exclusively on GPU acceleration inside that model.

## Contents
```
C1_Node_Acceleration/   OpenCL context inside a ROS 2 node lifecycle (setup, shutdown, error handling)
C2_Costmap_Inflation/   2D costmap inflation kernel: distance transform for obstacle padding
C3_Perception_Node/     Flagship: Lidar filtering + feature extraction, < 5 ms end-to-end
```

---

## C1_Node_Acceleration — OpenCL Inside a Node

**Goal**: Wire an OpenCL context into a `rclcpp::Node`, verify it survives subscriber callbacks without re-initialization, and measure the overhead of context creation so you never do it inside a callback.

### Build & run
```bash
source /opt/ros/humble/setup.bash
cd C1_Node_Acceleration
cmake -B build
cmake --build build
./build/opencl_node_demo
# GPU=NVIDIA ./build/opencl_node_demo
```

### Verify
Console output:
```
[INFO] OpenCL context initialized: NVIDIA GeForce RTX 3080 (2.1 ms)
[INFO] Node ready. Simulating 10 subscriber callbacks...
[INFO] Callback 1: kernel dispatched in 0.31 ms
[INFO] Callback 2: kernel dispatched in 0.29 ms
...
[INFO] Context alive after 10 callbacks. Shutdown clean.
```
Context init appears once. Per-callback time stays flat. No re-initialization.

### Core Concept: Context Lifetime in a Node

The wrong pattern — creating a context inside a callback:
```cpp
void on_pointcloud(const sensor_msgs::msg::PointCloud2::SharedPtr msg) {
    cl::Context ctx(device);  // 2 ms overhead — every callback!
    // ...
}
```

The right pattern — context as a node member, initialized in the constructor:
```cpp
class AccelNode : public rclcpp::Node {
public:
    AccelNode() : Node("accel_node") {
        ctx_ = cl::Context(select_device());   // once, at startup
        queue_ = cl::CommandQueue(ctx_, device_, CL_QUEUE_PROFILING_ENABLE);
        kernel_ = build_kernel(ctx_, "process.cl", "process");
        sub_ = create_subscription<sensor_msgs::msg::PointCloud2>(
            "points", 10,
            std::bind(&AccelNode::on_pointcloud, this, _1));
    }
private:
    cl::Context ctx_;
    cl::CommandQueue queue_;
    cl::Kernel kernel_;
    // ...
};
```

Destructor cleanup is automatic via RAII (`cl::Context`, `cl::CommandQueue` release on destruction — matching the node's own RAII lifecycle).

### Mini-challenge
Add a ROS 2 Lifecycle Node wrapper (`rclcpp_lifecycle::LifecycleNode`). Create the OpenCL context in `on_activate()` and destroy it in `on_deactivate()`. Verify the context is absent when the node is in `inactive` state.

---

## C2_Costmap_Inflation — Distance Transform on GPU

**Goal**: Implement a 2D costmap inflation kernel — the algorithm that pads obstacles with a cost gradient so a robot's path planner steers clear of walls — and verify it runs fast enough to keep up with a 10 Hz map update rate.

### Build & run
```bash
cd C2_Costmap_Inflation
cmake -B build
cmake --build build
./build/costmap_inflation_demo --map ../../../assets/warehouse.pgm --radius 0.5 --resolution 0.05
# Headless: produces output_costmap.bmp
```

### Verify
- `output_costmap.bmp` — obstacles are black, inflated zone is red gradient, free space is white
- Console prints:
  ```
  Map: 512x512, inflation radius: 10 cells
  [CPU  ] Distance transform:  48.2 ms
  [GPU  ] Distance transform:   3.1 ms   Speedup: 15.6x
  ```
  GPU must be < 10 ms to keep up with a 10 Hz map update rate (100 ms budget).

### Core Concept: Why Inflation?

A robot is not a point — it has a body. A path that passes 2 cm from a wall is geometrically valid but physically dangerous. The costmap inflation layer assigns high cost to cells within `inflation_radius` of any obstacle. The path planner then minimizes cost, naturally keeping the robot away from walls.

**The algorithm** is a distance transform: for every free cell, compute the distance to the nearest obstacle cell and assign `cost = f(distance)`. The naive O(N²) approach tests every free cell against every obstacle cell. The GPU version parallelizes the per-cell distance computation — each work item owns one cell.

```cl
__kernel void inflate(__global const uchar* obstacles,
                      __global uchar* costmap,
                      int width, int height,
                      int radius, float decay) {
    int x = get_global_id(0);
    int y = get_global_id(1);
    float min_dist = (float)radius + 1.0f;
    for (int dy = -radius; dy <= radius; dy++) {
        for (int dx = -radius; dx <= radius; dx++) {
            int nx = x + dx, ny = y + dy;
            if (nx >= 0 && ny >= 0 && nx < width && ny < height)
                if (obstacles[ny * width + nx] > 0)
                    min_dist = fmin(min_dist, sqrt((float)(dx*dx + dy*dy)));
        }
    }
    costmap[y * width + x] = (min_dist <= radius)
        ? (uchar)(255.0f * exp(-decay * min_dist)) : 0;
}
```

**Memory access pattern**: the inner loop reads `obstacles` at offsets scattered across a `2×radius` window. Run the kernel and look at the GPU memory bandwidth utilization — then ask why a neighborhood read pattern might be inefficient.

### Mini-challenge
Write a second version of the kernel that loads a tile of the obstacle map into `__local` memory before the inner loop. Profile naive vs tiled — at what tile size does the local-memory version peak? Does the crossover radius (below which it doesn't help) match your expectation? See [Toolbox: Local Memory](../../99_Toolbox/LocalMemory/LocalMemory.md) if you need the tiling pattern.

---

## C3_Perception_Node — Flagship Project

**Goal**: Build a ROS 2 node that receives a Lidar point cloud, filters it on GPU (ground removal + intensity threshold), extracts per-cluster features, and publishes the result — end-to-end latency < 5 ms for 100k points.

### Build & run
```bash
source /opt/ros/humble/setup.bash
cd C3_Perception_Node
cmake -B build
cmake --build build

# Terminal 1 — run the node
./build/perception_node --topic /points --gpu NVIDIA

# Terminal 2 — replay a bag or publish synthetic data
ros2 run C3_Perception_Node point_cloud_publisher --points 100000

# Or play a recorded bag:
ros2 bag play ../../../assets/lidar_sample.bag
```

### Verify
- Node publishes `/filtered_points` and `/cluster_features` topics
- `ros2 topic hz /filtered_points` reports ~200 Hz (5 ms period)
- Console prints per-message breakdown:
  ```
  [RECV ] PointCloud2 deserialized:  0.4 ms
  [GPU  ] Upload (100k pts):         0.8 ms
  [GPU  ] Filter kernel:             1.1 ms
  [GPU  ] Feature extraction:        1.6 ms
  [GPU  ] Download results:          0.5 ms
  [SEND ] Publish filtered cloud:    0.3 ms
  Total:                             4.7 ms  ← must be < 5 ms to pass
  ```

### Core Concept: The Serialization Problem

A `sensor_msgs/PointCloud2` message with 100k points (XYZ + intensity, float32) is 1.6 MB. The standard ROS 2 subscriber deserializes this from shared memory into a `PointCloud2` struct, which you then copy to a `cl::Buffer`. That's two copies before the GPU sees a single point.

**Loaned Messages** (ROS 2 Humble+) eliminate the first copy. The middleware loans you a pre-allocated message buffer directly in the publisher's shared memory region. If the publisher and subscriber are in the same process (or use a zero-copy transport like Iceoryx), no serialization occurs at all. The underlying GPU-side principle is the same as [Toolbox: Zero-Copy](../../99_Toolbox/ZeroCopy/ZeroCopy.md) — pinned or shared memory avoids the pageable-copy overhead on every transfer.

```cpp
// Standard (two copies: shm → ROS msg → cl::Buffer)
sub_ = create_subscription<PointCloud2>("points", 10,
    [this](PointCloud2::SharedPtr msg) {
        queue_.enqueueWriteBuffer(buf_, CL_FALSE, 0,
            msg->data.size(), msg->data.data());
    });

// Loaned (one copy: shm → cl::Buffer directly)
sub_ = create_subscription<PointCloud2>("points", rclcpp::QoS(10),
    [this](std::unique_ptr<PointCloud2> msg) {  // loaned unique_ptr
        queue_.enqueueWriteBuffer(buf_, CL_FALSE, 0,
            msg->data.size(), msg->data.data());
        // msg released back to middleware — no deallocation
    });
```

To reach < 5 ms you will need to use the loaned message path. The standard path typically adds 0.8–1.5 ms on a 1.6 MB message.

### Filtering Kernel: The Problem

The filter must produce a dense output array — feature extraction expects contiguous points with no gaps. A naive approach that marks rejected points in-place with a sentinel leaves holes; a second pass is needed to close them. Think about how many kernel launches and buffer round-trips that requires, and whether any of them can be merged or overlapped.

### Challenge: Real-Time Guarantee
Make the node miss-proof: if a new message arrives before the previous kernel finishes, the node must not block the subscriber thread. The Troubleshooting section describes the symptom when it does block. Design a strategy that decouples message arrival from kernel dispatch, measure the event callback latency on your platform, and verify with `ros2 topic hz` that no messages are dropped under load.

### Mini-challenge
Replace the point cloud data layout from Array-of-Structs (AoS: `XYZIXYZIXYZ...`) to Structure-of-Arrays (SoA: `XXX...YYY...ZZZ...III...`). Profile the filter kernel before and after. Which layout wins, and why? (Hint: memory coalescing — consecutive work items reading consecutive memory.)

---

## Performance Gate

This track is complete when:

| Project | Metric | Target |
|:--------|:-------|:-------|
| Accelerated Perception Node (C3) | End-to-end latency | < 5 ms @ 100k points |

**Measure with `cl::Event` profiling** on every GPU stage. The deserialization and publish stages are CPU-bound — measure them with `std::chrono::steady_clock`. The gate applies to the sum of all stages.

**If you are over 5 ms**: break down the per-stage times from the console output and identify which stage dominates. Then consult the [Optimization Toolbox](../../99_Toolbox/Toolbox.md) for the technique that matches your bottleneck.

---

## Troubleshooting

- **`source /opt/ros/humble/setup.bash` must be run before CMake**: Without it, `find_package(rclcpp REQUIRED)` fails silently. Add it to your `.bashrc` or use a ROS 2 dev container.
- **Loaned Messages not available**: Requires `rmw_fastrtps_cpp` or Iceoryx middleware. Check: `echo $RMW_IMPLEMENTATION`. Set `export RMW_IMPLEMENTATION=rmw_fastrtps_cpp` if using the default.
- **OpenCL context fails inside node constructor**: The ROS 2 executor runs on a thread that may not have the GPU context set. Initialize context with an explicit platform/device selection via the `GPU` env var — do not rely on thread-local state.
- **`ros2 topic hz` shows half the expected rate**: The node is blocking on `clFinish()` inside the callback. Replace with non-blocking enqueue + event callback to release the subscriber thread immediately.
- **PointCloud2 data size unexpected**: ROS 2 `PointCloud2` carries a `point_step` field (bytes per point). Always compute buffer size as `msg->width * msg->height * msg->point_step`, not a hard-coded value.
- **Wrong GPU**: `GPU=NVIDIA ./build/perception_node`, `GPU=AMD ./build/perception_node`, `GPU=INTEL ./build/perception_node`.

---

## What's Next

[Track A: Multimedia](../A_Multimedia/Multimedia.md) — zero-copy video pipelines and edge AI inference.

[Track B: Graphics/HPC](../B_Graphics_HPC/GraphicsHPC.md) — ray tracing, CLBlast, and BVH acceleration structures.

[Optimization Toolbox](../../99_Toolbox/Toolbox.md) — Local Memory, Async Pipelines, Zero-Copy, Multi-GPU.

[Module 4 Add-ons](../../04_Addons/Addons.md) — 3D Voxel Mapping grand finale (combines B ray casting with C Lidar data).
