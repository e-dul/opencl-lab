# Task 037: C2 — Costmap Inflation

## Context
- **Design Feature:** `workflow/design/06-robotics-ros2-projects.md`
- **Milestone:** Phase 2 — C2 Costmap Inflation
- **Relevant Files:**
  - `workflow/design/06-robotics-ros2-projects.md` — (read-only: source of truth)
  - `common/ocl_wrapper.hpp` — (read-only: `create_context()`, `OclContext`)
  - `common/opencl_utils.hpp` — (read-only: `CL_CHECK`, `build_program`, `duration_ms`, `get_binary_dir`)
  - `common/image_utils.hpp` — (read-only: BMP/PNG save helpers)
  - `common/common.cmake` — (read-only: CLI11, include paths)
  - `02_Projects/C_Robotics_ROS2/C1_Node_Acceleration/CMakeLists.txt` — (read-only: reference for ROS 2 CMake pattern)
  - `02_Projects/C_Robotics_ROS2/C2_Costmap_Inflation/CMakeLists.txt` — (new file)
  - `02_Projects/C_Robotics_ROS2/C2_Costmap_Inflation/main.cpp` — (new file, binary: `costmap_node`)
  - `02_Projects/C_Robotics_ROS2/C2_Costmap_Inflation/map_publisher.cpp` — (new file, compiled into `costmap_node`)
  - `02_Projects/C_Robotics_ROS2/C2_Costmap_Inflation/kernels/inflate.cl` — (new file)
  - `02_Projects/C_Robotics_ROS2/C2_Costmap_Inflation/kernels/inflate_tiled.cl` — (new file)
  - `assets/` — (add `warehouse.pgm`: 512×512 synthetic occupancy grid)

## Objective

Implement `C2_Costmap_Inflation`: a self-contained ROS 2 binary (`costmap_node`) that receives a `nav_msgs/OccupancyGrid` from an intra-process `MapPublisher`, runs CPU exact Euclidean DT + GPU naive `inflate` + GPU tiled `inflate_tiled` kernels, prints a timing comparison table, saves `output_costmap.bmp`, and publishes `/inflated_costmap`.

## Constraints & Rules

All standard constraints from `.claude/rules/00_master_specs.md` apply. Task-specific constraints:

