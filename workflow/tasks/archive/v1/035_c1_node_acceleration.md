# Task 035: C1 — Node Acceleration

## Context
- **Design Feature:** `workflow/design/06-robotics-ros2-projects.md`
- **Milestone:** Phase 1 — C1 Node Acceleration
- **Relevant Files:**
  - `workflow/design/06-robotics-ros2-projects.md` — (read-only: architecture source of truth)
  - `common/ocl_wrapper.hpp` — (read-only: `create_context()`, `OclContext`)
  - `common/opencl_utils.hpp` — (read-only: `CL_CHECK`, `CL_HPP_ENABLE_EXCEPTIONS`)
  - `common/common.cmake` — (read-only: CLI11 integration, include paths)
  - `02_Projects/C_Robotics_ROS2/SETUP.md` — (read-only: ROS 2 install reference)
  - `02_Projects/C_Robotics_ROS2/C1_Node_Acceleration/` — (new directory)

## Objective

Implement `C1_Node_Acceleration` with two explicit goals:

1. **ROS 2 setup verification**: C1 is the first ROS 2 binary in the module. It must exercise `find_package(rclcpp)`, `rclcpp::init`, and `rclcpp::Node` construction so that environment issues (missing `setup.bash`, wrong RMW, broken install) surface here — not in C3 where the full pipeline is at stake.

2. **Correct OpenCL context lifecycle**: `cl::Context`, `cl::CommandQueue`, and the passthrough kernel must be owned as members of `AccelNode` and initialized in the constructor — exactly where a real subscriber node would initialize them. RAII teardown in the destructor. This is the pattern C3 inherits; C1 establishes it without the complexity of real callbacks.

The binary simulates `--iterations` callbacks in a plain loop to prove context init cost is paid once (constructor) and per-callback dispatch is ≤ 0.5 ms flat.

## Constraints & Rules

All standard constraints from `.claude/rules/00_master_specs.md` apply (C++17, `cl.hpp`, CLI11, `create_context()`, `cl::Event` profiling, `CL_CHECK`, standalone CMake, kernel copy rule). Additional constraints for this task:

- **No daemon required**: ROS 2 has no master process — DDS handles discovery. The binary runs standalone without a ROS 2 daemon or active DDS network. Callbacks are simulated internally in a loop, not driven by `ros2 run` or `ros2 topic`. `rclcpp::init` / `rclcpp::shutdown` are called but no spinner is needed.
- **ROS 2 guard**: `CMakeLists.txt` must check `$ENV{ROS_DISTRO}` before `find_package(rclcpp)` and emit `message(FATAL_ERROR "source /opt/ros/jazzy/setup.bash first")` if unset.
- **ROS 2 version**: Jazzy. Use `find_package(rclcpp REQUIRED)`.
- **No BMP output**: C1 is a timing benchmark — structured console table is the required artifact (master spec §3 numeric-tool exception).
- **Passthrough kernel**: copies exactly 1M `float` elements (device → device). Must not be a no-op — drivers cannot elide a real memory copy.
- **Kernel profiling**: `cl::CommandQueue` must be created with `CL_QUEUE_PROFILING_ENABLE`. All dispatch times via `cl::Event` (`CL_PROFILING_COMMAND_START` / `CL_PROFILING_COMMAND_END`). Context init time via `std::chrono::steady_clock`.
- **No BMP, no image dependency**: do not link `stb_image` or `image_utils.hpp` here.
- **CLI flag**: `--iterations` (int, default 10). Use CLI11.
- **GPU selection**: via `GPU` env var only — `create_context()` from `common/ocl_wrapper.hpp`. No `--device` index flag.

---

## Implementation

1. **Directory scaffold**

   Create `02_Projects/C_Robotics_ROS2/C1_Node_Acceleration/` with:
   - `CMakeLists.txt`
   - `main.cpp`
   - `kernels/passthrough.cl`

2. **Kernel (`kernels/passthrough.cl`)**

   Single kernel `passthrough`:
   ```cl
   __kernel void passthrough(__global const float* src, __global float* dst, int n) {
       size_t gid = get_global_id(0);
       if (gid < (size_t)n) dst[gid] = src[gid];
   }
   ```
   Global work size: 1M (1 048 576). No local size hint needed (driver default).

3. **`AccelNode` class (in `main.cpp`)**

   - Inherits `rclcpp::Node`. Constructor name string: `"accel_node"`.
   - Constructor body (runs once before any callback):
     1. `steady_clock` start → `create_context()` → `steady_clock` stop. Print `[INIT] Context init: X.XXX ms`.
     2. Create `cl::CommandQueue` with `CL_QUEUE_PROFILING_ENABLE` on `ocl_.context` / `ocl_.device`. Store as member.
     3. Load and build `kernels/passthrough.cl` relative to the binary's location (use `std::filesystem::path` from `argv[0]` or `__FILE__` — prefer passing kernel path via a member set before `AccelNode` construction, or use `ros2 pkg prefix` fallback). Simplest correct approach: resolve kernel path as `std::filesystem::canonical(std::filesystem::path(argv[0]).parent_path() / "kernels" / "passthrough.cl")` and pass it as a constructor argument.
     4. Allocate `cl::Buffer src_buf` and `cl::Buffer dst_buf` (1M floats each, `CL_MEM_READ_WRITE`). Fill `src_buf` with `1.0f` via `enqueueWriteBuffer`. These are members — allocated once.
   - Method `simulate_callback(int iter)`:
     1. Set kernel args: `src_buf`, `dst_buf`, `cl_int(1'048'576)`. Wrap each `setArg` in `CL_CHECK`.
     2. `cl::Event ev`. `CL_CHECK(queue_.enqueueNDRangeKernel(..., &ev))`. `CL_CHECK(queue_.finish())`.
     3. Compute dispatch time from `ev` profiling info. Print `[CB %2d] Dispatch: X.XXX ms`.
   - Destructor: RAII handles `cl::Kernel`, `cl::CommandQueue`, `cl::Buffer` — no explicit release needed. Print `[DONE] Node destroyed.`.

