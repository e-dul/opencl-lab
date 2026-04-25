## Why

The four ROS 2 modules (C1–C3, Voxel Mapping) already use `ament_cmake` and `ament_target_dependencies` internally, but build as plain executables with no `package.xml` and no `ament_package()` call. This means `ros2 launch <package> <file>` doesn't work, `ros2 component load` doesn't work, and launch files resort to brittle `../build/` path hacks. The modules look like ROS 2 nodes but don't behave like ROS 2 packages — the worst of both worlds. Converting to proper composable packages completes the pattern, eliminates the hacks, and makes the build model match every production ROS 2 system students will encounter.

## What Changes

- **All four modules** (`01_Node_Acceleration`, `02_Costmap_Inflation`, `03_Perception_Node`, `04_Voxel_Mapping`): add `package.xml`; extend CMakeLists to register composable nodes (`rclcpp_components_register_node`), install targets, and call `ament_package()`; remove `main()` from node source files (component container provides the entry point); add `RCLCPP_COMPONENTS_REGISTER_NODE` macro.
- **Companion publishers** (`synthetic_publisher`, `map_publisher`, `point_cloud_publisher`, `voxel_point_cloud_publisher`): same composable treatment; loaded into the same container as their paired node.
- **Launch files**: new per-module `launch/<name>.launch.py` using `ComposableNodeContainer` + `ComposableNode` actions; `ros2 launch <package> <file>` works; no path hacks. Includes YAML parameter files (`config/<name>.yaml`). RViz2 remains a plain `Node` action.
- **Lifecycle self-management**: `auto_activate` parameter (default `true`) on all LifecycleNodes — when `true` the existing one-shot timer self-transition fires; when `false` the node waits for `ros2 lifecycle` CLI or external manager.
- **Intra-process**: all companion pairs land in the same `ComposableNodeContainer`; zero-copy guaranteed without hard-coded single-binary compilation.
- **Supersedes** `ros2-launch-files` (paused); absorbs its YAML parameter file work (D7).

## Capabilities

### New Capabilities

- `ros2-ament-packages`: All four ROS 2 modules are proper ament_cmake packages — `ros2 launch <pkg> <file>`, `ros2 component load`, `ros2 pkg list` all work.
- `ros2-composable-nodes`: Node classes and companion publishers registered as composable components; intra-process zero-copy via shared container.
- `ros2-lifecycle-auto-activate`: `auto_activate` parameter exposes both self-managed and externally-managed lifecycle patterns from a single binary.

### Modified Capabilities

*(none — no existing spec-level requirements change)*

## Impact

- **Files changed**: all four `CMakeLists.txt`, all four main node `.cpp` files (remove `main()`, add namespace + macro), companion publisher `.cpp` files (same), new `package.xml` × 4, new `launch/` × 4, new `config/` × 4
- **`common/common.cmake`**: minor extension to `opencl_lab_ros2_target` macro to handle `SHARED` library targets; kernel/asset install helper added
- **No kernel changes**, **no OpenCL API changes**, **no parameter name changes**
- **Depends on**: `ros2-param-improvements` applied first
- **Supersedes**: `ros2-launch-files` (paused)
- **Build workflow change**: `cmake -B build && cmake --build build` → `colcon build` (from module directory); `source install/setup.bash` required before `ros2 launch`
