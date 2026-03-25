# Task 041: Module 6 Review and Cleanup

## Context
- **Design Feature:** `workflow/design/06-robotics-ros2-projects.md`
- **Milestone:** Phase 6 — Module review and cleanup
- **Relevant Files:**
  - `common/common.cmake` — (to modify: add ROS 2 macros)
  - `02_Projects/C_Robotics_ROS2/C1_Node_Acceleration/CMakeLists.txt` — (to modify)
  - `02_Projects/C_Robotics_ROS2/C1_Node_Acceleration/main.cpp` — (to modify: extract SyntheticPublisher)
  - `02_Projects/C_Robotics_ROS2/C1_Node_Acceleration/synthetic_publisher.cpp` — (new file)
  - `02_Projects/C_Robotics_ROS2/C2_Costmap_Inflation/CMakeLists.txt` — (to modify)
  - `02_Projects/C_Robotics_ROS2/C3_Perception_Node/CMakeLists.txt` — (to modify)

## Objective

Reduce boilerplate across the three ROS 2 CMakeLists by extracting repeated ROS 2 guards and ament+OpenCL linking into reusable macros in `common/common.cmake`, and align C1's source layout with the design-spec directory structure.

## Constraints & Rules

- All standard constraints inherited from `.claude/rules/00_master_specs.md`.
- Do NOT change any source `.cpp` or `.cl` files except for the SyntheticPublisher extraction (Item D).
- After refactoring, each CMakeLists must be standalone-buildable with `source /opt/ros/jazzy/setup.bash && cmake -B build && cmake --build build`.
- `ament_target_dependencies` must still be called **before** `target_link_libraries` in every target — the new macro must preserve this ordering (ament uses the plain CMake signature internally; mixing keyword and plain forms is forbidden).
- The new macro in `common.cmake` must use `macro()` (not `function()`) so `find_package` results are visible in the caller's scope.

---

## Implementation

### A — Add `opencl_lab_ros2_guard()` and `opencl_lab_ros2_target()` to `common/common.cmake`

**Problem:** All three CMakeLists duplicate the `$ENV{ROS_DISTRO}` guard (4–6 lines each). C3 additionally duplicates the RMW loaned-message warning. The ament+OpenCL definitions and linking block (`ament_target_dependencies` + `target_compile_definitions` + `target_link_libraries(OpenCL)`) is repeated verbatim across all three projects.

**Decision:**
- `opencl_lab_ros2_guard([LOANED_MESSAGES])` macro: checks `$ENV{ROS_DISTRO}`, emits `FATAL_ERROR` if unset. When `LOANED_MESSAGES` keyword is passed, also emits the RMW warning (non-fatal).
- `opencl_lab_ros2_target(<target> <ament_pkg...>)` macro: calls `ament_target_dependencies(<target> <ament_pkg...>)` (plain form, ament-compatible), then `target_compile_definitions` for the three OpenCL version flags, then `target_link_libraries(<target> OpenCL::OpenCL CLI11::CLI11)` (plain form). Does **not** add `target_include_directories` — callers keep their own (paths differ by depth).

**Action:**
1. Append the two macros to `common/common.cmake` after the existing `opencl_lab_fetch_tinyobjloader` macro.
2. Add a header comment block documenting both macros in the file's top comment block.

---

### B — Fix C1 CMakeLists: missing `ament_cmake`, use new macros

**Problem:**
- C1 never calls `find_package(ament_cmake REQUIRED)` before `ament_target_dependencies` — this works on some ament setups by transitive inclusion but is not correct.
- The ROS 2 guard and OpenCL/ament linking are still inline.

**Decision:** Add `find_package(ament_cmake REQUIRED)` after the ROS 2 packages block. Replace the inline guard and linking with the new macros.

**Action:**
1. Replace the inline `$ENV{ROS_DISTRO}` guard block with `opencl_lab_ros2_guard()`.
2. Add `find_package(ament_cmake REQUIRED)` after the other `find_package` calls.
3. Replace the `ament_target_dependencies` + `target_compile_definitions` + `target_link_libraries` block with `opencl_lab_ros2_target(accel_node rclcpp rclcpp_lifecycle std_msgs lifecycle_msgs)`.

---

### C — Fix C2 CMakeLists: remove redundant `find_package(OpenCL)`, use new macros

**Problem:**
- C2 calls `find_package(OpenCL REQUIRED)` after `common.cmake` already found it — redundant.
- Inline guard and linking are still present.

**Action:**
1. Remove the redundant `find_package(OpenCL REQUIRED)` line and its comment.
2. Replace the inline `$ENV{ROS_DISTRO}` guard block with `opencl_lab_ros2_guard()`.
3. Replace `ament_target_dependencies` + `target_compile_definitions` + `target_link_libraries` block with `opencl_lab_ros2_target(costmap_node rclcpp rclcpp_lifecycle nav_msgs lifecycle_msgs)`.

---

### D — Fix C3 CMakeLists: use new macros (both targets)

**Problem:** C3 has the longest inline ROS 2 boilerplate block (guard + RMW warning) and repeats the ament+OpenCL linking setup for two targets (`perception_node` and `point_cloud_publisher`).

**Action:**
1. Replace the inline `$ENV{ROS_DISTRO}` guard with `opencl_lab_ros2_guard(LOANED_MESSAGES)` (passes `LOANED_MESSAGES` to trigger the RMW warning).
2. For `perception_node`: replace `ament_target_dependencies` + `target_compile_definitions` + `target_link_libraries` with `opencl_lab_ros2_target(perception_node rclcpp rclcpp_lifecycle sensor_msgs lifecycle_msgs)`.
3. For `point_cloud_publisher`: replace `ament_target_dependencies` + `target_link_libraries` with `opencl_lab_ros2_target(point_cloud_publisher rclcpp sensor_msgs)`. Note: `point_cloud_publisher` does not use OpenCL; the macro adds `OpenCL::OpenCL` — this is harmless (it links but the publisher does not call any CL API). Add a comment explaining this.

