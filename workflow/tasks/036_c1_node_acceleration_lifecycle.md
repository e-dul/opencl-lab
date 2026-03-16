# Task 036: C1 — Node Acceleration (LifecycleNode Rebuild)

## Context
- **Design Feature:** `workflow/design/06-robotics-ros2-projects.md`
- **Milestone:** Phase 1 — C1 Node Acceleration
- **Relevant Files:**
  - `workflow/design/06-robotics-ros2-projects.md` — (read-only: source of truth)
  - `common/ocl_wrapper.hpp` — (read-only: `create_context()`, `OclContext`)
  - `common/opencl_utils.hpp` — (read-only: `CL_CHECK`, `build_program`, `duration_ms`, `get_binary_dir`)
  - `common/common.cmake` — (read-only: CLI11 integration, include paths)
  - `02_Projects/C_Robotics_ROS2/C1_Node_Acceleration/main.cpp` — (replace entirely)
  - `02_Projects/C_Robotics_ROS2/C1_Node_Acceleration/CMakeLists.txt` — (modify: add `rclcpp_lifecycle`, `std_msgs`; remove CLI11 link for node args)
  - `02_Projects/C_Robotics_ROS2/C1_Node_Acceleration/kernels/passthrough.cl` — (keep as-is)

## Objective

Rebuild `C1_Node_Acceleration` so `AccelNode` is a `rclcpp_lifecycle::LifecycleNode` with a companion `SyntheticPublisher` node running in the same single-threaded executor via intra-process communication, replacing the simulated-loop implementation from task 035 which diverged from the design doc.

## Constraints & Rules

All standard constraints from `.claude/rules/00_master_specs.md` apply. Task-specific constraints:

- **Design wins**: the existing `main.cpp` uses `rclcpp::Node` + simulated loop. That is wrong. Replace it entirely to match the design.
- **LifecycleNode**: `AccelNode` must subclass `rclcpp_lifecycle::LifecycleNode`. Lifecycle callbacks: `on_configure`, `on_activate`, `on_deactivate`, `on_cleanup`. No override of `on_shutdown` needed.
- **Node parameters, not CLI11**: C1 nodes use `declare_parameter` (not CLI11 flags) for `iterations` (int, default 10) and `buffer_size` (int, default 1048576). This is idiomatic ROS 2. CLI11 is not used for node configuration in C1.
- **OpenCL init in `on_configure()`**: `create_context()`, `cl::CommandQueue` (with `CL_QUEUE_PROFILING_ENABLE`), kernel build, and `cl::Buffer` pre-allocation all happen in `on_configure()`. Log init time once via `std::chrono::steady_clock`.
- **Real pub/sub**: `AccelNode::on_activate()` creates a subscription on `/raw_floats` (`std_msgs/Float32MultiArray`) and a publisher on `/processed_floats`. Per-callback: `enqueueWriteBuffer` → `enqueueNDRangeKernel` → `enqueueReadBuffer` → publish result → log `cl::Event` dispatch time. After `iterations` callbacks, node self-transitions to deactivate.
- **SyntheticPublisher**: a separate `rclcpp::Node` subclass in the same `main.cpp`. A timer fires `iterations` times, publishing `Float32MultiArray` of `buffer_size` floats to `/raw_floats`. Intra-process comm delivers as shared pointer (zero serialization).
- **Intra-process comm**: both nodes must be registered with the same `rclcpp::IntraProcessManager`. Use `rclcpp::NodeOptions().use_intra_process_comms(true)` for both.
- **Single-threaded executor**: `rclcpp::executors::SingleThreadedExecutor`. Both nodes added. Executor spins until shutdown.
- **Lifecycle manager in `main()`**: after constructing both nodes and adding them to the executor, `main()` drives `AccelNode` through `configure` → `activate` before spinning. Use `accel_node->trigger_transition(lifecycle_msgs::msg::Transition::TRANSITION_CONFIGURE)` and `TRANSITION_ACTIVATE`. Wait for each transition to complete before the next.
- **Shutdown sequence**: after `iterations` callbacks, `AccelNode` calls `trigger_transition(TRANSITION_DEACTIVATE)` then `trigger_transition(TRANSITION_CLEANUP)` then `rclcpp::shutdown()`.
- **RAII in `on_cleanup()`**: destroy `cl::Kernel`, `cl::CommandQueue`, `cl::Buffer` objects by assigning default-constructed values (e.g., `queue_ = cl::CommandQueue();`). `cl::Context` destroyed last.
- **Buffer size guard**: in `on_configure()`, throw `std::runtime_error` if `buffer_size > INT_MAX` (master spec §7.1).
- **`CL_CHECK` coverage**: `setArg`, `finish`, `enqueueNDRangeKernel`, `enqueueReadBuffer`, `enqueueWriteBuffer` — all wrapped.
- **CMakeLists.txt additions**: add `rclcpp_lifecycle` and `std_msgs` to `find_package` and target link libraries. Add `lifecycle_msgs` for transition constants.
- **No BMP output**: C1 is a timing benchmark. Structured console log is the artifact.
- **`buffer_size` as kernel global work size**: `NDRange(static_cast<size_t>(buffer_size))`. Kernel signature unchanged.
- **ROS 2 distro guard**: CMakeLists.txt already checks `$ENV{ROS_DISTRO}` — keep it.

---

## Implementation

