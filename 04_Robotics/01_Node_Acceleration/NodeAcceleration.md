# C.1 — Node Acceleration: OpenCL Inside a LifecycleNode

**Goal**: Wire an OpenCL context into a `rclcpp_lifecycle::LifecycleNode`. OpenCL resources are created in `on_configure()` once and destroyed in `on_cleanup()` by RAII. Prove context init is paid exactly once, not per message.

## Prerequisites (delta from module index)

- ROS 2 Jazzy must be sourced: `source /opt/ros/jazzy/setup.bash`.
- New to lifecycle nodes? See the [ROS 2 Jazzy Lifecycle Tutorial](https://docs.ros.org/en/jazzy/Tutorials/Intermediate/Managed-Nodes.html) first.

## Build & Run

```bash
source /opt/ros/jazzy/setup.bash
cd 04_Robotics/01_Node_Acceleration
colcon build
source install/setup.bash
ros2 launch node_acceleration node_acceleration.launch.py
```

GPU selection and parameter overrides:

```bash
# Select GPU vendor
ros2 launch node_acceleration node_acceleration.launch.py gpu:=NVIDIA

# Override parameters
ros2 launch node_acceleration node_acceleration.launch.py \
    iterations:=20 buffer_size:=2097152

# Keep node INACTIVE after configure (manual lifecycle control):
ros2 launch node_acceleration node_acceleration.launch.py auto_activate:=false
# Then in a second terminal:
ros2 lifecycle set /accel_node activate
```

Launch arguments:

| Argument | Default | Description |
| :------- | :------ | :---------- |
| `iterations` | `10` | Number of callback cycles to run |
| `buffer_size` | `1048576` | Floats per message |
| `auto_activate` | `true` | Self-activate after configure |
| `gpu` | `` | GPU vendor substring (e.g. `NVIDIA`, `AMD`) |

## Verify

Console shows context init exactly once in `on_configure`. Per-callback dispatch time stays flat:

```text
[on_configure] OpenCL context initialized: NVIDIA GeForce RTX 3080 (2.1 ms)
[on_activate ] Subscribed to /raw_floats. Publishing to /processed_floats.
[callback  1 ] kernel dispatched in 0.310 ms
[callback  2 ] kernel dispatched in 0.290 ms
[callback  3 ] kernel dispatched in 0.330 ms
...
[on_cleanup  ] OpenCL resources released.
```

Context init appears once. Dispatch times are flat across all callbacks. The node self-manages its lifecycle — no manual `ros2 lifecycle set` commands needed.

**Performance gate**: per-callback dispatch time ≤ 0.5 ms flat. Hardware waiver applies for CPU-fallback and integrated GPU devices.

> **20 Hz vs 200 Hz**: the demo `SyntheticPublisher` runs at 20 Hz (not 200 Hz as specified in the design gate). A flat dispatch-time curve across all callbacks is the meaningful verification signal — raw throughput is not the goal. The 200 Hz gate applies when wiring the node into a real sensor pipeline.

## Inspecting Parameters

In a second terminal (with ROS 2 sourced), while the node is running:

All tunable parameters are declared via `declare_parameter()` and inspectable at runtime without recompiling.

```bash
# List all declared parameters
ros2 param list /accel_node

# Show type, description, and constraints for a single parameter
ros2 param describe /accel_node <param>

# Read a parameter value
ros2 param get /accel_node <param>

# Set a parameter value at runtime
ros2 param set /accel_node <param> <value>
```

## Key Concepts

### LifecycleNode State Machine Maps to OpenCL Resource Lifetime

`rclcpp_lifecycle::LifecycleNode` state transitions map directly onto resource lifetime:

```cpp
class AccelNode : public rclcpp_lifecycle::LifecycleNode {
public:
    AccelNode() : LifecycleNode("node_acceleration") {}

    rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn
    on_configure(const rclcpp_lifecycle::State &) override {
        // WHY declare_parameter in on_configure (not constructor):
        // idiomatic LifecycleNode pattern — parameters are declared once the
        // node transitions to the configured state, matching the actual code.
        declare_parameter("iterations", 10);
        declare_parameter("buffer_size", 1048576);
        int buf_size = get_parameter("buffer_size").as_int();
        // OpenCL init happens once, here — never inside a callback.
        ctx_    = create_context();
        queue_  = cl::CommandQueue(ctx_, device_, CL_QUEUE_PROFILING_ENABLE);
        kernel_ = build_kernel(ctx_, "passthrough.cl", "passthrough");
        buf_    = cl::Buffer(ctx_, CL_MEM_READ_WRITE,
                             static_cast<size_t>(buf_size) * sizeof(float));
        return CallbackReturn::SUCCESS;
    }

private:
    cl::Context      ctx_;
    cl::CommandQueue queue_;
    cl::Kernel       kernel_;
    cl::Buffer       buf_;
};
```

`on_cleanup()` is implicit: RAII destroys `ctx_`, `queue_`, `kernel_`, and `buf_` when the node transitions to `unconfigured`. No manual `clRelease*` calls needed.

### The Wrong Pattern

Creating a context inside a callback:

```cpp
void on_message(const std_msgs::msg::Float32MultiArray::SharedPtr msg) {
    cl::Context ctx(device);  // 2 ms overhead — every callback!
}
```

At 200 Hz (5 ms budget), that one line consumes 40% of your entire latency budget per message.

### Why Intra-Process?

Both `AccelNode` and `SyntheticPublisher` are added to the same `SingleThreadedExecutor`
with `use_intra_process_comms(true)`. When both nodes opt in, ROS 2 routes the
`Float32MultiArray` as a `shared_ptr` directly to the subscriber callback — no DDS
serialization, no copy. This makes the benchmark representative of a real pipeline: the
measured dispatch time is pure kernel overhead, not transport overhead.

If either node opts out, the full DDS serialization path activates silently. Both must
opt in for the optimization to take effect.

### The Passthrough Kernel

The `passthrough` kernel copies `src` → `dst` element-by-element. It is intentionally
non-trivial: an empty or no-op kernel body risks being elided by the driver optimizer,
producing artificially low dispatch times that do not reflect real dispatch overhead.
A minimal memory-bound copy forces the driver to actually schedule and dispatch a GPU
workgroup, giving a reliable baseline for subsequent modules that add real compute.

## Mini-Challenge

Run the node with `buffer_size:=524288`, `buffer_size:=1048576`, and `buffer_size:=4194304`. Record the per-callback dispatch times from `cl::Event` profiling for each size. At what buffer size does dispatch latency become non-trivial relative to the 0.5 ms gate? Tabulate your results.

## Troubleshooting

- **`source /opt/ros/jazzy/setup.bash` must run before `colcon build`**: without it, `find_package(rclcpp REQUIRED)` fails.
- **`source install/setup.bash` must run before `ros2 launch`**: without it, the package is not on the ROS 2 package path.
- **Wrong GPU**: pass `gpu:=NVIDIA` or `gpu:=AMD` as a launch argument.

---

[Path C: Robotics & ROS 2](../RoboticsROS2.md)
