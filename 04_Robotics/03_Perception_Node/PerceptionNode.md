# C.3 — Perception Node: Lidar Filtering End-to-End

**Goal**: Build a ROS 2 node that receives a Lidar point cloud, filters it on GPU (ground removal + intensity threshold), extracts per-cluster features, and publishes the result — end-to-end latency < 5 ms for 100k points.

## Prerequisites (delta from module index)

- ROS 2 Jazzy sourced and `export RMW_IMPLEMENTATION=rmw_fastrtps_cpp` set in both terminals.

## Build & Run

`rmw_fastrtps_cpp` is required for loaned messages (zero-copy path). Add to `~/.bashrc`:

```bash
source /opt/ros/jazzy/setup.bash
export RMW_IMPLEMENTATION=rmw_fastrtps_cpp
```

```bash
source /opt/ros/jazzy/setup.bash
export RMW_IMPLEMENTATION=rmw_fastrtps_cpp
cd 04_Robotics/03_Perception_Node
colcon build
source install/setup.bash
ros2 launch perception_node perception_node.launch.py
```

GPU selection and parameter overrides:

```bash
# Visual validation scene (three clusters, ground, noise)
ros2 launch perception_node perception_node.launch.py \
    gpu:=NVIDIA \
    scene:=mixed \
    ground_z:=0.1 \
    min_intensity:=50.0

# Without RViz
ros2 launch perception_node perception_node.launch.py use_rviz:=false

# Keep node INACTIVE after configure (manual lifecycle control):
ros2 launch perception_node perception_node.launch.py auto_activate:=false
# Then in a second terminal:
ros2 lifecycle set /perception_node activate
```

Launch arguments:

| Argument | Default | Description |
| :------- | :------ | :---------- |
| `topic` | `/points` | PointCloud2 topic |
| `ground_z` | `0.2` | Ground-plane Z threshold (metres) |
| `min_intensity` | `10.0` | Minimum intensity threshold |
| `max_points` | `100000` | Pre-allocated buffer size (points) |
| `use_double_buffer` | `false` | Enable non-blocking double-buffer pipeline |
| `scene` | `grid` | Publisher scene: `grid` (throughput) or `mixed` (validation) |
| `hz` | `200` | Publisher rate in Hz |
| `points` | `100000` | Points per publisher message |
| `auto_activate` | `true` | Self-activate after configure |
| `gpu` | `` | GPU vendor substring (e.g. `NVIDIA`, `AMD`) |
| `use_rviz` | `true` | Launch RViz2 for visualisation |

## Verify

Node publishes `/filtered_points` and `/cluster_features`.

Rate check: `ros2 topic hz /filtered_points` — expect ~200 Hz.

Console on startup (in `on_configure`):

```text
[INIT] Context + kernel compile: 312.000 ms
[DIAG] GPU timing and config published at 1 Hz — monitor with:
       ros2 topic echo /diagnostics
```

### Monitoring GPU Performance

Per-message timing is published as structured diagnostics at 1 Hz instead of per-line console output:

```bash
ros2 topic echo /diagnostics
```

Each tick reports two status entries for `perception_node`:

| Key | Description |
| :-- | :---------- |
| `avg_kernel_ms` | Rolling average total pipeline time (100-message window) |
| `min_kernel_ms` / `max_kernel_ms` | Window min and max |
| `messages_processed` | Total messages received since startup |
| `points_in_avg` / `points_out_avg` | Average points in and out of the filter |
| `sample_count` | Samples in the current window (up to 100) |

Status levels: **OK** ≤ 10 ms · **WARN** ≤ 25 ms · **ERROR** > 25 ms.

For a visual dashboard:

```bash
ros2 run rqt_robot_monitor rqt_robot_monitor
```

**Performance gate**: total end-to-end < 10 ms avg at 100k points (WARN threshold) †.

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

A `sensor_msgs/PointCloud2` with 100k points (XYZ + intensity, float32) is 1.6 MB. The standard subscriber copies this into a `PointCloud2` struct, then you copy it to a `cl::Buffer` — two copies before the GPU sees a single point.

**Loaned Messages** eliminate the first copy using middleware-managed shared memory. The implementation is in `on_activate()` in `main.cpp` — see the `// WHY loaned` block comment there for the specific `create_subscription` signature and why `unique_ptr` ownership enables the zero-copy path.

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
ros2 launch perception_node perception_node.launch.py \
    gpu:=NVIDIA \
    ground_z:=0.1 \
    min_intensity:=50.0 \
    max_points:=200000 \
    use_double_buffer:=true \
    hz:=200 \
    points:=100000 \
    use_rviz:=false
```

**Pass condition**: zero `WARN: double-buffer contention` entries in 10 seconds at 200 Hz, 100k points.

**MANUAL — RViz** (pre-configured with the launch file):

```bash
ros2 launch perception_node perception_node.launch.py use_rviz:=true scene:=mixed
```

Fixed Frame `lidar_link`; PointCloud2 on `/filtered_points`; with `scene:=mixed` must show only three valid clusters, no ground or low-intensity points, no frame drops over 10 seconds.

## Mini-Challenge

Replace AoS (`XYZIXYZIXYZ...`) with SoA (`XXX...YYY...ZZZ...III...`) in the upload path. Profile the filter kernel before and after. Which layout wins, and why? See [Toolbox: Memory Coalescing](../../05_Toolbox/02_Coalesced_Access/CoalescedAccess.md).

## Recording & Replay

Topic: `/points` · Bag name: `perception_bag`

```bash
# Record
ros2 bag record /points -o perception_bag

# Replay (suppresses PointCloudPublisher; ros2 bag play starts automatically)
ros2 launch perception_node perception_node.launch.py bag:=perception_bag
```

See [ROS 2 Setup §5 — Recording & Replay](../SETUP.md#5-recording--replay) for the full record → inspect → replay workflow and QoS override instructions.

## Troubleshooting

- **`source /opt/ros/jazzy/setup.bash` must run before `colcon build`**: without it, `find_package(rclcpp REQUIRED)` fails.
- **`source install/setup.bash` must run before `ros2 launch`**: without it, the package is not on the ROS 2 package path.
- **Wrong GPU**: pass `gpu:=NVIDIA` or `gpu:=AMD` as a launch argument.
- **Loaned messages not available**: requires `rmw_fastrtps_cpp`. The node falls back to copy-based transport automatically with a warning.
- **`ros2 topic hz` shows half the expected rate**: node blocking on `clFinish()` inside callback. Use non-blocking enqueue + event callback.
- **Double-buffer contention at nominal rate**: GPU pipeline > 5 ms. Break down per-stage times to find the bottleneck.
- **PointCloud2 size unexpected**: compute as `msg->width * msg->height * msg->point_step`. Never hard-code 16 bytes.

## Common Gotchas

### Host/Device Struct Layout

When sharing a C++ struct between host code and an OpenCL kernel, the device imposes its own
alignment rules — which may differ from the host compiler's layout. Silent corruption results
when the two disagree.

Two mandatory defences:

#### 1. Explicit padding fields

```cpp
struct PointCloud {
    cl_float x, y, z;
    cl_int   pad;     // satisfies 16-byte device alignment; never access on host
};
```

#### 2. Compile-time size assertion

```cpp
static_assert(sizeof(PointCloud) == 16,
    "PointCloud ABI mismatch — check device alignment");
```

The `static_assert` catches ABI drift at compile time, not at runtime with silent data
corruption. Add one for every struct that crosses the host/device boundary. If the assertion
fires after a refactor, fix the struct layout before touching any kernel code.

---

[Path C: Robotics & ROS 2](../RoboticsROS2.md)
