# Applied OpenCL Lab

Think of Applied OpenCL Lab as an OpenCL **cookbook for real systems**, not a traditional textbook.

Each module is a self‑contained “recipe”: a runnable project with a clear goal, required tools, step‑by‑step instructions, and a reference implementation that hits concrete performance gates (e.g., 30 FPS 1080p webcam processing).

You are encouraged to treat the provided code as one possible solution and then adapt, optimize, or re‑implement it to fit your own projects, as long as you meet the same verification and performance criteria.

**Key topics**: Zero-copy interop with OpenCV/FFmpeg/ROS2, SoftISP debayering, BVH ray tracing, ROS2 perception nodes, and an optimization toolbox covering memory coalescing, thread divergence, occupancy, and device enqueue.


---

## 📚 Table of Contents

1.  **[Introduction & Philosophy](#introduction--philosophy)**
2.  **[Course Structure](#course-structure)**
    *   [Module 0: Fundamentals](#module-0-fundamentals)
    *   [Module 1: Host API](#module-1-host-api)
    *   [Module 2: Projects (Specialization Paths)](#module-2-projects-specialization-paths)
    *   [Toolbox: Optimization](#toolbox-optimization)
    *   [Addons: Bonus Content](#addons-bonus-content)
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

### [Module 1: Host API](01_Host_API/)
Learning to control the GPU from C++.
*   [HostAPI.md](01_Host_API/HostAPI.md) - Setup instructions.
*   [01_Visual_Kernel](01_Host_API/01_Visual_Kernel/) - First graphical kernel (image brightening).
*   [02_Visual_Kernel_Events](01_Host_API/02_Visual_Kernel_Events/) - Introduction to profiling (Events).
*   [03_Buffer_Flags](01_Host_API/03_Buffer_Flags/) - Memory management and data layout.

### [Module 2: Projects (Specialization Paths)](02_Projects/)
The core part of the course. Choose one path:
*   **Path A: Multimedia & AI** ([02_Projects/A_Multimedia](02_Projects/A_Multimedia/))
    *   [A1_OpenCV_Interop](02_Projects/A_Multimedia/A1_OpenCV_Interop/) - Zero-copy buffer sharing with OpenCV.
    *   [A2_YUV_Pipeline](02_Projects/A_Multimedia/A2_YUV_Pipeline/) - YUV color space processing on GPU.
    *   [A2b_YUYV_Extension](02_Projects/A_Multimedia/A2b_YUYV_Extension/) - YUYV webcam format handling.
    *   [A3_1_OpenCV_DNN](02_Projects/A_Multimedia/A3_1_OpenCV_DNN/) - Neural network inference with OpenCV DNN.
    *   [A3_2_OpenVINO_GPU](02_Projects/A_Multimedia/A3_2_OpenVINO_GPU/) - Accelerated inference with OpenVINO.
    *   [A4_Smart_Webcam](02_Projects/A_Multimedia/A4_Smart_Webcam/) - Real-time webcam processing pipeline.
    *   [A5_Privacy_Mode](02_Projects/A_Multimedia/A5_Privacy_Mode/) - Face detection and GPU blurring.
*   **Path B: Graphics & HPC** ([02_Projects/B_Graphics_HPC](02_Projects/B_Graphics_HPC/))
    *   [B1_CLBlast_MatMul](02_Projects/B_Graphics_HPC/B1_CLBlast_MatMul/) - GPU matrix multiplication via CLBlast.
    *   [B2_Ray_Tracer_Basic](02_Projects/B_Graphics_HPC/B2_Ray_Tracer_Basic/) - Basic ray tracer running on GPU.
    *   [B3_Ray_Tracer_BVH](02_Projects/B_Graphics_HPC/B3_Ray_Tracer_BVH/) - Ray tracer with BVH acceleration structure.
    *   [B3_Ray_Tracer_BVH_Dynamic](02_Projects/B_Graphics_HPC/B3_Ray_Tracer_BVH_Dynamic/) - BVH for dynamic/animated scenes.
    *   [B4_Device_Enqueue](02_Projects/B_Graphics_HPC/B4_Device_Enqueue/) - GPU-driven kernel dispatch (OpenCL 2.0).
*   **Path C: Robotics & ROS 2** ([02_Projects/C_Robotics_ROS2](02_Projects/C_Robotics_ROS2/))
    *   [C1_Node_Acceleration](02_Projects/C_Robotics_ROS2/C1_Node_Acceleration/) - ROS 2 node with OpenCL acceleration.
    *   [C2_Costmap_Inflation](02_Projects/C_Robotics_ROS2/C2_Costmap_Inflation/) - GPU costmap inflation for Nav2.
    *   [C3_Perception_Node](02_Projects/C_Robotics_ROS2/C3_Perception_Node/) - Point cloud processing perception node.

### [Toolbox: Optimization](99_Toolbox/)
A collection of "on-demand" optimization techniques. Projects link here when they need more performance.

*   [Coalesced_Access](99_Toolbox/Coalesced_Access/) - Global memory access pattern optimization.
*   [Thread_Divergence](99_Toolbox/Thread_Divergence/) - Reducing warp/wavefront divergence.
*   [Local_Memory_Tile](99_Toolbox/Local_Memory_Tile/) - Tiled algorithms using local (shared) memory.
*   [Bank_Conflict_Test](99_Toolbox/Bank_Conflict_Test/) - Detecting and resolving local memory bank conflicts.
*   [Register_Pressure](99_Toolbox/Register_Pressure/) - Managing register usage and occupancy.
*   [Zero_Copy_Demo](99_Toolbox/Zero_Copy_Demo/) - Pinned memory zero-copy host↔device transfers.
*   [Debugging_Oclgrind](99_Toolbox/Debugging_Oclgrind/) - Kernel debugging and race detection with Oclgrind.

### [Addons: Bonus Content](04_Addons/)

Advanced topics and case studies.

*   [4_1_vkFFT_Audio](04_Addons/4_1_vkFFT_Audio/) - GPU FFT for audio processing via vkFFT.
*   [4_2_OpenCL_vs_CUDA](04_Addons/4_2_OpenCL_vs_CUDA/) - Performance and portability comparison.
*   [4_3_Deployment](04_Addons/4_3_Deployment/) - Packaging and shipping OpenCL applications.
*   [4_4_SVM_Theory](04_Addons/4_4_SVM_Theory/) - Shared Virtual Memory concepts and usage.
*   [4_5_Voxel_Mapping](04_Addons/4_5_Voxel_Mapping/) - 3D voxel mapping on GPU.
*   [4_6_FFmpeg_Pipeline](04_Addons/4_6_FFmpeg_Pipeline/) - Zero-copy FFmpeg + OpenCL video pipeline.
*   [4_7_SoftISP](04_Addons/4_7_SoftISP/) - Software ISP debayering pipeline.

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
- Mark sections with strict HW or OpenCL version dependencies
- Add better asset with selfie - blur is not clearly visible
- **[Feature] `cl::Buffer` sub-buffers — parallel band processing**: Slice a large frame into N horizontal bands; each band is a sub-buffer aliasing the same allocation, dispatched independently (separate queues for parallel execution). Natural motivation for work partitioning and memory aliasing without copies. Precursor to multi-GPU distribution. Candidate exercise: 4 horizontal strips on a 1080p image, benchmark vs. single full-frame dispatch.
- **[Feature] GPU-built BVH (LBVH via Morton codes)**: After B3's CPU SAH-BVH, add a challenge variant that constructs the BVH entirely on the GPU using Morton-code sorting + radix sort → parallel hierarchy build. Enables per-frame rebuild for dynamic scenes without CPU round-trip. Natural follow-on to B3 Challenge (Phase 4).
- **[Feature] `global_work_offset` — tiled processing benchmark**: Natural fit is explicit work partitioning over a static grid, not face-detection ROI (which forces an artificial host read-back). Candidate exercise: process a 4K image in 4 quadrants (2×2 tiles), each launched with `global_work_offset = {tile_x, tile_y}` / `global_work_size = {W/2, H/2}`. Compare against single dispatch with explicit offset arithmetic inside the kernel body.
- Consider removing C1 node acceleration - marginal value
- Consider ROS2 related examples to integrate with ROS world better(launch files, service to restart static publisher etc.)
- Improve using assets
- Work on setup with docker
- Explore AMD specific SDK features
- link kernels instead of copy?