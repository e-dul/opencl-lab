# Applied OpenCL Lab

Think of Applied OpenCL Lab as an OpenCL **cookbook for real systems**, not a traditional textbook.

Each module is a self‑contained "recipe": a runnable project with a clear goal, required tools, step‑by‑step instructions, and a reference implementation that hits concrete performance gates (e.g., 30 FPS 1080p webcam processing).

You are encouraged to treat the provided code as one possible solution and then adapt, optimize, or re‑implement it to fit your own projects, as long as you meet the same verification and performance criteria.

**Key topics**: Zero-copy interop with OpenCV/FFmpeg/ROS2, SoftISP debayering, BVH ray tracing, ROS2 perception nodes, and an optimization toolbox covering memory coalescing, thread divergence, occupancy, and device enqueue.


---

## 📚 Table of Contents

1.  **[Introduction & Philosophy](#introduction--philosophy)**
2.  **[Course Structure](#course-structure)**
    *   [Module 0: Fundamentals](#module-0-fundamentals)
    *   [Module 1: Host API](#module-1-host-api)
    *   [Module 2: Multimedia & AI](#module-2-multimedia--ai)
    *   [Module 3: Graphics & HPC](#module-3-graphics--hpc)
    *   [Module 4: Robotics & ROS 2](#module-4-robotics--ros-2)
    *   [Toolbox: Optimization](#toolbox-optimization)
    *   [Bonus Content](#bonus-content)
3.  **[Quick Start](#quick-start)**
4.  **[For AI Agent (Claude Code)](#for-ai-agent-claude-code)**
5.  **[More Information](#more-information)**

---

## Introduction & Philosophy

The goal of this project is to teach you **engineering**, not just OpenCL syntax. We focus on:
*   **Zero-Copy:** How not to waste time copying data between CPU and GPU.
*   **Profiling:** Measuring performance from the very first line of code.
*   **Real-World Scenarios:** Video processing, Ray Tracing, Robotics (ROS 2).

More about the project vision: [workflow/design/00-executive-summary.md](workflow/design/00-executive-summary.md).

---

## Course Structure

### [Module 0: Fundamentals](00_Setup/)
Environment setup, Docker, CMake, and the first "Smoke Test" verifying drivers.
*   [Setup.md](00_Setup/Setup.md) - Setup instructions.
*   [01_Smoke_Test](00_Setup/01_Smoke_Test/) - Simple `vector_add` program.

### [Module 1: Host API](01_Host_API/HostAPI.md)
Learning to control the GPU from C++.

*   [01_Visual_Kernel](01_Host_API/01_Visual_Kernel/) - First graphical kernel (image brightening).
*   [02_Visual_Kernel_Events](01_Host_API/02_Visual_Kernel_Events/) - Introduction to profiling (Events).
*   [03_Buffer_Flags](01_Host_API/03_Buffer_Flags/) - Memory management and data layout.

### [Module 2: Multimedia & AI](02_Multimedia/Multimedia.md)

OpenCV interop, YUV pipelines, neural network inference, and a real-time smart webcam.

*   [01_OpenCV_Interop](02_Multimedia/01_OpenCV_Interop/) - Zero-copy buffer sharing with OpenCV.
*   [02_YUV_Pipeline](02_Multimedia/02_YUV_Pipeline/) - YUV color space processing on GPU.
*   [03_YUYV_Extension](02_Multimedia/03_YUYV_Extension/) - YUYV webcam format handling.
*   [04_OpenCV_DNN](02_Multimedia/04_OpenCV_DNN/) - Neural network inference with OpenCV DNN.
*   [05_OpenVINO_GPU](02_Multimedia/05_OpenVINO_GPU/) - Accelerated inference with OpenVINO.
*   [06_Smart_Webcam](02_Multimedia/06_Smart_Webcam/) - Real-time webcam processing pipeline.
*   [07_Privacy_Mode](02_Multimedia/07_Privacy_Mode/) - Face detection and GPU blurring.
*   [08_FFmpeg_Pipeline](02_Multimedia/08_FFmpeg_Pipeline/) - Zero-copy FFmpeg + OpenCL video pipeline.
*   [09_SoftISP](02_Multimedia/09_SoftISP/) - Software ISP 4K Bayer debayering.

### [Module 3: Graphics & HPC](03_GraphicsHPC/GraphicsHPC.md)

Ray tracing with progressive BVH acceleration structures.

*   [01_Ray_Tracer_Basic](03_GraphicsHPC/01_Ray_Tracer_Basic/) - Basic ray tracer running on GPU.
*   [02_Ray_Tracer_BVH](03_GraphicsHPC/02_Ray_Tracer_BVH/) - Ray tracer with BVH acceleration structure.
*   [03_Ray_Tracer_BVH_Dynamic](03_GraphicsHPC/03_Ray_Tracer_BVH_Dynamic/) - BVH for dynamic/animated scenes.

### [Module 4: Robotics & ROS 2](04_Robotics/RoboticsROS2.md)

GPU acceleration inside a ROS 2 perception pipeline.

*   [01_Node_Acceleration](04_Robotics/01_Node_Acceleration/) - ROS 2 node with OpenCL acceleration.
*   [02_Costmap_Inflation](04_Robotics/02_Costmap_Inflation/) - GPU costmap inflation for Nav2.
*   [03_Perception_Node](04_Robotics/03_Perception_Node/) - Point cloud processing perception node.

### [Toolbox: Optimization](05_Toolbox/Toolbox.md)

A collection of "on-demand" optimization techniques. Projects link here when they need more performance.

*   [01_Local_Memory](05_Toolbox/01_Local_Memory/) - Tiled algorithms using local (shared) memory.
*   [02_Coalesced_Access](05_Toolbox/02_Coalesced_Access/) - Global memory access pattern optimization.
*   [03_Debugging](05_Toolbox/03_Debugging/) - Kernel debugging and race detection with Oclgrind.
*   [04_Deployment](05_Toolbox/04_Deployment/) - Packaging and shipping OpenCL applications.
*   [05_Fast_Math](05_Toolbox/05_Fast_Math/) - Fast math flags and accuracy tradeoffs.
*   [06_Generic_Kernel_Templates](05_Toolbox/06_Generic_Kernel_Templates/) - Generic kernel templates for multiple types.
*   [07_Global_Work_Offset](05_Toolbox/07_Global_Work_Offset/) - Global work offset for sub-region dispatch.
*   [08_Multi_GPU_Strategy](05_Toolbox/08_Multi_GPU_Strategy/) - Multi-GPU distribution strategies.
*   [09_OpenCL_vs_CUDA](05_Toolbox/09_OpenCL_vs_CUDA/) - Performance and portability comparison.
*   [10_SVM](05_Toolbox/10_SVM/) - Shared Virtual Memory deep dive.
*   [12_Sync_Atomics](05_Toolbox/12_Sync_Atomics/) - Atomic operations and synchronization.
*   [13_Thread_Divergence](05_Toolbox/13_Thread_Divergence/) - Reducing warp/wavefront divergence.
*   [14_Work_Group_Sizing](05_Toolbox/14_Work_Group_Sizing/) - Occupancy and work-group sizing.
*   [15_Zero_Copy](05_Toolbox/15_Zero_Copy/) - Pinned memory zero-copy host↔device transfers.
*   [16_Async_Multi_Thread](05_Toolbox/16_Async_Multi_Thread/) - Async pipelines and multi-threading patterns.

### [Bonus Content](06_Bonus/)

Advanced modules that extend the core tracks.

*   [01_CLBlast_MatMul](06_Bonus/01_CLBlast_MatMul/) - GPU matrix multiplication via CLBlast.
*   [02_Device_Enqueue](06_Bonus/02_Device_Enqueue/) - GPU-driven kernel dispatch (OpenCL 2.0).
*   [03_VkFFT_Audio](06_Bonus/03_VkFFT_Audio/) - GPU FFT for audio processing via vkFFT.
*   [04_Voxel_Mapping](06_Bonus/04_Voxel_Mapping/) - 3D voxel mapping on GPU.

---

## Quick Start

Requirements: CMake 3.18+, C++17 Compiler, OpenCL Drivers (Nvidia/Intel/AMD/PoCL).

```bash
# 1. Clone the repository
git clone https://github.com/your-username/Applied-OpenCL-Lab.git
cd Applied-OpenCL-Lab

# 2. Build all modules (default configuration)
cmake -B build
cmake --build build -j$(nproc)

# 3. Run Smoke Test (environment verification)
./build/00_Setup/01_Smoke_Test/smoke_test
```

To build a specific project (e.g., Visual Kernel):
```bash
cd 01_Host_API/01_Visual_Kernel
cmake -B build
cmake --build build
./build/visual_kernel
```

---

## For AI Agent (Claude Code)

This repository is optimized for collaboration with AI assistants.
*   **Workspace:** [workflow/](workflow/) contains protocols, memory, and active tasks.
*   **Guidelines:** [CLAUDE.md](CLAUDE.md) defines coding style and communication.
*   **Strategy:** [workflow/design/](workflow/design/) is the "Source of Truth". do not modify these files without explicit instruction.

---

## More Information

*   **License:** [LICENSE](LICENSE) (MIT)
*   **Author:** ED / Claude Code

---

## TODO

- Add links to external resources for more in depth information
- Summarize command, agents, skills and usage for this project
- Add better asset with selfie - blur is not clearly visible
- **[Feature] `cl::Buffer` sub-buffers — parallel band processing**: Slice a large frame into N horizontal bands; each band is a sub-buffer aliasing the same allocation, dispatched independently (separate queues for parallel execution). Natural motivation for work partitioning and memory aliasing without copies. Precursor to multi-GPU distribution. Candidate exercise: 4 horizontal strips on a 1080p image, benchmark vs. single full-frame dispatch.
- **[Feature] GPU-built BVH (LBVH via Morton codes)**: After B3's CPU SAH-BVH, add a challenge variant that constructs the BVH entirely on the GPU using Morton-code sorting + radix sort → parallel hierarchy build. Enables per-frame rebuild for dynamic scenes without CPU round-trip. Natural follow-on to B3 Challenge (Phase 4).= {W/2, H/2}`. Compare against single dispatch with explicit offset arithmetic inside the kernel body.
- Consider ROS2 related examples to integrate with ROS world better(launch files, service to restart static publisher, parameters handling etc.)
- Work on setup with docker
- Explore AMD specific SDK features
- Add AI assisted coding experiment note or disclaimer to adjust expectations
