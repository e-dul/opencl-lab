## Context

C3 (`perception_node` + `point_cloud_publisher`) and Voxel Mapping (`voxel_mapping` + `voxel_point_cloud_publisher`) are already split into separate binaries — the correct architecture for launch-file orchestration. Neither is a proper ament package (no `package.xml`, no `install()`), so `ros2 launch <package> <launch_file>` doesn't apply. Instead, launch files are invoked directly by path: `ros2 launch ./launch/perception_node.launch.py`.

Both modules have standalone CMake builds; executables land in `./build/`. Launch files reference executables by their absolute path derived from `os.path` relative to the launch file's location.

C1 and C2 are intentionally self-contained single-binary designs and receive no launch files.

## Goals / Non-Goals

**Goals:**
- `ros2 launch ./launch/perception_node.launch.py` starts perception_node + point_cloud_publisher + RViz2 in one command
- `ros2 launch ./launch/voxel_mapping.launch.py` starts voxel_mapping + voxel_point_cloud_publisher + RViz2 in one command
- All node parameters overridable via launch arguments (`ros2 launch ... ground_z:=0.5`)
- RViz2 starts by default; `use_rviz:=false` disables it
- `perception_node.rviz` pre-configured: PointCloud2 `/filtered_points`, fixed frame `lidar_link`
- Voxel Mapping reuses existing `voxel_mapping.rviz`

**Non-Goals:**
- `nav2_lifecycle_manager` or Python lifecycle actions (simple launch only)
- ament package registration (`package.xml`, `install()`, `setup.py`)
- C1 / C2 launch files
- Launch files for the publisher-only binaries in isolation

## Decisions

### D1: Executable path resolution in launch files

**Decision**: Derive executable paths relative to the launch file location using `os.path`:
```python
import os
_dir = os.path.dirname(os.path.realpath(__file__))
_build = os.path.join(_dir, '..', 'build')
```

**Why**: The modules are standalone CMake builds, not installed ROS 2 packages. `get_package_share_directory` doesn't apply. Path-relative resolution works as long as the user builds with `cmake -B build` from the module root — the lab's standard invocation.

**Alternative considered**: `FindPackageShare` — rejected, requires `ament_cmake` package registration.

### D2: No lifecycle management in launch files

**Decision**: Launch files spawn nodes and let them manage themselves. `perception_node` already drives its own configure/activate transitions in `main()`. No `LifecycleTransition` actions.

**Why**: The design spec for C3 explicitly allows self-managed lifecycle in `main()` as a valid standalone pattern. Adding lifecycle actions would add complexity without teaching benefit at this stage. The separate `ros2-launch-files` change is scoped to orchestration only.

### D3: RViz2 always starts by default; opt-out via `use_rviz`

**Decision**: `use_rviz` launch argument defaults to `'true'`. An `IfCondition` wraps the RViz2 `Node` action.

**Why**: Aligns with Q2 answer: RViz starts by default, user passes `use_rviz:=false` to suppress (e.g., headless CI). Matches nav2 and MoveIt2 conventions.

### D4: perception_node.rviz content

**Decision**: Pre-configure with:
- Fixed Frame: `lidar_link`
- PointCloud2 display on `/filtered_points`, Style: Points, Size: 0.01m, Color: Flat (intensity)
- PointCloud2 display on `/cluster_features`, Style: Spheres, Size: 0.3m, Color: red
- Camera position: top-down orthographic

**Why**: The existing `PerceptionNode.md` documents RViz setup manually. The config file makes that setup reproducible. `/cluster_features` at Size 0.3m matches the doc note that the centroid is invisible at default pixel size.

### D5: Point cloud publisher scene and rate as launch args

**Decision**: Expose `scene` (default `grid`), `hz` (default `200`), `points` (default `100000`) as launch arguments for both C3 and Voxel Mapping publishers.

**Why**: The most common tuning operations during development. Allows `ros2 launch ... scene:=mixed hz:=10` without editing the launch file.

### D6: Launch arg for GPU selection

**Decision**: Expose `gpu` launch argument (default empty string). When non-empty, pass as `GPU=<value>` via `additional_env` on the node action.

**Why**: GPU selection is via env var per lab convention. Exposing it as a launch arg (`gpu:=NVIDIA`) is more ergonomic than requiring a separate `export GPU=NVIDIA` before launching.

### D7: Default parameters via YAML file

**Decision**: Each launch file loads `config/<node>.yaml` as default parameters.
Launch argument overrides take precedence (`--ros-args -p` overrides file values).
New files: `config/perception_node.yaml`, `config/voxel_mapping.yaml`.

**Why**: `ros2-param-improvements` adds descriptors that define valid ranges.
Exposing defaults as an editable YAML file completes the parameter story:
edit YAML → launch loads it → `declare_parameter` validates it → callback enforces it at runtime.
This matches the production pattern for robot configuration; `--ros-args -p` inline
remains available for one-shot overrides.

**Alternative considered**: Launch arguments only — rejected because a YAML file is the
standard way operators configure a deployed robot without touching the launch file itself.

## Risks / Trade-offs

- **Absolute build path assumption**: launch files assume `../build/` relative to `launch/`. If user builds with a different `-B` path, the executable won't be found. → Mitigation: document the assumption clearly in the README; this is consistent with the lab's existing `cmake -B build` convention.
- **RViz2 must be installed**: `ros2 run rviz2 rviz2` fails if `rviz2` package is absent (e.g., ROS 2 base install). → Mitigation: `use_rviz:=false` suppresses it; add note in troubleshooting.
- **perception_node.rviz is hand-authored YAML**: RViz config format is undocumented and version-sensitive. → Mitigation: keep the config minimal; test against Jazzy's RViz2.
