# C.1 — Node Acceleration: OpenCL Inside a LifecycleNode

**Goal**: Wire an OpenCL context into a `rclcpp_lifecycle::LifecycleNode`. OpenCL resources are created in `on_configure()` once and destroyed in `on_cleanup()` by RAII. Prove context init is paid exactly once, not per message.

## Prerequisites (delta from module index)

- ROS 2 Jazzy must be sourced: `source /opt/ros/jazzy/setup.bash`.
- New to lifecycle nodes? See the [ROS 2 Jazzy Lifecycle Tutorial](https://docs.ros.org/en/jazzy/Tutorials/Intermediate/Managed-Nodes.html) first.

## Build & Run

```bash
cd C1_Node_Acceleration
source /opt/ros/jazzy/setup.bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/node_acceleration
# GPU selection: GPU=NVIDIA ./build/node_acceleration
# Override parameters at launch:
# ./build/node_acceleration --ros-args -p iterations:=20 -p buffer_size:=2097152
```

Parameters (set via `declare_parameter`, overridable at launch):

| Parameter | Type | Default | Description |
| :-------- | :--- | :------ | :---------- |
| `iterations` | int | `10` | Number of callback cycles to run |
| `buffer_size` | int | `1048576` | Floats per message |

## Verify

Console shows context init exactly once in `on_configure`. Per-callback dispatch time stays flat:

```
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

## Key Concepts

### LifecycleNode State Machine Maps to OpenCL Resource Lifetime

`rclcpp_lifecycle::LifecycleNode` state transitions map directly onto resource lifetime:

```cpp
class AccelNode : public rclcpp_lifecycle::LifecycleNode {
public:
    AccelNode() : LifecycleNode("node_acceleration") {
        declare_parameter("iterations", 10);
        declare_parameter("buffer_size", 1048576);
    }

    rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn
    on_configure(const rclcpp_lifecycle::State &) override {
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

- **`source /opt/ros/jazzy/setup.bash` must run before CMake**: without it, `find_package(rclcpp REQUIRED)` fails.
- **Wrong GPU**: `GPU=NVIDIA ./build/node_acceleration`, `GPU=AMD ./build/node_acceleration`.

---

[Path C: Robotics & ROS 2](../RoboticsROS2.md)
