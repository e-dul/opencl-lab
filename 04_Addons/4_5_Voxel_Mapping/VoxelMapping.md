# 4.5 — 3D Voxel Mapping: Grand Finale

**When to use**: you've completed both Track B and Track C and want to see their techniques combine into a real robotics application.

## Prerequisites
See [main README](../../README.md) for base requirements (OpenCL 1.2+, CMake 3.18+).

- **Required**: [B3_Ray_Tracer_BVH](../../02_Projects/B_Graphics_HPC/GraphicsHPC.md#b3_ray_tracer_bvh--flagship-project) completed
- **Required**: [C3_Perception_Node](../../02_Projects/C_Robotics_ROS2/RoboticsROS2.md#c3_perception_node--flagship-project) completed
- ROS 2 Jazzy: `source /opt/ros/jazzy/setup.bash`

## Build & Run

```bash
source /opt/ros/jazzy/setup.bash
cd 04_Addons/4_5_Voxel_Mapping
cmake -B build && cmake --build build
```

**Live (synthetic publisher in terminal 1, voxel_mapping in terminal 2):**

```bash
# Terminal 1 — static scene (DDA sanity check):
./build/voxel_point_cloud_publisher --scene static --hz 10 --frames 10

# Terminal 2:
./build/voxel_mapping --topic /points --resolution 0.1
# Ctrl-C to stop → output_voxel_slice.bmp written

# Dynamic scene (exercises flip-count filter):
./build/voxel_point_cloud_publisher --scene dynamic --hz 10 --frames 50 &
./build/voxel_mapping --topic /points --resolution 0.1 --enable-flip-filter
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
  Voxel grid: 200x200x50 @ 0.1m resolution
  [GPU] Ray casting (100k pts):  3.1 ms per frame
  [GPU] Map update:              1.4 ms per frame
  Total pipeline:                4.5 ms  ← inherits C3 performance gate (< 5 ms)
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

## Challenge: Dynamic Object Filter

If a voxel flips between OCCUPIED and FREE more than N times per second, classify it as dynamic (moving person/vehicle) and exclude it from the static map.

1. Add a `flip_count` buffer alongside the occupancy grid
2. Increment `flip_count[voxel_id]` atomically on every state change
3. In a post-processing kernel, zero out occupancy for voxels where `flip_count > threshold`
4. Profile the counter buffer overhead vs the base ray casting time

## Troubleshooting

- **Voxel slice shows all grey (unknown)**: check that the publisher or `ros2 bag play` is running and publishing on the same topic as `--topic`. Default is `/points`.
- **Map drifts over time**: sensor pose is assumed static. For a moving robot, integrate odometry into the origin parameter per frame.
- **Pipeline exceeds 5 ms**: the voxel update step uses global atomics. If this dominates, reduce grid resolution (`--resolution 0.2`) or use a hierarchical update (only mark changed voxels).

---

[Back to Add-ons](../Addons.md)
