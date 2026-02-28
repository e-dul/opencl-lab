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

## Track A: Multimedia & AI
**[Full track guide](A_Multimedia/Multimedia.md)**

The central constraint: a 4K frame at 60 FPS gives you ~16 ms. A naive `cv::Mat` copy to the GPU can consume 8 ms of that budget before processing one pixel. You will feel this, measure it, then eliminate it.

Steps: `A1_OpenCV_Interop` → `A2_YUV_Pipeline` → `A3_AI_Inference` → `A4_Smart_Webcam`

### Build & run
```bash
cd A_Multimedia/A1_OpenCV_Interop
cmake -B build
cmake --build build
./build/opencv_interop_demo --input ../../../assets/sample.bmp
```

### Verify
Console prints two transfer times — `cv::Mat copy` vs `UMat zero-copy`. On integrated GPU the zero-copy path is measurably faster. On discrete GPU the gap appears at 1080p+.

---

## Track B: Graphics & HPC
**[Full track guide](B_Graphics_HPC/GraphicsHPC.md)**

The central constraint: naive ray tracing is O(N × R) — every triangle tested against every ray. A 100k-triangle scene at 60 FPS is impossible without a spatial acceleration structure. You'll build the naive version first, measure where it breaks, then implement BVH.

Steps: `B1_CLBlast_MatMul` → `B2_Ray_Tracer_Basic` → `B3_Ray_Tracer_BVH` → `B4_Device_Enqueue` (advanced)

### Build & run
```bash
cd B_Graphics_HPC/B1_CLBlast_MatMul
cmake -B build
cmake --build build
./build/matmul_demo --size 1024
```

### Verify
Console prints CLBlast GEMM time vs naive kernel time for a 1024×1024 matrix. CLBlast should win by 2–10× depending on hardware.

---

## Track C: Robotics & ROS 2
**[Full track guide](C_Robotics_ROS2/RoboticsROS2.md)**

**Assumption:** You know ROS 2 basics (nodes, pub/sub, topics). This track focuses exclusively on GPU acceleration — turning a CPU node into a GPU-accelerated one without breaking the ROS 2 contract.

The central constraint: copying sensor data through ROS messages is the latency killer. A 100k-point cloud serialized through a standard topic takes longer to copy than to process. You'll measure this, then eliminate it with Loaned Messages.

Steps: `C1_Node_Acceleration` → `C2_Costmap_Inflation` → `C3_Perception_Node`

### Build & run
```bash
source /opt/ros/humble/setup.bash
cd C_Robotics_ROS2/C1_Node_Acceleration
cmake -B build
cmake --build build
./build/opencl_ros_node_demo
```

### Verify
Console prints OpenCL context initialization time inside the node constructor, and confirms the context survives a simulated subscriber callback cycle without errors.

---

## Core Concepts

### The Copy Problem
In Module 1 you measured three stages: upload, kernel, download. For small images, upload dominated. For real workloads (4K video, 100k-point clouds, large triangle meshes), transfer is the hardest constraint.

The GPU is not slow. Moving data to it is slow. Every track in this module is about minimizing that movement.

### Zero-Copy in One Paragraph
`CL_MEM_COPY_HOST_PTR`: OpenCL copies your data into its own allocation. Safe, portable, costs time.

`CL_MEM_USE_HOST_PTR`: OpenCL uses your pointer directly. On UMA architectures (Intel iGPU, ARM Mali) the buffer is physically shared — zero latency.

`CL_MEM_ALLOC_HOST_PTR`: OpenCL allocates pinned (page-locked) host memory. DMA transfers to/from pinned memory are faster than from pageable memory.

You don't need to memorize these now. When the profiler shows upload time dominating, come back here and pick the right flag.

### Why YUV Exists (Track A)
Cameras and codecs don't store RGB. They store YUV (luminance + chrominance), because human vision is ~4× more sensitive to brightness than color. Chroma subsampling (4:2:0) discards 3/4 of the color data with near-zero perceptual loss, cutting bandwidth roughly in half. You'll encounter this the moment you open a real webcam frame and see a green-pink checkerboard — that's a raw NV12 buffer before conversion.

### Why BVH Exists (Track B)
A scene with N triangles and R rays requires O(N × R) intersection tests without acceleration. A Bounding Volume Hierarchy reduces this to O(R × log N) by partitioning triangles into a tree of axis-aligned bounding boxes. The stackless variant matters on GPU because GPU threads cannot use a recursive call stack — you'll implement iterative traversal using a bitmask to track which nodes to visit.

---

## Troubleshooting

- **Which track?** Track A for cameras/video. Track B for simulation, rendering, or scientific computing. Track C for robots.
- **Wrong GPU selected**: `GPU=NVIDIA ./build/binary`, `GPU=AMD ./build/binary`, `GPU=INTEL ./build/binary`. See [Module 1 Troubleshooting](../01_Host_API/HostAPI.md#troubleshooting).
- **`cmake -B build` fails before any code**: Run `clinfo -l`. If no platforms appear, the issue is your driver/runtime, not your code.
- **Track A: `find_package(OpenCV REQUIRED)` fails**: `sudo apt install libopencv-dev`. Verify: `pkg-config --modversion opencv4`.
- **Track C: ROS 2 packages not found**: `source /opt/ros/humble/setup.bash` before running CMake.

---

## Performance Gates

This module is complete when the flagship project passes its gate:

| Track | Project | Metric | Target |
|:------|:--------|:-------|:-------|
| A | AI Smart Webcam — Bokeh | Frame time | < 33 ms @ 1080p |
| A | AI Smart Webcam — Privacy ROI | Frame time | < 20 ms @ 1080p |
| B | Advanced Ray Tracer | Render time | 60 FPS @ 100k triangles, 1080p |
| C | Accelerated Perception Node | Pipeline latency | < 5 ms @ 100k points |

**Hint**: Use `cl::Event` profiling (from `01_Host_API/02_Visual_Kernel_Events`) on every kernel. Console output alone is not sufficient to claim the gate.

See [Optimization Toolbox](../99_Toolbox/Toolbox.md) for techniques to reach the gate: zero-copy, memory coalescing, work-group sizing, thread divergence, local memory, async pipelines.

---

## What's Next

After passing your track's performance gate, explore [Module 4 Add-ons](../04_Addons/Addons.md): vkFFT audio mini-project, FFmpeg hardware decoding pipeline, and the 3D Voxel Mapping grand finale (combining Track B ray casting with Track C Lidar data).
