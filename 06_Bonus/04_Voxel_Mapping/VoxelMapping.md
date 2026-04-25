# 4.5 — 3D Voxel Mapping: Grand Finale

**When to use**: you've completed both Track B and Track C and want to see their techniques combine into a real robotics application.

## Prerequisites
See [main README](../../README.md) for base requirements (OpenCL 1.2+, CMake 3.18+).

- **Required**: [02_Ray_Tracer_BVH](../../03_GraphicsHPC/02_Ray_Tracer_BVH/RayTracerBVH.md) completed
- **Required**: [03_Perception_Node](../../04_Robotics/03_Perception_Node/PerceptionNode.md) completed
- ROS 2 Jazzy: `source /opt/ros/jazzy/setup.bash`

## Build & Run

```bash
source /opt/ros/jazzy/setup.bash
cd 06_Bonus/04_Voxel_Mapping
colcon build --packages-select voxel_mapping
source install/setup.bash
ros2 launch voxel_mapping voxel_mapping.launch.py
# Ctrl-C to stop → output_voxel_slice.bmp written
```

Parameter overrides (former CLI11 flags are now `ros2 param` / launch args):

```bash
# Static scene (DDA sanity check), no RViz
ros2 launch voxel_mapping voxel_mapping.launch.py \
    scene:=static hz:=10.0 frames:=10 use_rviz:=false

# Dynamic scene with flip-count filter (exercises dynamic removal)
ros2 launch voxel_mapping voxel_mapping.launch.py \
    scene:=dynamic hz:=10.0 frames:=50 \
    enable_flip_filter:=true flip_threshold:=3

# GPU selection
ros2 launch voxel_mapping voxel_mapping.launch.py gpu:=NVIDIA
```

Launch arguments:

| Argument | Default | Description |
| :------- | :------ | :---------- |
| `topic` | `/points` | PointCloud2 topic to subscribe/publish on |
| `resolution` | `0.1` | Voxel size in metres |
| `output` | `output_voxel_slice.bmp` | Output BMP path for top-down slice |
| `enable_flip_filter` | `false` | Enable dynamic-object flip-count filter |
| `flip_threshold` | `5` | Flip count threshold for dynamic object removal |
| `scene` | `static` | Publisher scene: `static` or `dynamic` |
| `hz` | `10.0` | Publisher rate in Hz |
| `points` | `10000` | Points per publisher message |
| `frames` | `0` | Stop after N frames (0 = infinite) |
| `move_speed` | `0.05` | Orbit angle increment per frame (rad, dynamic scene only) |
| `gpu` | `` | GPU vendor substring (e.g. `NVIDIA`, `AMD`) |
| `use_rviz` | `true` | Launch RViz2 for visualisation |

**From a bag (any PointCloud2 bag, XYZI point_step=16):**

```bash
ros2 launch voxel_mapping voxel_mapping.launch.py frames:=0 &
ros2 bag play <path/to/bag>
```

## Verify
- `output_voxel_slice.bmp` — top-down 2D slice (occupied = black, free = white, unknown = grey)
- Console prints:
  ```
  [INFO] Sensor pose assumed static. Odometry integration not implemented.
  [INFO] Voxel grid: 200x200x50 @ 0.10m resolution (3 MB)
    [GPU] Upload     : 0.312 ms
    [GPU] DDA cast   : 1.847 ms
    [GPU] Flip filter: 0.211 ms   ← only when enable_flip_filter:=true
    Total pipeline   : 2.370 ms (10000 pts)
  ```
  Total must remain < 5 ms @ 100k points (C3 performance gate). A `[WARN]` line is printed if the gate is exceeded.

> **Note:** The "3 MB" figure in the console output is approximate; actual size depends on the element type used in `dda_cast.cl`. 200×200×50 = 2,000,000 cells — as `uint` (4 bytes): 8 MB; as `uchar` (1 byte): 2 MB.

## Live Visualisation (RViz)

Two debug topics are published every frame and can be inspected in RViz without recording a bag:

| Topic | Type | Content |
| :--- | :--- | :--- |
| `/voxel_map` | `sensor_msgs/PointCloud2` | XYZ centroids of all `OCCUPIED` voxels in world frame |
| `/voxel_slice` | `sensor_msgs/Image` (MONO8) | Above-sensor column projection; width=gx, height=gy |

**RViz setup:**
1. Set **Fixed Frame** to `map` (or whatever frame the publisher uses).
2. Add a **PointCloud2** display, topic `/voxel_map`. Set Style to `Voxels` or `Points`.
3. Add an **Image** display, topic `/voxel_slice`. Each pixel encodes: 0 = occupied (black), 255 = free (white), 128 = unknown (grey).