- **Phase 3 (tiled kernel) is NOT in scope for this task.** Implement only the CPU reference and the GPU naive `inflate` kernel. The `inflate_tiled.cl` file must be created as a stub (identical to `inflate.cl` body but with `__local` tile placeholder + a TODO comment). The timing table must have the tiled column present but populated with 0.000 ms and a `[STUB]` marker. Phase 3 (C2 Challenge) will be a separate task.
- **C2 Standalone Mode**: binary spins `CostmapNode` + `MapPublisher` in a single-threaded executor with intra-process comm enabled. No external ROS 2 peers required.
- **Node parameters (`declare_parameter`), not CLI11**: `CostmapNode` declares `map_path` (string), `inflation_radius` (double, default 0.5), `resolution` (double, default 0.05), `decay` (double, default 3.0). CLI11 is not used for node params.
- **`map_path` default**: `""` (empty string). When `map_path` is empty, `MapPublisher` generates a synthetic 512×512 occupancy grid in memory (border walls + a few obstacle clusters) instead of reading a file. This keeps the binary runnable without `assets/warehouse.pgm`.
- **`warehouse.pgm` asset**: create a 512×512 PGM (ASCII or binary P5) synthetic occupancy grid in `assets/warehouse.pgm`. Obstacles = 0, free = 255. Used when `--ros-args -p map_path:=assets/warehouse.pgm` is passed.
- **ROS 2 distro guard**: CMakeLists.txt must check `$ENV{ROS_DISTRO}` and emit `message(FATAL_ERROR ...)` with human-readable instructions if unset, before any `find_package`.
- **CMake strictness**: `set(CMAKE_CXX_STANDARD 17)`, `set(CMAKE_CXX_EXTENSIONS OFF)` mandatory.
- **Kernel file copy rule**: kernels directory must be copied post-build via `add_custom_command ... POST_BUILD` per master spec §1.
- **`CL_QUEUE_PROFILING_ENABLE`**: mandatory on the `cl::CommandQueue` in `CostmapNode::on_configure()`.
- **`CL_CHECK` coverage**: `setArg`, `finish`, `enqueueNDRangeKernel`, `enqueueReadBuffer`, `enqueueWriteBuffer` — all wrapped.
- **CPU reference DT algorithm**: exact Euclidean distance transform — for each free cell, iterate over all obstacle cells within `inflation_radius` (in pixels = `inflation_radius / resolution`), find minimum Euclidean distance, map to cost 0–255 via exponential decay: `cost = 255 * exp(-decay * dist_m)`. This is the correctness oracle.
- **GPU naive `inflate` kernel**: same algorithm expressed as a 2D NDRange — each work-item processes one output cell. Global memory only (no `__local`). `point_step` is not applicable here (this is a 2D grid, not a point cloud).
- **GPU result correctness check**: compare GPU result against CPU reference; if any cell deviates by more than ±1 cost unit (0–255 scale), print a warning with the cell coordinates and max deviation — do not hard-fail, just warn.
- **BMP colorization** (CPU, after GPU readback): obstacles (input value 100 in `OccupancyGrid`) → RGBA black (0,0,0,255). Inflated cells (cost > 0, not obstacle) → red gradient: R=255, G=0, B=0, A=cost. Free cells (cost == 0) → white (255,255,255,255). Save via `stb_image_write`.
- **`OccupancyGrid` convention**: value 100 = obstacle, 0 = free, -1 = unknown (treat as free for inflation purposes).
- **Published `/inflated_costmap`**: `nav_msgs/OccupancyGrid` with the GPU naive result, same metadata as input.
- **Timing units**: GPU via `cl::Event` profiling (ns → ms). CPU via `std::chrono::steady_clock`. All in ms to 3 decimal places.
- **Integer overflow guard**: buffer size = `width * height` bytes. Use `static_cast<size_t>(width) * height` (master spec §7.1). Throw `std::runtime_error` if `width * height > INT_MAX`.
- **RAII cleanup in `on_cleanup()`**: assign default-constructed values to all `cl::` members (`queue_ = cl::CommandQueue(); ctx_ = cl::Context();` etc.).
- **No OpenCL 2.0 features**: this path is OpenCL 1.2 baseline only.

---

## Implementation

1. **`assets/warehouse.pgm`** — create a 512×512 P5 binary PGM file. Content: white background (255) with a rectangular border of obstacles (0) and 3–4 rectangular obstacle clusters inside. This provides a realistic costmap test case.

2. **`CMakeLists.txt`** — new file at `02_Projects/C_Robotics_ROS2/C2_Costmap_Inflation/CMakeLists.txt`.

   Required structure:
   ```cmake
   cmake_minimum_required(VERSION 3.18)
   project(c2_costmap_inflation)

   # ROS 2 distro guard — must precede find_package
   if(NOT DEFINED ENV{ROS_DISTRO})
     message(FATAL_ERROR "ROS_DISTRO is not set. Run: source /opt/ros/jazzy/setup.bash")
   endif()

   set(CMAKE_CXX_STANDARD 17)
   set(CMAKE_CXX_EXTENSIONS OFF)

   include(../../common/common.cmake)

   find_package(ament_cmake REQUIRED)
   find_package(rclcpp REQUIRED)
   find_package(rclcpp_lifecycle REQUIRED)
   find_package(nav_msgs REQUIRED)
   find_package(lifecycle_msgs REQUIRED)
   find_package(OpenCL REQUIRED)

   add_executable(costmap_node
     main.cpp
     map_publisher.cpp
   )

   target_include_directories(costmap_node PRIVATE
     ${CMAKE_CURRENT_SOURCE_DIR}/../../common
     ${CMAKE_CURRENT_SOURCE_DIR}/../../vendor
   )

   ament_target_dependencies(costmap_node
     rclcpp rclcpp_lifecycle nav_msgs lifecycle_msgs
   )

   target_link_libraries(costmap_node OpenCL::OpenCL CLI11::CLI11)

   # Kernel copy rule (master spec §1)
   add_custom_command(TARGET costmap_node POST_BUILD
     COMMAND ${CMAKE_COMMAND} -E copy_directory
             ${CMAKE_CURRENT_SOURCE_DIR}/kernels
             $<TARGET_FILE_DIR:costmap_node>/kernels
     COMMENT "Copying kernels"
   )
   ```

