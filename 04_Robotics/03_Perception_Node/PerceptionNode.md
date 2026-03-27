# C.3 — Perception Node: Lidar Filtering End-to-End

**Goal**: Build a ROS 2 node that receives a Lidar point cloud, filters it on GPU (ground removal + intensity threshold), extracts per-cluster features, and publishes the result — end-to-end latency < 5 ms for 100k points.

## Prerequisites (delta from module index)

- ROS 2 Jazzy sourced and `export RMW_IMPLEMENTATION=rmw_fastrtps_cpp` set in both terminals.

## Build & Run

Both terminals must source ROS 2 and export the RMW before running.

```bash
cd 03_Perception_Node
source /opt/ros/jazzy/setup.bash
export RMW_IMPLEMENTATION=rmw_fastrtps_cpp
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

**Terminal 1 — perception node:**

```bash
GPU=NVIDIA ./build/perception_node --ros-args \
    -p topic:=/points \
    -p ground_z:=0.2 \
    -p min_intensity:=10.0
```

**Terminal 2 — synthetic publisher:**

```bash
./build/point_cloud_publisher --topic /points --hz 200 --points 100000
```

Node parameters (via `declare_parameter`): `topic`, `ground_z`, `min_intensity`, `max_points` (default 200000), `use_double_buffer` (default false — see Challenge below).

Publisher flags (CLI11): `--topic`, `--hz`, `--points`, `--scene` (`grid` for throughput or `mixed` for visual validation).

## Verify

Node publishes `/filtered_points` and `/cluster_features`.

Rate check: `ros2 topic hz /filtered_points` — expect ~200 Hz.

Console prints per-message stage breakdown:

```
[RECV ] PointCloud2 deserialized:  0.400 ms
[GPU  ] Upload (100k pts):         0.800 ms
[GPU  ] Filter kernel:             1.100 ms
[GPU  ] Feature extraction:        1.600 ms
[GPU  ] Download results:          0.500 ms
[SEND ] Publish filtered cloud:    0.300 ms
Total:                             4.700 ms  <- must be < 5 ms to pass
```

**Performance gate**: total end-to-end < 5 ms at 100k points †.

## Inspecting Parameters

All tunable parameters are declared via `declare_parameter()` and inspectable at runtime without recompiling.

```bash
# List all declared parameters
ros2 param list /perception_node

# Show type, description, and constraints for a single parameter
ros2 param describe /perception_node <param>

# Read a parameter value
ros2 param get /perception_node <param>

# Set a parameter value at runtime
ros2 param set /perception_node <param> <value>
```

## Key Concepts

### The Serialization Problem (Loaned Messages)

A `sensor_msgs/PointCloud2` with 100k points (XYZ + intensity, float32) is 1.6 MB. The standard subscriber deserializes this into a `PointCloud2` struct, then you copy it to a `cl::Buffer` — two copies before the GPU sees a single point.

**Loaned Messages** eliminate the first copy. The middleware loans a pre-allocated buffer directly in the publisher's shared memory region:

```cpp
// Loaned: one copy (shm -> cl::Buffer directly)
sub_ = create_subscription<PointCloud2>("points", rclcpp::QoS(10),
    [this](std::unique_ptr<PointCloud2> msg) {  // loaned unique_ptr
        CL_CHECK(queue_.enqueueWriteBuffer(buf_, CL_FALSE, 0,
            msg->data.size(), msg->data.data(), nullptr, &upload_event_));
        // msg released back to middleware automatically
    });
```

To reach < 5 ms you will likely need the loaned path. The standard path adds 0.8–1.5 ms on a 1.6 MB message. Same principle as [Toolbox: Zero-Copy](../../05_Toolbox/15_Zero_Copy/ZeroCopy.md).

### Filter Kernel Design (Dense Output)

The filter must produce a dense output array — feature extraction expects contiguous points with no gaps. The filter predicate combines two conditions in a single dispatch to avoid two separate kernel launches (~0.1–0.3 ms each):

```cl
bool keep = (pt.z >= ground_z) && (pt.intensity >= min_intensity);
```

## Challenge: Double-Buffer Real-Time Guarantee

**Goal**: Make the node miss-proof. While the GPU processes buffer N, the CPU fills buffer N+1. Verify zero `WARN: double-buffer contention` entries over a 10-second run at 200 Hz.

The base C3 node calls `queue_.finish()` inside the subscriber callback. At 200 Hz (5 ms inter-message interval), any GPU stage exceeding 5 ms stalls the executor and drops the next message. The double-buffer pattern decouples arrival from dispatch.

Pre-allocate two identical buffer sets at `on_configure()`. `std::atomic<int> active_buf_{0}` tracks which index the GPU currently owns. The subscriber callback reads the current index, issues all enqueue calls as non-blocking, sets a `cl::Event` callback that atomically swaps the index on `CL_COMPLETE`, then returns immediately.

> **Thread-safety**: OpenCL fires event callbacks from an internal driver thread. Only `std::atomic` operations are safe inside those callbacks — no `rclcpp` logging, publishers, or heap allocation.

**Run the challenge:**

```bash
# Terminal 1
GPU=NVIDIA ./build/perception_node --ros-args \
    -p topic:=/points -p ground_z:=0.1 -p min_intensity:=50.0 \
    -p max_points:=200000 -p use_double_buffer:=true

# Terminal 2
./build/point_cloud_publisher --topic /points --hz 200 --points 100000
```

**Pass condition**: zero `WARN: double-buffer contention` entries in 10 seconds at 200 Hz, 100k points.

**MANUAL — RViz**:
```bash
ros2 run rviz2 rviz2
```
Fixed Frame `lidar_link`; PointCloud2 on `/filtered_points`; with `--scene mixed` must show only three valid clusters, no ground or low-intensity points, no frame drops over 10 seconds.

## Mini-Challenge

Replace AoS (`XYZIXYZIXYZ...`) with SoA (`XXX...YYY...ZZZ...III...`) in the upload path. Profile the filter kernel before and after. Which layout wins, and why? See [Toolbox: Memory Coalescing](../../05_Toolbox/02_Coalesced_Access/CoalescedAccess.md).

## Troubleshooting

- **Loaned messages not available**: requires `rmw_fastrtps_cpp`. The node falls back to copy-based transport automatically with a warning.
- **`ros2 topic hz` shows half the expected rate**: node blocking on `clFinish()` inside callback. Use non-blocking enqueue + event callback.
- **Double-buffer contention at nominal rate**: GPU pipeline > 5 ms. Break down per-stage times to find the bottleneck.
- **PointCloud2 size unexpected**: compute as `msg->width * msg->height * msg->point_step`. Never hard-code 16 bytes.

---

[Path C: Robotics & ROS 2](../RoboticsROS2.md)
