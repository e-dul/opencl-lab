# Executive Summary: Applied OpenCL Lab

**Version:** 2.2 (Final)
**Status:** Minor update - see v2.2 Update Summary 
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

For details see Updated Structure

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

---

## 6. Updated Structure

### MODULE 0: Foundations & Environment (10% / 4.0h)

1.  **0.1:** Setup: Docker (Nvidia/PoCL) & CMake (Reproducibility).
2.  **0.2:** Technical "Smoke Test" (vector_add – driver verification only).

### MODULE 1: Host API & Feedback Loop (15% / 6.0h)

**Key change:** Introducing performance measurement right from the very first steps.

0.  **0.0:** Heterogeneous Architecture in a Nutshell (Host vs Device, Command Queues).

1.  **1.1:** Educational Hello World – "Visual Kernel" (Image Brightener).
    1.  Immediate visual verification of errors.
    2.  MAD operation (multiply-add): `output = pixel × contrast + brightness`.
2.  **1.2:** Application Anatomy (C++ Wrapper, RAII, Error Handling).
3.  **1.3:** Profiling Basics (Events & Timestamps).
    1.  Measuring CPU vs GPU time from the first project.
    2.  Building the habit: "Measure, then optimize".

### MODULE 2: Real-World Integration (60% / 24.0h)

**Philosophy:** Each student picks one specialization path. All paths utilize the same Optimization Toolbox.

#### Path A: Multimedia Systems (Video & AI) – 20% / 8.0h
**Goal:** Throughput & Memory Integration.

1.  **A.1:** OpenCV & OpenCL Interop.
    1.  Problem: Copying a 4K frame takes longer than processing it.
    2.  Solution: *[Toolbox: Zero-Copy & SVM]*.
2.  **A.2:** Video Processing Pipeline (YUV Theory + Kernels).
    1.  **YUV vs RGB Formats:** Why do cameras use YUV? Chroma Subsampling (4:2:0 vs 4:4:4).
    2.  **Memory Layout:** Planar vs Packed. Stride/Pitch (avoiding segfaults).
    3.  **Kernels:** Color Space Conversion (NV12 to RGBA). Optimization: Integer Math and vectors (uchar8).
    4.  **Channel Extraction:** Extracting the Y channel (Greyscale) for AI without copying.
    5.  **Goal:** The student stops treating the image as a black box (`cv::Mat`) and starts seeing a raw byte buffer.
3.  **A.3.1** AI Inference (Edge AI / DNN Backend) -  OpenCV DNN (T-API Approach): 
    1. Focuses on the ease of use with cv::UMat and DNN_TARGET_OPENCL.
    2. Postprocessing (NMS, bbox decoding) in OpenCL.
4.  ~~**A.3.2** TensorFlow Lite (Edge Approach):~~
    ~~1. Focuses on explicit memory mapping (clEnqueueMapBuffer), passing raw OpenCL buffer pointers to TfLiteGpuDelegateV2, and edge optimization.~~
    ~~2. Postprocessing (NMS, bbox decoding) in OpenCL.~~
    > ❌ **Cancelled:** TFLite GPU delegate `.so` is ARM-packaged — no official pre-built x86_64 binary exists. Building from source requires the Android NDK toolchain. Not feasible for a cross-platform educational module.
4.  **A.3.2** OpenVINO GPU Plugin (Intel iGPU — Intel-only):
    1. Intel-focused: pass `cl::Buffer` directly as input/output tensor via OpenVINO RemoteTensor API — zero host round-trip between inference and OpenCL kernel.
    2. Postprocessing (NMS, bbox decoding) in OpenCL.
    3. Educational angle: OpenVINO internally runs on OpenCL — exposes what the T-API hides in A3_1, at the raw `cl_mem` handle level, on hardware where it reliably works.
