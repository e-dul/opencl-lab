# Path C: Robotics & ROS 2

Accelerate a real ROS 2 perception pipeline without breaking the node contract. You start by wiring an OpenCL context into a LifecycleNode's state machine, apply it to a real AMR navigation problem (costmap inflation), then eliminate the serialization overhead that makes Lidar processing miss real-time deadlines.

## Prerequisites

- Base requirements: see [main README](../README.md) (OpenCL, CMake 3.18+).
- ROS 2 Jazzy: see [SETUP.md](SETUP.md) for APT repo, sourcing, and RMW configuration. Required for all sub-modules.
- `export RMW_IMPLEMENTATION=rmw_fastrtps_cpp` for C3 (loaned messages).
- Assets: `assets/warehouse.pgm` (512×512 occupancy grid, required for C2).
- **Assumption**: you know ROS 2 basics — nodes, pub/sub, topics, `rclcpp`. This track focuses on GPU acceleration inside that model.

## Contents

| Sub-module | Goal | Doc |
| :--------- | :--- | :-- |
| C1 — Node Acceleration | OpenCL context in a LifecycleNode (init once in on_configure) | [NodeAcceleration.md](C1_Node_Acceleration/NodeAcceleration.md) |
| C2 — Costmap Inflation | GPU distance transform for obstacle padding (10 Hz map rate) | [CostmapInflation.md](C2_Costmap_Inflation/CostmapInflation.md) |
| C3 — Perception Node | Flagship: Lidar filter + feature extraction < 5 ms @ 100k pts | [PerceptionNode.md](C3_Perception_Node/PerceptionNode.md) |

## Performance Gates

| Sub-module | Metric | Target |
| :--------- | :----- | :----- |
| C1 Node Acceleration | Per-callback dispatch time | ≤ 0.5 ms flat; context init logged once only † |
| C2 Costmap (naive) | GPU distance transform | < 10 ms @ 512×512 † |
| C2 Costmap (tiled) | GPU distance transform | < 5 ms @ 512×512 † |
| C3 Perception Node | End-to-end latency | < 5 ms @ 100k points (all stages summed) † |
| C3 Double-Buffer | Contention-free rate | Zero contention entries @ 200 Hz, 100k pts, 10 s † |

† Gates marked with † apply a hardware waiver for CPU-fallback and integrated GPU devices.

**Measure with `cl::Event` profiling** on every GPU stage; CPU stages use `std::chrono::steady_clock`.

## Known Issues

- **C2 LDS tiling yields ~1.0x on RTX 4060 / Radeon 680M**: dense 2D neighbourhood scans are not LDS-bandwidth-bound on these architectures. Hardware waiver † applies to the C2 tiled ≥ 1.5× speedup gate. The correct optimisation for large radii is a separable 1D distance transform (Meijster/Saito).

## Troubleshooting

- **`source /opt/ros/jazzy/setup.bash` must run before CMake**: without it, `find_package(rclcpp REQUIRED)` fails.
- **Loaned messages not available**: requires `rmw_fastrtps_cpp`. Set `export RMW_IMPLEMENTATION=rmw_fastrtps_cpp`.
- **`ros2 topic hz` shows half the expected rate**: node is blocking on `clFinish()` inside callback. Use non-blocking enqueue + event callback.
- **Wrong GPU**: `GPU=NVIDIA ./build/accel_node`, `GPU=AMD ./build/costmap_node`.

## What's Next

[Track A: Multimedia & AI](../02_Multimedia/Multimedia.md) — zero-copy video pipelines and edge AI.

[Track B: Graphics & HPC](../03_GraphicsHPC/GraphicsHPC.md) — ray tracing, BVH acceleration.

[Optimization Toolbox](../05_Toolbox/Toolbox.md) — Local Memory, Async Pipelines, Zero-Copy.
