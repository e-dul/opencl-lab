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
*   [00_Theory_Heterogeneous_Architecture](01_Host_API/00_Theory_Heterogeneous_Architecture.md) - Minimal theory
*   [01_Visual_Kernel](01_Host_API/01_Visual_Kernel/) - First graphical kernel (image brightening).
*   [02_Visual_Kernel_Events](01_Host_API/02_Visual_Kernel_Events/) - Introduction to profiling (Events).
*   [03_Buffers_Layout](01_Host_API/03_Buffers_Layout/) - Memory management and data layout.

### [Module 2: Projects (Specialization Paths)](02_Projects/)
The core part of the course. Choose one path:
*   **Path A: Multimedia & AI** ([02_Projects/A_Multimedia](02_Projects/A_Multimedia/))
    *   OpenCV Interop, YUV Processing, AI Smart Webcam.
*   **Path B: Graphics & HPC** ([02_Projects/B_Graphics_HPC](02_Projects/B_Graphics_HPC/))
    *   Ray Tracing, BVH, Device Enqueue, CLBlast.
*   **Path C: Robotics & ROS 2** ([02_Projects/C_Robotics_ROS2](02_Projects/C_Robotics_ROS2/))
    *   Node Acceleration, Costmap Inflation, Perception Node.

### [Toolbox: Optimization](99_Toolbox/)
A collection of "on-demand" optimization techniques. Projects link here when they need more performance.
*   Memory Coalescing, Thread Divergence, Local Memory Tiling, Debugging (Oclgrind).

### [Addons: Bonus Content](Addons/)
Advanced topics and case studies.
*   vkFFT, OpenCL vs CUDA, Deployment, SoftISP Debayering.

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
*   **Guidelines:** [CLAUDE.md](.CLAUDE.md) defines coding style and communication.
*   **Strategy:** [workflow/design/](workflow/design/) is the "Source of Truth". do not modify these files without explicit instruction.

---

## More Information

*   **License:** [LICENSE](LICENSE) (MIT)
*   **Author:** ED / Claude Code

---

## TODO

- Add links to external resources for more in depth information
- Command, agent and skill to review content without specific focus on code - pseudo user experience
- Command, agent and skill to review - technical audit, validate claims and conclusions
- Move B1 CLBlast section to addons
- Summarize command, agents, skills and usage for this project
- Mark sections with strict HW or OpenCL version dependencies