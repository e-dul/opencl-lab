## Context

Three ROS 2 nodes across two modules currently use `declare_parameter` without descriptors or validation:
- **C2 CostmapNode** (`04_Robotics/02_Costmap_Inflation/main.cpp`): 4 params declared bare; fire-once execution so runtime update is architecturally moot, but validation and introspection still teach the full pattern.
- **C3 PerceptionNode** (`04_Robotics/03_Perception_Node/main.cpp`): 5 params; `ground_z` and `min_intensity` are naturally live-tunable (affect every incoming message); `max_points`/`use_double_buffer` are configure-time only. Loaned-message path selection is silent — log says "loaned path" or "copy-based path" but never shows the RMW name or warns why the fallback happened.
- **VoxelMappingNode** (`06_Bonus/04_Voxel_Mapping/main.cpp`): plain `rclcpp::Node` using CLI11 in `main()` and passing 6 constructor args to the node — a spec violation (CLI11 reserved for standalone tool binaries). The `/proc/self/exe` kernel-dir hack is also non-idiomatic.

All three are standalone CMake builds (not ament packages), so `ros2 param` works via DDS but launch-file parameter injection requires `--ros-args -p`. The change is purely additive in C2; C3 and Voxel Mapping require small structural changes.

## Goals / Non-Goals

**Goals:**
- `ros2 param describe` returns useful type, description, and range for every declared parameter
- `ros2 param set` with invalid value returns a descriptive rejection (not silent accept)
- C3 `ground_z` and `min_intensity` take effect on the next incoming message without restart
- C3 logs the active RMW at `on_activate()` and warns if loaned messages are unavailable
- Voxel Mapping node conforms to spec: params via `declare_parameter`, no CLI11

**Non-Goals:**
- C1 changes of any kind
- Kernel file changes
- CMakeLists.txt changes
- C2 re-running the GPU pipeline on parameter change (fire-once architecture unchanged)
- Adding LifecycleNode to Voxel Mapping
- Launch files (separate change: `ros2-launch-files`)

## Decisions

### D1: `add_on_set_parameters_callback` placement

**Decision**: Register in `on_configure()` for C2 (LifecycleNode) and in the constructor for Voxel Mapping (plain Node). For C3, register in `on_configure()`.

**Why**: LifecycleNodes should not accept external param changes before `on_configure()` completes — the member variables being validated don't exist yet. Plain nodes (Voxel Mapping) initialise everything in the constructor, so the callback can be registered there.

**Alternative considered**: Register in constructor for all — rejected because C2/C3 LifecycleNode member variables are not initialised until `on_configure()`.

### D2: C3 configure-time-only params (`max_points`, `use_double_buffer`)

**Decision**: Reject with `result.successful = false` and reason string `"<param> is only configurable at configure time (restart required)"`. Do NOT attempt to realloc buffers or reinitialise the double-buffer state at runtime.

**Why**: Buffer reallocation mid-stream would require queue drain, buffer teardown, and realloc — equivalent to a full `on_cleanup` + `on_configure` cycle. The LifecycleNode pattern exists precisely for this: transition to INACTIVE → UNCONFIGURED and reconfigure. Teaching an ad-hoc in-place realloc would undermine the lifecycle lesson.

### D3: C3 live-update params (`ground_z`, `min_intensity`)

**Decision**: Update `ground_z_` and `min_intensity_` member variables atomically inside the callback. The `process_callback` / `run_pipeline` path reads these members at dispatch time, so the next message naturally picks up the new value.

**Why**: No synchronisation primitive needed — `SingleThreadedExecutor` serialises both the param callback and the subscription callback on the same thread. Atomic or mutex would be correct but add unnecessary complexity for a single-threaded context.

**Note**: Add a comment explaining the single-threaded executor guarantee.

### D4: Voxel Mapping kernel-dir

**Decision**: Replace `/proc/self/exe` symlink read with `get_binary_dir()` from `opencl_utils.hpp` (same helper used in C1/C2/C3).

**Why**: Idiomatic — reuses the existing pattern, removes the try/catch, and is more portable. The only reason CLI11 parsing preceded `rclcpp::init` was to support `--help` before DDS init; with `declare_parameter` that constraint is gone.

### D5: `ParameterDescriptor` for `map_path` in C2

**Decision**: Add description only; no range constraint (it's a string path). Constraints for string params are free-form; the existing runtime check (throw if stbi_load fails) remains the effective validation.

**Why**: `rcl_interfaces` doesn't provide a `string_range` constraint type. A description is sufficient for `ros2 param describe` to be informative.

### D6: `flip_threshold` type in Voxel Mapping

**Decision**: Declare as `int64_t` parameter (ROS 2 integer params are always `int64_t`); guard against `> UINT32_MAX` before narrowing to `uint32_t`. Same pattern as `buffer_size` in C1 and `max_points` in C3.

**Why**: ROS 2 parameter system has no `uint32` type. The guard makes the narrowing safe and educationally consistent.

## Risks / Trade-offs

- **Voxel Mapping CLI change is user-visible**: `--topic /points` becomes `--ros-args -p topic:=/points`. Must update `VoxelMapping.md` run commands. → Mitigation: update the doc as part of the same task.
- **C3 param callback thread safety**: callback and subscription callback both run on `SingleThreadedExecutor` — no race. If executor is ever changed to multi-threaded in future, a mutex would be required. → Mitigation: add a WHY comment documenting the assumption.
- **C2 validation callback runs after node shuts down**: The node processes one map and exits. A user calling `ros2 param set` post-shutdown sees no effect. The callback is still correct (it would reject invalid values if the node were still alive) and educational. → Accept as-is.

## Open Questions

*(none — all decisions resolved during explore session)*