3. **`kernels/inflate.cl`** — GPU naive distance transform kernel.

   Kernel signature:
   ```c
   __kernel void inflate(
     __global const uchar* input,   // obstacle map: 1=obstacle, 0=free
     __global uchar* output,        // cost map: 0–255
     int width,
     int height,
     int radius_px,                 // inflation_radius / resolution, rounded
     float decay,
     float resolution
   )
   ```
   Each work-item = one output cell `(gx, gy)`. Iterate over all cells in `[-radius_px, radius_px]` box; compute Euclidean distance to obstacle cells; apply `cost = 255 * exp(-decay * dist_m)` for the nearest obstacle within radius. If the cell itself is an obstacle, output = 255 (max cost).

   Guard: `if (gx >= width || gy >= height) return;`

4. **`kernels/inflate_tiled.cl`** — stub file only for this task.

   ```c
   // TODO (Task 038 — C2 Challenge): Replace with __local tiled implementation.
   // Currently delegates to the same logic as inflate.cl.
   __kernel void inflate_tiled(
     __global const uchar* input,
     __global uchar* output,
     int width,
     int height,
     int radius_px,
     float decay,
     float resolution
   )
   {
     // Stub: identical to inflate kernel. Tiled optimization pending.
     int gx = (int)get_global_id(0);
     int gy = (int)get_global_id(1);
     if (gx >= width || gy >= height) return;
     // ... same body as inflate.cl ...
   }
   ```

5. **`map_publisher.cpp`** — `MapPublisher : public rclcpp::Node`.

   - Constructor parameter: `std::string map_path`.
   - `NodeOptions().use_intra_process_comms(true)`.
   - If `map_path` is empty: generate 512×512 synthetic grid (border obstacles + clusters) in memory.
   - If `map_path` is non-empty: load via `stb_image` (single channel). Threshold: pixel < 128 → obstacle (100), else free (0).
   - Convert to `nav_msgs::msg::OccupancyGrid`. Set `info.resolution`, `info.width`, `info.height`.
   - Publish once to `/map` on a one-shot timer (10 ms delay to allow `CostmapNode` subscription to activate first).
   - `MapPublisher` does not trigger shutdown — `CostmapNode` handles shutdown after processing.