5.  **A.4:** **PROJECT #1: AI Smart Webcam.**
    1.  Pipeline: Camera (OpenCV) → AI Segmentation → OpenCL Blur → Screen.
    2.  **Main Tutorial: "Bokeh Mode" (Background Blur)**
        1.  AI Model: Person segmentation (MediaPipe Selfie Segmentation).
        2.  Output: Pixel mask (0 = background, 1 = person).
        3.  OpenCL Kernel: Full-frame processing with a conditional `if (mask[id] == BACKGROUND)`.
        4.  Educational Goal: Memory bandwidth optimization (Zero-Copy between AI and OpenCL). Full-frame processing on GPU.
    3.  **Challenge/Homework: "Privacy Mode" (Face Blur)**
        1.  AI Model: Face detection (YuNet / Haar Cascade).
        2.  Output: Bounding Box (x, y, width, height).
        3.  Task: Modify host-code to launch the blur kernel only on the image crop (ROI).
        4.  Educational Goal: Region of Interest processing. Configuring `global_work_offset` and `global_work_size`. Optimization by "not processing unnecessary pixels". Boundary checks at ROI edges.
        5.  Bonus: Compare Full Frame vs ROI performance (measuring via Events).
    4.  **Performance Gate:** Bokeh Mode < 33ms/frame @ 1080p (30 FPS). Privacy Mode < 20ms/frame @ 1080p (50 FPS on a single face).

#### Path B: Algorithms & Graphics (HPC) – 20% / 8.0h
**Goal:** Compute Power & Vector Math.

1.  **B.1:** High-Performance Math (CLBlast / BLAS).
    1.  Using ready-made libraries for matrix multiplication.
    2.  When to write your own kernel vs. when to use a library?
2.  **B.2:** Graphics: Ray Tracer (Basic + OpenGL Interop).
    1.  Ray Tracing basics: rays, intersections, lighting.
    2.  Interop with OpenGL (displaying without copying).
3.  **B.3:** **PROJECT #2: Advanced Ray Tracer (Stackless BVH).**
    1.  Problem: Stuttering on complex scenes (100k+ triangles).
    2.  **BVH (Bounding Volume Hierarchy):** Building the acceleration structure.
    3.  **Stackless Traversal:** Avoiding recursion on the GPU (iterative tree traversal).
    4.  Solution: *[Toolbox: Thread Divergence]* (using `select()` instead of `if-else`) and *[Toolbox: Fast Math]*.
    5.  Performance Gate: 60 FPS for a scene with 100k triangles @ 1080p.
4.  **B.4:** Deep Dive: Dynamic Parallelism (Device Enqueue & Templates).
    1.  **Problem:** Recursive ray bounces in Ray Tracer require returning to CPU (slow).
    2.  **Solution (OpenCL 2.0+):** Using `enqueue_kernel` from the GPU side.
    3.  **Nested Parallelism:** The kernel itself decides if it needs more compute power for reflections/refractions.
    4.  **Kernel Templates:** Dynamic parameterization of kernels at runtime.
    5.  **Goal:** Showcasing a unique OpenCL feature missing in older APIs.
    6.  **Level:** Advanced (Theory: 9/10).

#### Path C: Robotics & Autonomy (ROS 2) – 20% / 8.0h
**Goal:** Latency & Middleware Integration.

**Assumption:** Student knows ROS 2 basics (pub/sub, nodes). We focus solely on acceleration.

1.  **C.1:** ROS 2 Node Anatomy (Node Acceleration).
    1.  How to turn a CPU node into a GPU-accelerated node?
    2.  OpenCL integration in subscriber callbacks.
    3.  Managing OpenCL context within the node lifecycle.
2.  **C.2:** 2D Costmap Inflation (Map Algorithms).
    1.  Practical problem in AMR robots (warehouses, vacuums).
    2.  Inflation algorithm: cost propagation from obstacles.
    3.  OpenCL kernel for distance transform.
3.  **C.3:** **PROJECT #3: Accelerated Perception Node.**
    1.  Pipeline: Lidar/Camera → OpenCL Processing → ROS Message.
    2.  Problem: Overhead of ROS message serialization (data copying).
    3.  Solution: *[Toolbox: Shared Memory & Loaned Messages]*.
    4.  **Zero-Copy Transport:** Using ROS 2 Loaned Messages to share memory between nodes.
    5.  Performance Gate: Node latency < 5ms for a pointcloud of 100k points.


### RESOURCE: Optimization Toolbox (ex-Module 3) – 10% / 4.0h

**Format:** A library of "How-To" videos/articles linked from Module 2.

**Thematic Breakdown:**

