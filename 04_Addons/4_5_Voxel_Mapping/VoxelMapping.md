# 4.5 — 3D Voxel Mapping: Grand Finale

**When to use**: you've completed both Track B and Track C and want to see their techniques combine into a real robotics application.

## Prerequisites
See [main README](../../README.md) for base requirements (OpenCL 1.2+, CMake 3.18+).

- **Required**: [B3_Ray_Tracer_BVH](../../02_Projects/B_Graphics_HPC/GraphicsHPC.md#b3-bvh-ray-tracer) completed
- **Required**: [C3_Perception_Node](../../02_Projects/C_Robotics_ROS2/RoboticsROS2.md#c3-perception-node) completed
- ROS 2 Jazzy: `source /opt/ros/jazzy/setup.bash`

## Build & Run

```bash
source /opt/ros/jazzy/setup.bash
cd 04_Addons/4_5_Voxel_Mapping
cmake -B build && cmake --build build
```

**Live (synthetic publisher in terminal 1, voxel_mapping in terminal 2):**

> Two processes communicate over ROS 2 DDS — run in separate terminals that have each sourced ROS 2 (`source /opt/ros/jazzy/setup.bash`).

```bash
# Terminal 1 — static scene (DDA sanity check):
./build/voxel_point_cloud_publisher --scene static --hz 10 --frames 10

# Terminal 2:
./build/voxel_mapping --topic /points --resolution 0.1
# Ctrl-C to stop → output_voxel_slice.bmp written

# Dynamic scene (exercises flip-count filter):
./build/voxel_point_cloud_publisher --scene dynamic --hz 10 --frames 50 &
./build/voxel_mapping --topic /points --resolution 0.1 --enable-flip-filter --flip-threshold 3 --move-speed 0.2
```

**From a bag (any PointCloud2 bag, XYZI point_step=16):**

```bash
./build/voxel_mapping --topic /points --resolution 0.1 &
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
    [GPU] Flip filter: 0.211 ms   ← only when --enable-flip-filter
    Total pipeline   : 2.370 ms (10000 pts)
  ```
  Total must remain < 5 ms @ 100k points (C3 performance gate). A `[WARN]` line is printed if the gate is exceeded.

> **Note:** The "3 MB" figure is storage-type-dependent and undocumented. 200×200×50 voxels = 2,000,000 cells. As `uint` (4 bytes): 8 MB; as `uchar` (1 byte): 2 MB; as a 2-bit packed field: ~0.5 MB. Update this line to match the actual `cl::Buffer` element type used in `dda_cast.cl`.

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

This is the 3D DDA (Digital Differential Analyzer) algorithm, run in parallel — one GPU work-item per Lidar point. The voxel grid lives in a `cl::Buffer` and cells are updated with `atomic_or` to avoid races.

**The knowledge transfer**: the ray-box intersection test from `B3_Ray_Tracer_BVH` is reused here unchanged. Instead of testing against BVH node AABBs, you step through voxel grid cells. Same math, different context — this is why Track B teaches BVH before this finale.

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
- `clear_occupied.cl` — clears only `OCCUPIED_BIT` per frame (preserving accumulated `FREE_BIT`) when `--enable-flip-filter` is active, so dynamic voxels are suppressed without erasing free-space history.

## Challenge: Dynamic Object Filter

If a voxel flips between OCCUPIED and FREE more than N times per second, classify it as dynamic (moving person/vehicle) and exclude it from the static map.

1. Add a `flip_count` buffer alongside the occupancy grid.
2. Increment `flip_count[voxel_id]` atomically on every state change.
3. In a post-processing kernel, zero out occupancy for voxels where `flip_count > threshold`.
4. After zeroing, reset `flip_count` to 0 for those voxels so they can be re-detected in subsequent cycles. Filtering is periodic, not permanent — a voxel that stops moving will accumulate occupancy again once it no longer flips past the threshold.
5. Profile the counter buffer overhead vs the base ray casting time.

## Troubleshooting

- **Voxel slice shows all grey (unknown)**: check that the publisher or `ros2 bag play` is running and publishing on the same topic as `--topic`. Default is `/points`.
- **Map drifts over time**: sensor pose is assumed static. For a moving robot, integrate odometry into the origin parameter per frame.
- **Pipeline exceeds 5 ms**: the voxel update step uses global atomics. If this dominates, reduce grid resolution (`--resolution 0.2`) or use a hierarchical update (only mark changed voxels).
- **Dynamic objects never re-appear after filtering**: flip counts must be reset to 0 after each threshold crossing. If objects are permanently absent, verify that `clear_occupied.cl` is dispatched each frame and that the host-side reset pass runs before the next DDA cast.
- **`/voxel_slice` missing ground detail**: the projection starts at `z = gz/2 + 1` to exclude ground-level voxels. Lower this offset if your sensor is mounted near the bottom half of the grid.

---

[Back to Add-ons](../Addons.md)
