# C.2 — Costmap Inflation: GPU Distance Transform

**Goal**: Implement a 2D costmap inflation kernel — the algorithm that pads obstacles with a cost gradient so a robot's path planner steers clear of walls — and verify it runs fast enough to sustain a 10 Hz map update rate.

## Prerequisites (delta from module index)

- ROS 2 Jazzy must be sourced: `source /opt/ros/jazzy/setup.bash`.
- Asset required: `assets/warehouse.pgm` (512×512 occupancy grid) in the repository root.

## Build & Run

```bash
cd 02_Costmap_Inflation
source /opt/ros/jazzy/setup.bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/costmap_inflation --ros-args -p map_path:=assets/warehouse.pgm
# Full parameter override:
# GPU=NVIDIA ./build/costmap_inflation --ros-args \
#     -p map_path:=assets/warehouse.pgm \
#     -p inflation_radius:=0.5 \
#     -p resolution:=0.05 \
#     -p decay:=3.0
```

Parameters:

| Parameter | Type | Default | Description |
| :-------- | :--- | :------ | :---------- |
| `map_path` | string | required | Path to `.pgm` occupancy grid |
| `inflation_radius` | double | `0.5` | Inflation radius in metres |
| `resolution` | double | `0.05` | Metres per cell |
| `decay` | double | `3.0` | Cost decay rate |

## Verify

- `output_costmap.bmp` — obstacles are black, inflated zone is a red gradient, free space is white.
- Console prints a three-row comparison table:

```text
Map: 512x512, inflation radius: 10 cells
[CPU          ] Distance transform: 48.200 ms
[GPU naive    ] Distance transform:  3.100 ms   Speedup vs CPU: 15.6x
[GPU tiled    ] Distance transform:  1.800 ms   Speedup vs naive: 1.7x
```

**Performance gates**: GPU naive < 10 ms and GPU tiled < 5 ms at 512×512 †.

> **Known issue — LDS tiling on RTX 4060 / Radeon 680M**: dense 2D neighbourhood scans are not LDS-bandwidth-bound on these architectures; tiled may show ~1.0× vs naive. Hardware waiver † applies to the tiled ≥ 1.5× speedup gate. The correct optimisation for large radii is a separable 1D distance transform (Meijster/Saito algorithm).

## Inspecting Parameters

```bash
# List all declared parameters
ros2 param list /costmap_node

# Show type, description, and constraints for a single parameter
ros2 param describe /costmap_node <param>

# Read a parameter value
ros2 param get /costmap_node <param>

# Set a parameter value at runtime
ros2 param set /costmap_node <param> <value>
```

## Key Concepts

### Why Costmap Inflation?

A robot is not a point — it has a body. A path passing 2 cm from a wall is geometrically valid but physically dangerous. The inflation layer assigns high cost to cells within `inflation_radius` of any obstacle. The path planner minimises cost, naturally keeping the robot clear of walls.

### Distance Transform: Per-Cell Parallelism

A distance transform assigns to each free cell the Euclidean distance to the nearest obstacle cell, then maps that distance to a cost value. The naive CPU approach is O(N²). The GPU version parallelises this — each work item owns one cell:

```cl
__kernel void inflate(__global const uchar* input,
                      __global uchar* output,
                      int width, int height,
                      int radius_px, float decay, float resolution) {
    size_t gx = get_global_id(0);
    size_t gy = get_global_id(1);
    if (gx >= (size_t)width || gy >= (size_t)height) return;
    int x = (int)gx, y = (int)gy;
    // Accumulate squared distances to avoid sqrt inside the inner loop.
    float min_dist_sq = (float)(radius_px + 1) * (float)(radius_px + 1);
    for (int ny = max(0, y - radius_px); ny <= min(height - 1, y + radius_px); ++ny) {
        for (int nx = max(0, x - radius_px); nx <= min(width - 1, x + radius_px); ++nx) {
            if (input[(size_t)ny * width + nx] != 0) {
                float dx = (float)(x - nx), dy = (float)(y - ny);
                float dsq = dx * dx + dy * dy;
                if (dsq < min_dist_sq) min_dist_sq = dsq;
            }
        }
    }
    // Single sqrt at the end — avoids N*radius_px^2 sqrt calls per cell.
    float dist_m = sqrt(min_dist_sq) * resolution;
    output[(size_t)y * width + x] = (min_dist_sq <= (float)radius_px * radius_px)
        ? (uchar)(255.0f * exp(-decay * dist_m)) : 0;
}
```

The inner loop reads `obstacles` at offsets scattered across a `2×radius` window for every cell. Run the kernel and observe GPU memory bandwidth utilisation — then consider why a scattered neighbourhood read from global memory is inefficient and what loading a tile into `__local` memory would change.

## Mini-Challenge

Write a second kernel that loads a tile of the obstacle map into `__local` memory before the inner loop. Profile naive vs tiled — at what tile size does the local-memory version peak? Does the crossover radius (below which local memory gives no benefit) match your expectation?

See [Toolbox: Work-Group Sizing](../../05_Toolbox/14_Work_Group_Sizing/WorkGroupSizing.md) for guidance on choosing tile dimensions for your hardware.

> **Local memory primer**: `__local` declares per-workgroup shared memory. Use `barrier(CLK_LOCAL_MEM_FENCE)` to synchronise before reading data filled by other work-items. See [Toolbox: Local Memory](../../05_Toolbox/01_Local_Memory/LocalMemory.md) for the full tiling pattern.

## Troubleshooting

- **`source /opt/ros/jazzy/setup.bash` must run before CMake**: without it, `find_package(rclcpp REQUIRED)` fails.
- **Wrong GPU**: `GPU=NVIDIA ./build/costmap_inflation`, `GPU=AMD ./build/costmap_inflation`.

---

[Path C: Robotics & ROS 2](../RoboticsROS2.md)