1. **`CMakeLists.txt` — extend dependencies**

   Add `rclcpp_lifecycle` and `std_msgs` and `lifecycle_msgs` to:
   - `find_package(rclcpp_lifecycle REQUIRED)`
   - `find_package(std_msgs REQUIRED)`
   - `find_package(lifecycle_msgs REQUIRED)`
   - `ament_target_dependencies(accel_node rclcpp rclcpp_lifecycle std_msgs lifecycle_msgs)`
   - Keep `find_package(OpenCL REQUIRED)` and `find_package(rclcpp REQUIRED)` as-is.
   - Keep `target_link_libraries(accel_node OpenCL::OpenCL CLI11::CLI11)`.
   - Remove CLI11 from node argument handling (keep the library linked, just not used for node params).
   - Rename executable from `c1_node_acceleration` to `accel_node` (matches design doc binary name: `accel_node`).

2. **`main.cpp` — full replacement**

   Structure:
   ```
   // Includes: rclcpp, rclcpp_lifecycle, lifecycle_msgs, std_msgs/msg/float32_multi_array.hpp,
   //           ocl_wrapper.hpp, opencl_utils.hpp, <chrono>, <filesystem>, <iomanip>, etc.

   // AccelNode : public rclcpp_lifecycle::LifecycleNode
   //   Members: OclContext ocl_; cl::CommandQueue queue_; cl::Kernel kernel_;
   //            cl::Buffer src_buf_, dst_buf_;
   //            int iterations_, buffer_size_, callback_count_;
   //            double init_ms_;
   //            std::vector<double> dispatch_times_;
   //            rclcpp::Subscription<std_msgs::msg::Float32MultiArray>::SharedPtr sub_;
   //            rclcpp_lifecycle::LifecyclePublisher<std_msgs::msg::Float32MultiArray>::SharedPtr pub_;
   //
   //   on_configure(): declare_parameter, create_context, queue, kernel, buffers. Log init time.
   //   on_activate():  create_subscription("/raw_floats"), create_publisher("/processed_floats").
   //                   activate pub_.
   //   on_deactivate(): destroy sub_, deactivate pub_. Print summary table.
   //   on_cleanup():   assign default-constructed values to all cl:: members.
   //
   //   callback(msg): enqueueWriteBuffer → enqueueNDRangeKernel → enqueueReadBuffer →
   //                  publish → log dispatch time.
   //                  if (++callback_count_ >= iterations_) trigger deactivate/cleanup/shutdown.

   // SyntheticPublisher : public rclcpp::Node
   //   Constructor: declare timer, NodeOptions().use_intra_process_comms(true).
   //   Timer callback: publish Float32MultiArray(buffer_size floats = 1.0f) to "/raw_floats".
   //                   Stops after iterations_ publishes.

   // main():
   //   rclcpp::init(argc, argv)
   //   auto exec = std::make_shared<rclcpp::executors::SingleThreadedExecutor>()
   //   auto accel = std::make_shared<AccelNode>(NodeOptions().use_intra_process_comms(true))
   //   auto synth = std::make_shared<SyntheticPublisher>(accel->iterations_, accel->buffer_size_,
   //                                                     NodeOptions().use_intra_process_comms(true))
   //   exec->add_node(accel->get_node_base_interface())
   //   exec->add_node(synth)
   //   accel->trigger_transition(Transition::TRANSITION_CONFIGURE); exec->spin_some()
   //   accel->trigger_transition(Transition::TRANSITION_ACTIVATE);  exec->spin_some()
   //   exec->spin()   // runs until rclcpp::shutdown()
   //   return 0
   ```

   Summary table (printed from `on_deactivate()`):
   ```
   +----------------------------------+
   |  C1 Node Acceleration Summary    |
   +--------------+-------------------+
   | Context init | XXXXXXX.XXX ms    |
   | CB avg       |       X.XXX ms    |
   | CB min       |       X.XXX ms    |
   | CB max       |       X.XXX ms    |
   +--------------+-------------------+
   ```

3. **`kernels/passthrough.cl`** — no changes needed.

---

## Definition of Done (DoD)

Standard DoD from master spec §8 applies. Task-specific items:

- [ ] `source /opt/ros/jazzy/setup.bash && cmake -B build && cmake --build build` succeeds with zero errors and zero warnings from within `02_Projects/C_Robotics_ROS2/C1_Node_Acceleration/`.
- [ ] Binary is named `accel_node` (`./build/accel_node`).
- [ ] `./build/accel_node --help` exits with code 0 (rclcpp handles `--help` or exits cleanly; CLI11 no longer drives node params).
- [ ] `./build/accel_node` (no args, default parameters) runs 10 subscription callbacks via intra-process comm and exits with code 0.
- [ ] Console output shows exactly one `[INIT] Context init:` line.
- [ ] Console output shows exactly 10 `[CB  N] Dispatch:` lines from real subscription callbacks (not a simulated loop).
- [ ] Summary table is printed from `on_deactivate()`.
- [ ] `GPU=<vendor> ./build/accel_node` selects the correct device without crashing.
- [ ] MANUAL: Inspect console — exactly one context init line appears before any callback line, proving one-time cost.
- [ ] MANUAL: Inspect `[CB N] Dispatch:` values — all are ≤ 0.5 ms on the target GPU (hardware-waiver applies for CPU fallback / integrated GPU).

---

## Execution Report

- **Status:** PENDING
- **Session:** [YYYY-MM-DD]

### Validation
```
[output here]
```

### Changed Files
| File | Change |
|------|--------|
| `02_Projects/C_Robotics_ROS2/C1_Node_Acceleration/main.cpp` | Replaced — LifecycleNode + SyntheticPublisher + intra-process comm |
| `02_Projects/C_Robotics_ROS2/C1_Node_Acceleration/CMakeLists.txt` | Modified — add rclcpp_lifecycle, std_msgs, lifecycle_msgs; rename binary to accel_node |

### Remaining
- [ ] Implementation pending.