4. **`main()` function**

   ```
   rclcpp::init(argc, argv);
   // Parse CLI with CLI11 (--iterations)
   // Construct AccelNode(kernel_path, iterations) — pass kernel_path derived from argv[0]
   // Loop iterations: node.simulate_callback(i)
   // Print summary table
   rclcpp::shutdown();
   ```

   Summary table printed after loop:
   ```
   ┌─────────────────────────────────┐
   │  C1 Node Acceleration Summary   │
   ├──────────────┬──────────────────┤
   │ Context init │   X.XXX ms       │
   │ CB avg       │   X.XXX ms       │
   │ CB min       │   X.XXX ms       │
   │ CB max       │   X.XXX ms       │
   └──────────────┴──────────────────┘
   ```
   (Plain ASCII box acceptable — Unicode box only if `std::cout` supports it.)

5. **`CMakeLists.txt`**

   - Check `$ENV{ROS_DISTRO}` → fatal error if unset.
   - `cmake_minimum_required(VERSION 3.18)`, `project(c1_node_acceleration)`.
   - `set(CMAKE_CXX_STANDARD 17)`, `set(CMAKE_CXX_EXTENSIONS OFF)`.
   - `find_package(OpenCL REQUIRED)`, `find_package(rclcpp REQUIRED)`.
   - Include `common/common.cmake` (adjust relative path: `../../common/common.cmake`).
   - `add_executable(c1_node_acceleration main.cpp)`.
   - Link: `OpenCL::OpenCL`, `rclcpp::rclcpp`, `CLI11::CLI11`.
   - Include dirs: `../../common`, `../../vendor`.
   - Kernel copy post-build rule (master spec §1):
     ```cmake
     add_custom_command(TARGET c1_node_acceleration POST_BUILD
       COMMAND ${CMAKE_COMMAND} -E copy_directory
               ${CMAKE_CURRENT_SOURCE_DIR}/kernels
               $<TARGET_FILE_DIR:c1_node_acceleration>/kernels
       COMMENT "Copying kernels")
     ```
   - `target_compile_definitions`: `CL_HPP_ENABLE_EXCEPTIONS`, `CL_HPP_TARGET_OPENCL_VERSION=120`, `CL_HPP_MINIMUM_OPENCL_VERSION=120`.

---

## Definition of Done (DoD)

Standard DoD from master spec §8 applies. Task-specific items:

- [x] `source /opt/ros/jazzy/setup.bash && cmake -B build && cmake --build build` succeeds with zero errors and zero warnings from within `02_Projects/C_Robotics_ROS2/C1_Node_Acceleration/`.
- [x] `./build/c1_node_acceleration --help` prints CLI11 usage including `--iterations`.
- [x] `./build/c1_node_acceleration` (no args) runs 10 simulated callbacks and exits with code 0.
- [x] Console output shows exactly one `[INIT] Context init:` line (context init is a one-time cost).
- [x] Console output shows 10 `[CB  N] Dispatch:` lines (one per callback).
- [x] Summary table is printed after all callbacks.
- [x] `./build/c1_node_acceleration --iterations 5` runs exactly 5 callbacks.
- [x] `GPU=<vendor> ./build/c1_node_acceleration` selects the correct device without crashing.
- [ ] MANUAL: Inspect console — context init time is visibly larger than any single callback dispatch time (proving the one-time cost point).
- [ ] MANUAL: Inspect `[CB N] Dispatch:` values — all are ≤ 0.5 ms on the target GPU (hardware-waiver applies for CPU fallback / integrated GPU).

---

## Execution Report

- **Status:** PASSED
- **Session:** 2026-03-16

### Validation
```
Build: source /opt/ros/jazzy/setup.bash && cmake -B build && cmake --build build
  → rclcpp 28.1.16 / jazzy found. 0 errors, 0 warnings.

./build/c1_node_acceleration --help
  → Prints CLI11 usage with --iterations INT:POSITIVE. EXIT: 0

./build/c1_node_acceleration
  → Platform: NVIDIA CUDA / RTX 4060 Laptop GPU
  → [INIT] Context init: 188.790 ms  (one-time cost)
  → [CB  1] Dispatch: 0.020 ms
     ...
  → [CB 10] Dispatch: 0.011 ms
  → Summary table printed.
  → [DONE] Node destroyed.
  → EXIT: 0

./build/c1_node_acceleration --iterations 5
  → Exactly 5 [CB N] lines printed. EXIT: 0

GPU=NVIDIA ./build/c1_node_acceleration
  → Platform: NVIDIA CUDA [GPU=NVIDIA] selected. EXIT: 0
```

### Changed Files
| File | Change |
|------|--------|
| `02_Projects/C_Robotics_ROS2/C1_Node_Acceleration/CMakeLists.txt` | Created |
| `02_Projects/C_Robotics_ROS2/C1_Node_Acceleration/main.cpp` | Created |
| `02_Projects/C_Robotics_ROS2/C1_Node_Acceleration/kernels/passthrough.cl` | Created |

### Remaining
- None — all agent-verifiable DoD items pass. MANUAL items left for human review.