The slice projects z from `gz/2 + 1` upward, so ground-return voxels are excluded from the 2D footprint.

Spot-check a single image message without RViz:
```bash
ros2 topic echo --once /voxel_slice
```

## Concept: Ray Casting in Voxel Space

In Track B, rays go from a camera into a scene to find what they hit. Here the direction reverses: rays originate at the Lidar sensor and travel outward along each measured return.

```
For each Lidar return:
  origin  = sensor position
  endpoint = measured hit point

  Mark every voxel along origin → endpoint:  FREE
  Mark the endpoint voxel:                   OCCUPIED
  Voxels never traversed:                    UNKNOWN
```

This is the 3D DDA (Digital Differential Analyzer) algorithm, run in parallel — one GPU work-item per Lidar point. The voxel grid lives in a `cl::Buffer` and cells are updated with `atomic_or` to avoid races. `atomic_or` sets individual bits without clearing others — same principle as `atomic_add` from Toolbox 12, applied to bitfield flags.

**The knowledge transfer**: the ray-box intersection test from `02_Ray_Tracer_BVH` is reused here unchanged. Instead of testing against BVH node AABBs, you step through voxel grid cells. Same math, different context — this is why Track B teaches BVH before this finale.

> **Ray–AABB intersection** (for students who skipped Track B): given a ray `origin + t * direction` and an axis-aligned bounding box defined by `box_min` and `box_max`, the intersection test computes the entry and exit distances `t_near` and `t_far` by slabbing — dividing the box into three pairs of infinite planes (one per axis) and finding where the ray enters and exits each slab. The ray hits the box when `t_near < t_far` and `t_far > 0`.

```cl
// From B3: ray-AABB intersection (reused here)
bool intersects_aabb(Ray ray, float3 box_min, float3 box_max, float* t_near);

// New in 4.5: DDA voxel traversal
void trace_ray(__global uint* grid, int3 grid_dims, float3 origin, float3 endpoint) {
    // Step through voxels using DDA
    // Mark FREE along the path, OCCUPIED at endpoint
}
```

Three kernels drive the pipeline:
- `dda_cast.cl` — marks FREE bits along each ray and sets `OCCUPIED_BIT` at the endpoint.
- `flip_count.cl` — increments a per-voxel counter on every FREE/OCCUPIED transition.
- `clear_occupied.cl` — clears only `OCCUPIED_BIT` per frame (preserving accumulated `FREE_BIT`) when `enable_flip_filter:=true`, so dynamic voxels are suppressed without erasing free-space history.

## Challenge: Dynamic Object Filter

If a voxel flips between OCCUPIED and FREE more than N times per second, classify it as dynamic (moving person/vehicle) and exclude it from the static map.

1. Add a `flip_count` buffer alongside the occupancy grid.
2. Increment `flip_count[voxel_id]` atomically on every state change.
3. In a post-processing kernel, zero out occupancy for voxels where `flip_count > threshold`.
4. After zeroing, reset `flip_count` to 0 for those voxels so they can be re-detected in subsequent cycles. Filtering is periodic, not permanent — a voxel that stops moving will accumulate occupancy again once it no longer flips past the threshold.
5. Profile the counter buffer overhead vs the base ray casting time.

## Recording & Replay

Topic: `/points` · Bag name: `voxel_bag`

```bash
# Record
ros2 bag record /points -o voxel_bag

# Replay (suppresses VoxelCloudPublisher; ros2 bag play starts automatically)
ros2 launch voxel_mapping voxel_mapping.launch.py bag:=voxel_bag
```

See [ROS 2 Setup §5 — Recording & Replay](../../04_Robotics/SETUP.md#5-recording--replay) for the full record → inspect → replay workflow and QoS override instructions.

## Troubleshooting

- **Voxel slice shows all grey (unknown)**: check that the publisher or `ros2 bag play` is running and publishing on the same topic (`ros2 param list /voxel_mapping` to inspect). Default is `/points`.
- **Map drifts over time**: sensor pose is assumed static. For a moving robot, integrate odometry into the origin parameter per frame.
- **Pipeline exceeds 5 ms**: the voxel update step uses global atomics. If this dominates, reduce grid resolution (`resolution:=0.2`) or use a hierarchical update (only mark changed voxels).
- **Dynamic objects never re-appear after filtering**: flip counts must be reset to 0 after each threshold crossing. If objects are permanently absent, verify that `clear_occupied.cl` is dispatched each frame and that the host-side reset pass runs before the next DDA cast.
- **`/voxel_slice` missing ground detail**: the projection starts at `z = gz/2 + 1` to exclude ground-level voxels. Lower this offset if your sensor is mounted near the bottom half of the grid.

---

[Back to Bonus.md](../Bonus.md)