6. **`main.cpp`** — `CostmapNode : public rclcpp_lifecycle::LifecycleNode` + `main()`.

   `CostmapNode` members:
   ```
   OclContext ocl_;
   cl::CommandQueue queue_;
   cl::Program program_naive_, program_tiled_;
   cl::Kernel inflate_kernel_, inflate_tiled_kernel_;
   // Parameters
   std::string map_path_;
   double inflation_radius_, resolution_, decay_;
   // ROS 2
   rclcpp::Subscription<nav_msgs::msg::OccupancyGrid>::SharedPtr sub_;
   rclcpp_lifecycle::LifecyclePublisher<nav_msgs::msg::OccupancyGrid>::SharedPtr pub_;
   ```

   `on_configure()`:
   - `declare_parameter("map_path", "")`, `declare_parameter("inflation_radius", 0.5)`, etc.
   - Get parameter values.
   - `create_context()` → `cl::CommandQueue(ctx, device, CL_QUEUE_PROFILING_ENABLE)`.
   - `build_program()` for both `inflate.cl` and `inflate_tiled.cl`.
   - Log context init time via `steady_clock`.

   `on_activate()`:
   - `create_subscription("/map", ...)` with `rclcpp::QoS(1).transient_local()`.
   - `pub_ = create_publisher(...)` → `pub_->on_activate()`.

   Subscription callback (`on_map_received`):
   - Extract `width`, `height` from `OccupancyGrid::info`.
   - Integer overflow guard on `static_cast<size_t>(width) * height`.
   - Build obstacle map (uchar: 1=obstacle, 0=free) from `OccupancyGrid::data` (100 → 1, else → 0).
   - Compute `radius_px = static_cast<int>(std::round(inflation_radius_ / resolution_))`.

   CPU path (timed via `steady_clock`):
   - Exact Euclidean DT: for each cell, scan `[-radius_px, radius_px]` box, find nearest obstacle, apply cost formula.

   GPU naive path:
   - `cl::Buffer input_buf(ctx, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, ...)`.
   - `cl::Buffer output_buf(ctx, CL_MEM_WRITE_ONLY, ...)`.
   - `CL_CHECK(inflate_kernel_.setArg(0, input_buf))` ... all args.
   - `cl::Event naive_event`.
   - `CL_CHECK(queue_.enqueueNDRangeKernel(inflate_kernel_, cl::NullRange, cl::NDRange(width, height), cl::NullRange, nullptr, &naive_event))`.
   - `CL_CHECK(queue_.finish())`.
   - Read back result.

   GPU tiled path (stub — same kernel, record separately):
   - Dispatch `inflate_tiled_kernel_` identically. Record `cl::Event` → compute time.
   - Print `[STUB]` next to the tiled time in the table.

   Correctness check: compare GPU naive result vs CPU reference. Log max deviation.

   BMP colorization + save `output_costmap.bmp`.

   Publish `/inflated_costmap`.

   Print timing table:
   ```
   +-------------------------------------------+
   |  C2 Costmap Inflation                     |
   +------------------+------------------------+
   | CPU DT           |        XX.XXX ms       |
   | GPU naive        |         X.XXX ms       |
   | GPU tiled        |         X.XXX ms [STUB]|
   | GPU-vs-CPU       |          XX.XXx speedup|
   | Tiled-vs-naive   |           X.XXx speedup|
   +------------------+------------------------+
   ```

   After printing table: `trigger_transition(TRANSITION_DEACTIVATE)` → `trigger_transition(TRANSITION_CLEANUP)` → `rclcpp::shutdown()`.

   `on_deactivate()`: reset subscription, deactivate publisher.
   `on_cleanup()`: assign default-constructed values to all `cl::` members.

   `main()`:
   - `rclcpp::init(argc, argv)`.
   - Read `map_path` from node parameters (pass through ROS args).
   - Create `CostmapNode` + `MapPublisher` with `NodeOptions().use_intra_process_comms(true)`.
   - `SingleThreadedExecutor` → add both nodes.
   - Drive `CostmapNode` through `CONFIGURE` → `ACTIVATE`.
   - `executor.spin()`.
   - `return 0`.

---

## Definition of Done (DoD)

Standard DoD from master spec §8 applies. Task-specific items:

- [x] `source /opt/ros/jazzy/setup.bash && cmake -B build && cmake --build build` succeeds with zero errors and zero warnings from within `02_Projects/C_Robotics_ROS2/C2_Costmap_Inflation/`.
- [x] Binary is named `costmap_node` (`./build/costmap_node`).
- [x] `./build/costmap_node` (no args, synthetic grid) runs to completion and exits with code 0.
- [x] `GPU=<vendor> ./build/costmap_node` selects the correct device without crashing.
- [x] `output_costmap.bmp` is created in the working directory after a successful run.
- [x] Console prints exactly one `[INIT] Context init:` line.
- [x] Console prints the timing comparison table with CPU, GPU naive, GPU tiled (`[STUB]`), speedup columns.
- [x] GPU naive result correctness check runs; max deviation is logged (must be ≤ 1 cost unit, or a warning is printed).
- [x] `assets/warehouse.pgm` file exists (512×512, valid PGM format).
- [x] Running with `--ros-args -p map_path:=assets/warehouse.pgm` from the repo root completes without error and produces `output_costmap.bmp`.
- [x] MANUAL: Open `output_costmap.bmp` — obstacles appear black, inflated zone is a red gradient, free space is white.
- [x] MANUAL: Inspect timing table — GPU naive time is reported in ms; CPU DT time is larger than GPU naive on dedicated GPU hardware (hardware-waiver applies for CPU fallback / iGPU).

---

## Execution Report

- **Status:** DONE
- **Session:** 2026-03-17

### Validation
```
# Build
source /opt/ros/jazzy/setup.bash && cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -- -j4
[  0%] Built target CLI11
[100%] Built target costmap_node
# Zero errors, zero warnings.

# Run (no args, synthetic grid)
./build/costmap_node
[INFO] [costmap_node]: [INIT] Context init: 210.793 ms
[INFO] [map_publisher]: [MAP] Published synthetic 512×512 grid.
[INFO] [costmap_node]: [OK] GPU vs CPU max deviation: 0 cost units
[INFO] [costmap_node]: [BMP] Saved output_costmap.bmp (512x512).
[INFO] [costmap_node]: [DONE] OpenCL resources released.
Platform : NVIDIA CUDA
Device   : NVIDIA GeForce RTX 4060 Laptop GPU
+-------------------------------------------+
|  C2 Costmap Inflation                     |
+------------------+------------------------+
| CPU DT           |         47.843 ms        |
| GPU naive        |          0.227 ms        |
| GPU tiled        |          0.000 ms [STUB] |
| GPU-vs-CPU       |        210.459x speedup  |
| Tiled-vs-naive   |          0.000x speedup  |
+------------------+------------------------+
Exit code: 0

# Run with warehouse.pgm
./costmap_node --ros-args -p map_path:=assets/warehouse.pgm
[INFO] [costmap_node]: [INIT] Context init: 171.487 ms
[INFO] [map_publisher]: [MAP] Published grid from 'assets/warehouse.pgm' (512x512).
[INFO] [costmap_node]: [OK] GPU vs CPU max deviation: 0 cost units
[INFO] [costmap_node]: [BMP] Saved output_costmap.bmp (512x512).
Exit code: 0

# GPU vendor selection
GPU=NVIDIA ./costmap_node
Platform : NVIDIA CUDA  [GPU=NVIDIA]
Device   : NVIDIA GeForce RTX 4060 Laptop GPU
Exit code: 0

# output_costmap.bmp
-rw-rw-r-- 1 emil emil 1048698 Mar 17 17:43 <repo>/output_costmap.bmp
```

### Changed Files
| File | Change |
|------|--------|
| `assets/warehouse.pgm` | Created — 512×512 synthetic occupancy grid |
| `02_Projects/C_Robotics_ROS2/C2_Costmap_Inflation/CMakeLists.txt` | Created |
| `02_Projects/C_Robotics_ROS2/C2_Costmap_Inflation/main.cpp` | Created |
| `02_Projects/C_Robotics_ROS2/C2_Costmap_Inflation/map_publisher.cpp` | Created |
| `02_Projects/C_Robotics_ROS2/C2_Costmap_Inflation/kernels/inflate.cl` | Created |
| `02_Projects/C_Robotics_ROS2/C2_Costmap_Inflation/kernels/inflate_tiled.cl` | Created (stub) |

### Remaining
- [ ] Phase 3 (C2 Challenge — tiled kernel) deferred to Task 038.