#### Tool 1: Memory Management Deep Dive (1.6h)
1.  **Zero-Copy (Mapping vs Copying).**
    1.  `CL_MEM_USE_HOST_PTR` vs `CL_MEM_ALLOC_HOST_PTR`.
    2.  When to map, and when to copy?
    3.  Pinned memory and its impact on performance.
2.  **Coalesced Access (How to read memory without clogging the bus).**
    1.  Memory transactions and bank conflicts.
    2.  Alignment and stride patterns.
    3.  Examples: Image access optimization.
3.  **Shared Virtual Memory (SVM).**
    1.  Unified Memory in OpenCL 2.0+.
    2.  Fine-grained vs Coarse-grained SVM.
    3.  When does SVM make sense (and when does it not)?

#### Tool 2: Kernel Execution Optimization (1.6h)
1.  **Work-Group Size & Occupancy (How to pick `local_work_size`).**
    1.  Relationship between work-group size and registers/LDS.
    2.  Occupancy calculator.
    3.  Autotuning: profiling different configurations.
2.  **Thread Divergence (Why `if-else` hurts).**
    1.  SIMD/SIMT execution model.
    2.  Cost of warp divergence.
    3.  Techniques: `select()`, branchless code, predication.
3.  **Local Memory (LDS) – Manual cache in the kernel.**
    1.  When to use `__local`?
    2.  Tile-based processing (example: convolution, debayering).
    3.  Synchronization: `barrier(CLK_LOCAL_MEM_FENCE)`.

#### Tool 3: Debugging & Professional Tools (0.8h)
1.  **Oclgrind (Memory error simulation).**
    1.  Detecting out-of-bounds, race conditions.
    2.  Running kernels in verification mode.
2.  **Nsight/VTune (Professional profiler demo).**
    1.  Timeline analysis (CPU-GPU interactions).
    2.  Kernel bottleneck identification.
    3.  Memory bandwidth analysis.

#### Tool 4: GenericKernelTemplates (1.0h)
1.  "Kernel templates" via compilation options (`clBuildProgram` + `-D …`) – one `.cl` codebase, multiple type variants (`TYPE=float/uchar/half`).
2.  Simple variant autotuning based on profiling (selecting the fastest variant at application startup).
3.  Example: `threshold.cl` with a `TYPE` macro, built with different flags `-DTYPE=float`, `-DTYPE=uchar`, `-DTYPE=half`.
4.  Goal: Eliminating code duplication (DRY) + automatic per-device optimization.

#### Tool 5: AsyncMultiThread (1.0h)
1.  Asynchronous enqueue (events, non-blocking transfers/kernels).
2.  Usage pattern in multi-threaded applications: shared context, per-thread queue, synchronization via events instead of `clFinish`.
3.  Example: `clEnqueueWriteBuffer(..., CL_FALSE, ..., &event_write); clEnqueueNDRangeKernel(..., 1, &event_write, &event_kernel); clWaitForEvents(1, &event_kernel);`
4.  Goal: Maximum throughput via asynchronous pipeline + eliminating host thread blocking.

#### Tool 6: MultiGPU_Strategy (1.0h)
1.  Working on multi-GPU systems (device selection, per-device queues, data/workload splitting).
2.  Event synchronization between devices.
3.  Recipe: Device enumeration → separate `cl_context` per GPU → data split (static/dynamic) → result merge.
4.  Application: When a throughput/latency limit appears on a single GPU.
5.  Performance Gate: Speedup ≥ 1.65x with 2x GPUs.

#### Tool 7: Synchronization & Atomics (1.2h)
1. **Race Conditions (Why threads overwrite each other).**
  - Read-Modify-Write hazards in global and local memory.
  - When a simple `x = x + 1` corrupts data (e.g., building histograms or BVH structures).
2. **Atomic Operations (Resolving conflicts).**
  - Core functions: `atomic_add`, `atomic_inc`, `atomic_max`.
  - Global vs. Local atomics (differences in latency and practical use cases).
3. **Compare-and-Swap (Advanced synchronization).**
  - Using `atomic_cmpxchg` to build custom mechanisms (e.g., spinlocks in GPU memory).
  - The performance cost of atomic operations — why they should be used as a last resort.

