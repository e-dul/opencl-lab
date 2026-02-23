# Executive Summary: Applied OpenCL Lab

**Version:** 1.0 (Final)
**Status:** Active Strategy
**Target Audience:** Mid/Senior C++ Engineers, Roboticists, HPC Developers

---

## 1. Vision & Philosophy

**Applied OpenCL Lab** is a practical educational resource designed to teach heterogeneous system engineering (CPU+GPU) through "Learning by Building". Unlike traditional courses that focus on dry syntax or outdated standards, this project focuses on **Zero-Bullshit Engineering**.

### Core Philosophies
1.  **Code-First Knowledge:** The repository is the textbook. We learn by reading and writing working code, not by memorizing definitions.
2.  **Just-in-Time Learning:** Theory is introduced only when a specific engineering problem requires it. (e.g., "Why is this kernel slow?" -> "Here is the theory of Memory Coalescing").
3.  **Don't Reinvent the Wheel:** We use established libraries (CLBlast, vkFFT, OpenCV) for standard tasks and write custom OpenCL kernels only when they provide a distinct advantage (Custom ISP, Ray Tracing, Zero-Copy integration).
4.  **Performance Culture:** "Measure, then Optimize." Profiling (Events/Timestamps) is mandatory from Module 1.

---

## 2. Content Architecture: Hub & Spoke

The course structure is non-linear to support different specializations while sharing a common core of optimization techniques.

### The Hub: Optimization Toolbox (`99_Toolbox/`)
A shared library of isolated optimization techniques. Projects in Module 2 "link" to these tools when they hit performance bottlenecks.
-   **Memory:** Zero-Copy (Map vs Copy), Coalesced Access, Shared Virtual Memory (SVM).
-   **Execution:** Thread Divergence, Work-Group Sizing, Occupancy.
-   **Advanced:** Device Enqueue, Sub-groups, Inline PTX (if applicable).

### The Spokes: Specialization Paths (`02_Projects/`)
User choose **one** path but share the same foundational knowledge.

#### 🎥 Path A: Multimedia & AI
*Focus: High-bandwidth video processing and Edge AI.*
-   **Key Concepts:** Zero-Copy Interop (OpenCV/FFmpeg), YUV Color Spaces, AI Post-processing.
-   **Flagship Project:** **AI Smart Webcam** (Bokeh effect, Privacy blurring) running at >30 FPS on 1080p.
-   **Unique Tech:** SoftISP (Raw Debayering), Hardware Video Decoding integration.

#### 🎮 Path B: Graphics & HPC
*Focus: Compute-heavy math and rendering.*
-   **Key Concepts:** Ray Tracing, Bounding Volume Hierarchies (BVH), Dynamic Parallelism.
-   **Flagship Project:** **Advanced Ray Tracer** with Stackless BVH traversal.
-   **Unique Tech:** Device Enqueue (GPU spawning GPU work), CLBlast integration.

#### 🤖 Path C: Robotics & ROS 2
*Focus: Low-latency sensor processing and autonomy.*
-   **Key Concepts:** ROS 2 Node Acceleration, Costmap Inflation, Lidar Point Clouds.
-   **Flagship Project:** **Accelerated Perception Node** (Lidar filtering + Feature extraction).
-   **Unique Tech:** ROS 2 Loaned Messages (Zero-Copy middleware transport), Real-time guarantees.

---

## 3. Operational Workflow: Strategy & Tactics

This project is built using a strict AI-Assisted Engineering protocol to ensure quality and scalability.

### The "Design Wins" Rule
-   **Strategy (`design/*.md`):** The Single Source of Truth. If the Code contradicts the Design, the Code is wrong.
-   **Tactics (`workflow/tasks/`):** Ephemeral, atomic units of work. Generated from Strategy, executed, then archived.
-   **Review:** All code is reviewed against the "Performance Gates" defined in the Strategy (e.g., "Must process 1080p < 16ms").

---

## 4. Key Technical Decisions

-   **OpenCL Version:** **OpenCL 1.2** is the baseline for maximum compatibility (Nvidia, Intel, Mobile, FPGA), with OpenCL 2.0+ features (Device Enqueue, SVM) treated as extensions.
-   **C++ Wrapper:** We use `cl.hpp` (C++ bindings) for RAII and type safety. Raw C API is avoided in user code.
-   **Build System:** **CMake** with a "Hybrid" structure.
    -   *Root:* Orchestrates the build of selected modules.
    -   *Leaf:* Every project is standalone buildable to ensure users can copy-paste it into their work.
-   **Snapshot Folders:** Code evolves in separate directories (e.g., `01_Visual_Kernel` -> `02_Visual_Kernel_Events`) rather than Git branches, allowing side-by-side comparison in IDEs.

---

## 5. Performance Gates (Examples)

Success is defined by metrics, not just compilation.

| Project | Metric | Target (Standard GPU) |
| :--- | :--- | :--- |
| **Visual Kernel** | Execution Time | < 1ms (1080p) |
| **Smart Webcam** | Frame Rate | 30 FPS (1080p) |
| **Ray Tracer** | Scene Complexity | 100k Triangles @ 60 FPS |
| **ROS 2 Node** | Latency | < 5ms (Point Cloud) |