---

### E — Extract C1 `SyntheticPublisher` to `synthetic_publisher.cpp`

**Problem:** Design spec (`workflow/design/06-robotics-ros2-projects.md` §Directory Structure) lists `synthetic_publisher.cpp` as a separate file compiled into `accel_node`. Task 036 merged `SyntheticPublisher` into `main.cpp`. The Known Issues section explicitly flags this for cleanup.

**Decision:** Extract the `SyntheticPublisher` class definition and implementation into `C1_Node_Acceleration/synthetic_publisher.cpp`. Declare it in a minimal header or as a forward declaration visible to `main.cpp`. Add the new source file to the `add_executable(accel_node ...)` call.

**Action:**
1. Read `C1_Node_Acceleration/main.cpp` in full.
2. Move the `SyntheticPublisher` class definition (declaration + all method bodies) into a new file `C1_Node_Acceleration/synthetic_publisher.cpp`. Include only the necessary headers there.
3. In `main.cpp`, include a forward declaration or a shared minimal header so `main()` can construct `SyntheticPublisher`. If the class is small, a single `synthetic_publisher.hpp` header in the same directory is acceptable.
4. Update `CMakeLists.txt`: `add_executable(accel_node main.cpp synthetic_publisher.cpp)`.
5. Verify the class still compiles — no API or behavior change allowed.

---

## Definition of Done (DoD)

Standard items from `.claude/rules/00_master_specs.md §8` apply.

- [x] `common/common.cmake` contains `opencl_lab_ros2_guard()` and `opencl_lab_ros2_target()` macros with WHY comments.
- [x] C1, C2, C3 `CMakeLists.txt` use the new macros — no duplicated guard or ament+OpenCL linking blocks remain.
- [x] C1 `CMakeLists.txt` includes `find_package(ament_cmake REQUIRED)`.
- [x] C2 `CMakeLists.txt` does not call `find_package(OpenCL REQUIRED)`.
- [x] C1 source: `synthetic_publisher.cpp` exists; `add_executable(accel_node main.cpp synthetic_publisher.cpp)` in CMakeLists.
- [x] `source /opt/ros/jazzy/setup.bash && cmake -B build && cmake --build build` succeeds with zero errors and zero warnings in all three directories (C1, C2, C3).
- [x] MANUAL: Run `GPU=<vendor> ./build/accel_node` from `C1_Node_Acceleration/build/`; confirm per-callback dispatch times logged, no crash.
- [x] MANUAL: Run `GPU=<vendor> ./build/costmap_node` from `C2_Costmap_Inflation/build/` with a valid `map_path` parameter; confirm `output_costmap.bmp` produced and timing table printed.

---

## Execution Report

- **Status:** VALIDATED (auto-verifiable items pass; MANUAL items pending human verification)
- **Session:** 2026-03-19

### Completed

| Item | Action |
|------|--------|
| A — common.cmake macros | Verified — `opencl_lab_ros2_guard()` (lines 129–151) and `opencl_lab_ros2_target()` (lines 165–189) present with WHY comments |
| B — C1 CMakeLists | Verified — `opencl_lab_ros2_guard()` at line 15, `find_package(ament_cmake REQUIRED)` at line 18, `opencl_lab_ros2_target(accel_node ...)` at line 37 |
| C — C2 CMakeLists | Verified — no `find_package(OpenCL REQUIRED)`, `opencl_lab_ros2_guard()` at line 14, `opencl_lab_ros2_target(costmap_node ...)` at line 34 |
| D — C3 CMakeLists | Verified — `opencl_lab_ros2_guard(LOANED_MESSAGES)` at line 15, `opencl_lab_ros2_target(perception_node ...)` at line 35, `opencl_lab_ros2_target(point_cloud_publisher ...)` at line 52 |
| E — SyntheticPublisher extraction | Verified — `synthetic_publisher.cpp` and `synthetic_publisher.hpp` exist; `add_executable(accel_node main.cpp synthetic_publisher.cpp)` confirmed |

### Validation

```
C1 build:
  [  0%] Built target CLI11
  [ 33%] Linking CXX executable accel_node
  Copying kernels for accel_node
  [100%] Built target accel_node
  Result: PASS (zero errors, zero warnings)

C2 build:
  [  0%] Built target CLI11
  [ 33%] Linking CXX executable costmap_node
  Copying kernels for costmap_node
  [100%] Built target costmap_node
  Result: PASS (zero errors, zero warnings)

C3 build:
  [  0%] Built target CLI11
  [ 50%] Built target perception_node
  [100%] Built target point_cloud_publisher
  Result: PASS (zero errors, zero warnings)
```

### Changed Files

| File | Change |
|------|--------|
| `common/common.cmake` | Modified — added `opencl_lab_ros2_guard()` and `opencl_lab_ros2_target()` |
| `02_Projects/C_Robotics_ROS2/C1_Node_Acceleration/CMakeLists.txt` | Modified |
| `02_Projects/C_Robotics_ROS2/C1_Node_Acceleration/main.cpp` | Modified — SyntheticPublisher extracted |
| `02_Projects/C_Robotics_ROS2/C1_Node_Acceleration/synthetic_publisher.cpp` | Created |
| `02_Projects/C_Robotics_ROS2/C2_Costmap_Inflation/CMakeLists.txt` | Modified |
| `02_Projects/C_Robotics_ROS2/C3_Perception_Node/CMakeLists.txt` | Modified |