### MODULE 4: Add-ons (Bonus & Case Studies) – 5% / 2.0h

1.  **4.1:** vkFFT (Audio Mini-Project) – showcasing ready-made accelerated libraries.
    1.  Fast Fourier Transform on GPU.
    2.  Example: Real-time spectrogram.
2.  **4.2:** OpenCL vs CUDA (Honest Market Analysis).
    1.  Where CUDA wins (AI libraries, Nvidia ecosystem).
    2.  Where OpenCL makes sense (FPGA, Mobile, Intel iGPU, portable code, AMD).
    3.  Pragmatic decision: when to choose which tool?
3.  **4.3:** Application Distribution (Deployment).
    1.  How to ship an OpenCL application to the client?
    2.  Runtime dependencies (ICD Loader, drivers).
    3.  Installers and packaging (Docker, AppImage, Windows installers).
4.  **4.4:** Deep Dive: SVM and Zero-Copy Theory.
    1.  In-depth theoretical material for the curious.
    2.  Memory coherency models.
    3.  Hardware support: PCIe, UMA architectures.
5.  **4.5:** The "Fusion" Project: 3D Voxel Mapping (Lidar + Ray Casting).
    1.  **Grand Finale** for graduates of Path B and C.
    2.  Using Ray Casting (from Project #2) to build a 3D map from a point cloud (ROS 2).
    3.  Demonstrates the power of knowledge transfer: computer graphics technique (gaming) applied to robot navigation.
    4.  Algorithm: Ray Casting in Voxel Grid space to remove dynamic noise.
6.  **4.6:** FFmpeg Deep Dive: Professional Video Pipeline.
    1.  **Title:** "Offline Video Processing: FFmpeg + OpenCL".
    2.  **Goal:** Hardware decoding (NVDEC/VAAPI) directly to GPU memory (Zero-Copy from decoder).
    3.  **Project:** "Transcoder with Filter". Take an .mp4 file, apply an effect (sepia/blur using the kernel from Project #1), save a new .mp4.
    4.  **Why it matters:** Shows how to use the same kernel in two different worlds (Real-Time Camera vs Offline Processing).
    5.  **FFmpeg C API:** Contexts, packets, codecs, hardware acceleration contexts.
7.  **4.7:** SoftISP: Real-Time 4K Raw Processing (Debayering Case Study).
    1.  **Problem:** Industrial camera outputs raw Bayer pattern (RGGB). How to convert this to RGB in real-time at 4K/60fps?
    2.  **Scope:** Focusing on the single hardest step – Demosaicing (Debayering).
    3.  **Kernel V1 (Naive):** Simple bilinear interpolation (reading 4 neighbors from global memory). Result: Works, but slow.
    4.  **Kernel V2 (Optimized):** Using Local Memory (LDS) to load an image tile once, then reading neighbors from fast local memory. Result: 5x faster.
    5.  **Application:** Showing that an OpenCL kernel is faster than `cv::cvtColor(Bayer2RGB)` on CPU.
    6.  **Educational Goal:** The perfect example of Local Memory (LDS) optimization. Memory bound operation. Tile-based processing pattern.
    7.  **Visual Effect:** Turning a green-pink checkerboard (RAW) into a beautiful, colorful RGB image – huge satisfaction.
    8.  **Market Niche:** Embedded vision engineers using cheap sensors (without hardware ISP) or raw industrial cameras.


## Pivot to Cookbook v2.0 Summary (2026-03-22)

### Context

v1 was module-based (00_Setup → 04_Addons). Too linear for "code-first" philosophy.

**Decision:** Full Cookbook Reorganization. Convert to Hub & Spoke with independent, runnable recipes:

New structure: 
- Parts 0-1: Sequential foundations (Setup + Host API)
- Parts 2-4: Three parallel Tracks (A: Multimedia, B: Graphics/HPC, C: Robotics)
- Part 5: Toolbox (reference tools)
- Part 6: Bonus (standalone recipes, cross-track capstones)

Detailed changes:

1. Move FFmpeg Pipeline and SoftISP from Addons to Multimedia track
2. Rename Addons to Bonus
3. Move CLBlast and Device Enqueue from GraphicsHPC to Bonus
4. Move OpenCL vs CUDA, SVM Theory and Deployment from Bonus to Toolbox
5. Update main folder to match new structure
    - Split 02_Projects to -> 02_Multimedia, 03_GraphicsHPC, 04_Robotics
    - Rename 99_Toolbox to 05_Toolbox
    - Rename 04_Addons to 06_Bonus

### Protocol changes

Updated name formats: T<id>_*.md for tasks and D<id>_*.md for designs for easier filtering.

### Design limtations

1. Support focused on linux and tested on Ubuntu 24.04
2. Due to common utilites location fixed structure is assumed - user moving folders and/or CMake is not a concern
3. Duplications can be side effect of independent runnable recipies approach, but allow to experiment is more controlled way.

### New content

#### Toolbox: `global_work_offset` \& Tiled Benchmark

* **Problem:** Processing a full 4K frame when only a small bounding box (Region of Interest - ROI) requires computation (e.g., face blurring). Masking pixels inside the kernel wastes compute, and cropping the image on the CPU wastes memory bandwidth.
* **Solution:** Utilizing `global_work_offset` and `global_work_size` from the Host API to launch threads exclusively over the target tile, without copying buffers or rewriting the kernel.
* **Index Space \& Memory Layout:**
    - Understanding how `get_global_id()` shifts when an offset is applied.
    - Stride and Pitch awareness: Calculating correct linear buffer indices when the thread's logical `(0,0)` corresponds to `(offset_x, offset_y)` in the actual image memory.
    - Boundary checks: Handling edge cases when the ROI tile does not perfectly align with the `local_work_size`.
* **The Tiled Benchmark:**
    - **Educational Goal:** Using `cl_event` to profile and compare the execution time of processing a 256x256 tile versus the full 4K frame.
    - **Application:** Demonstrating that "not processing unnecessary pixels" via Host API configuration is fundamentally faster and more power-efficient than using `if-else` branch masking inside the kernel.

*(Note: This recipe directly supports the "Privacy Mode" Face Blur challenge in Track A, Project \#1)*

### Improvements

#### Copy assets to binary directory via CMake POST_BUILD

Previously, docs required fragile relative paths like `../../../assets/sample.bmp` — depth-dependent and silently broken if run from the wrong directory. To eliminate path-counting entirely, symlink `assets/` into each module's binary dir at build time (mirroring the existing kernel-copy pattern), so every module's docs reduce to a single flat path.

Add to each module's CMakeLists.txt:

```
add_custom_command(TARGET <target> POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E create_symlink
          ${CMAKE_SOURCE_DIR}/assets
          $<TARGET_FILE_DIR:<target>>/assets)
```

Docs show: `./build/opencv_interop_demo --input assets/sample.bmp`

#### README Unification — Toolbox Pattern for All Modules

All module documentation adopts a two-level structure:

**Module index** (`<ModuleName>.md`): thin (~40-60 lines), contains the common prerequisites,
hardware limitations, and a navigation table linking to sub-modules.

**Sub-module doc** (`<SubName>/<SubName>.md`): self-contained, follows one of two templates:
- *Track/Bonus sub-module*: `# N.M — Name` → Goal, Prerequisites (delta), Build & Run, Verify, Key Concepts
- *Toolbox entry*: `# Tool Name` → Symptom, Prerequisites (delta), Build & Run, Verify
- Always link to Module index

Custom file names (e.g. `LocalMemory.md`, `OpenCVInterop.md`) are kept — not renamed to
`README.md` — for clearer navigation in editors and search results.

Links between index and sub-modules replace duplicated prerequisite/limitation blocks.
Toolbox and Bonus are already compliant; Multimedia, GraphicsHPC, Robotics, and Host API
require splitting their current fat single-file READMEs into this structure.

## v2.1 Update Summary (2026-03-25)

### Context

Eliminate remaining structural debt, module consolidation opportunities, and quality gaps not addressed in the v2.0 backlog (D10).

> **Purpose:** Cross-cutting improvement backlog for the Applied OpenCL Lab v2.1.

### Changes

- Merge 10_SVM into 11_SVM_Theory — Absorb `10_SVM`'s Buffer+Map/Unmap baseline mode and per-phase split timing into `11_SVM_Theory/main.cpp.
- UX Audit v2 — Re-run `/test-ux` across all 7 modules in their v2.0 structure.
- Tech Audit v2 — Re-run `/audit` across all 7 modules against their current v2.0 READMEs.

## v2.2 Update Summary (2026-03-28)

### Context

Further improvements.

> **Purpose:** Optimization Toolbox consolidation, deep-hardware edge cases, and memory partitioning.

### New content

#### New Core Tool: Sub-Buffers (Tool 10)
The final empty slot in the Optimization Toolbox has been allocated to **Sub-Buffers**, serving as the critical bridge between single-GPU constraints and advanced multi-queue pipelines.
*   **Tool:** `10_Sub_Buffers_Partitioning`
*   **Symptom:** VRAM limits exceeded / High CPU overhead from manual `memcpy` chunking.
*   **Educational Value:** Replaces the C++ anti-pattern of manual host-side slicing. Teaches safe memory aliasing (`cl::Buffer::createSubBuffer`) with zero data movement.
*   **Strategic Placement:** Acts as the mechanical prerequisite for out-of-core streaming, `08_Multi_GPU_Strategy`, and `16_Async_Multi_Thread` pipelining.

### Improvements

#### Architectural Refinement: Preventing Toolbox Bloat
To maintain a high-signal-to-noise ratio and avoid overwhelming students with standalone theory, hardcore hardware edge cases have been strategically nested into existing foundational tools as **Advanced Challenges**:
*   **AoS vs. SoA (Data Layout):** Added as an advanced extension inside 02_Coalesced_Access. Demonstrates how Object-Oriented C++ structs (Array of Structures) implicitly create strided memory access, wasting up to 75% of memory bus bandwidth. Teaches the Data-Oriented shift to Structure of Arrays (SoA) to guarantee perfect 128-byte cache line utilization.
*   **Bank Conflicts:** Added as an advanced extension inside `01_Local_Memory`. Demonstrates how bad stride patterns destroy LDS bandwidth and teaches `+1` padding fixes.
*   **Register Pressure & Spilling:** Added as an advanced extension inside `14_Work_Group_Sizing`. Teaches students to break the "black box" of the compiler (e.g., using `-cl-nv-verbose`) to diagnose silent occupancy drops and VRAM spilling caused by excessive `private` variable usage.

#### Project-Level Refinements: The "Silicon Realities"
Critical data layout and vectorization "gotchas" are now deeply integrated directly into the project modules (Path B - Ray Tracer, Path C - Robotics) as inline **"Stop and Read"** engineering lessons, rather than isolated theory:
*   **The `float3` Alignment Trap:** Teaching the reality that OpenCL aligns `float3` to 16 bytes. Exposing implicit widening bugs (e.g., AMD driver `NaN` generation on `normalize()`) and the necessity of explicit custom math functions (like `dot3`).
*   **Hardware-Safe C++ Structs:** Enforcing strict memory padding rules (`int pad[2]`) and the mandatory use of `static_assert(sizeof(MyStruct) == N)` on the Host to mathematically guarantee Host-Device ABI alignment before compilation.

#### Updated Optimization Toolbox content

**Added:**
```markdown
| [Sub-Buffers](10_Sub_Buffers_Partitioning/SubBuffers.md) | VRAM limits exceeded / High CPU overhead from manual `memcpy` chunking | `10_Sub_Buffers_Partitioning/` |
```

**Modified (Advanced challenges nested into symptoms):**
```markdown
| [Local Memory](01_Local_Memory/LocalMemory.md) | Kernel re-reads same global data repeatedly *(Incl. Bank Conflicts)* | `01_Local_Memory/` |
| [Coalesced Access](02_Coalesced_Access/CoalescedAccess.md) | Kernel slow despite simple logic *(Incl. AoS vs. SoA)* | `02_Coalesced_Access/` |
| [Work-Group Sizing](14_Work_Group_Sizing/WorkGroupSizing.md) | GPU underutilized, low occupancy *(Incl. Register Pressure)* | `14_Work_Group_Sizing/` |
``` 

*(All other 13 tools remain exactly the same as your original list).*

#### Quality assesment 
Grading Pass — Run `/grade-module` on all 7 top-level modules (and key submodules).