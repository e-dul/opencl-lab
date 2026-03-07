# Module 2: Real-World Integration

Apply your GPU pipeline to problems that ship in production: video AI, ray tracing, and autonomous robots. Pick one track and follow it to a working flagship project with a measurable performance gate.

## Prerequisites
See [main README](../README.md) for base requirements (OpenCL, CMake, Docker setup).

**Additional (track-specific):**
- Track A: OpenCV 4.5+ (`sudo apt install libopencv-dev`)
- Track B: None beyond base stack
- Track C: ROS 2 Humble or later + `rclcpp`, `sensor_msgs`

## Contents
```
A_Multimedia/     Path A — OpenCV interop, YUV pipeline, AI Smart Webcam
B_Graphics_HPC/   Path B — CLBlast, ray tracing, Stackless BVH
C_Robotics_ROS2/  Path C — ROS 2 node acceleration, costmaps, Lidar perception
```

---

## Choosing Your Track

| Track | Core Problem | Flagship Project | Performance Gate |
|:------|:-------------|:-----------------|:-----------------|
| **[A: Multimedia](A_Multimedia/Multimedia.md)** | Memory bandwidth, zero-copy interop | AI Smart Webcam (Bokeh blur) | < 33 ms/frame @ 1080p |
| **[B: Graphics/HPC](B_Graphics_HPC/GraphicsHPC.md)** | Compute throughput, spatial acceleration | Advanced Ray Tracer (Stackless BVH) | 60 FPS @ 100k triangles, 1080p |
| **[C: Robotics/ROS 2](C_Robotics_ROS2/RoboticsROS2.md)** | Latency, middleware integration | Accelerated Perception Node | < 5 ms @ 100k-point cloud |

If unsure: start with Track A. OpenCV interop is the most universally applicable skill and the zero-copy problem appears in every other domain.

---

## Troubleshooting

- **Which track?** Track A for cameras/video. Track B for simulation, rendering, or scientific computing. Track C for robots.
- **Wrong GPU selected**: `GPU=NVIDIA ./build/binary`, `GPU=AMD ./build/binary`, `GPU=INTEL ./build/binary`. See [Module 1 Troubleshooting](../01_Host_API/HostAPI.md#troubleshooting).
- **`cmake -B build` fails before any code**: Run `clinfo -l`. If no platforms appear, the issue is your driver/runtime, not your code.
- **Track A: `find_package(OpenCV REQUIRED)` fails**: `sudo apt install libopencv-dev`. Verify: `pkg-config --modversion opencv4`.
- **Track C: ROS 2 packages not found**: `source /opt/ros/humble/setup.bash` before running CMake.

---

## Performance Gate

Gate is met when the flagship project passes with `cl::Event` profiling (wall-clock times do not count):

| Track | Project | Metric | Target |
|:------|:--------|:-------|:-------|
| A | AI Smart Webcam — Bokeh | Frame time | < 33 ms @ 1080p |
| A | AI Smart Webcam — Privacy ROI | Frame time | < 20 ms @ 1080p |
| B | Advanced Ray Tracer | Render time | 60 FPS @ 100k triangles, 1080p |
| C | Accelerated Perception Node | Pipeline latency | < 5 ms @ 100k points |

---

## What's Next

Hit a bottleneck before reaching the gate? See [Optimization Toolbox](../99_Toolbox/Toolbox.md) for zero-copy, memory coalescing, work-group sizing, thread divergence, local memory, and async pipeline techniques.

After passing the gate: [Module 4 Add-ons](../04_Addons/Addons.md) — vkFFT audio mini-project, FFmpeg hardware decoding pipeline, and the 3D Voxel Mapping grand finale.
