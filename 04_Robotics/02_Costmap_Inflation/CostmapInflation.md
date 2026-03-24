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

## Key Concepts

### Why Costmap Inflation?

A robot is not a point — it has a body. A path passing 2 cm from a wall is geometrically valid but physically dangerous. The inflation layer assigns high cost to cells within `inflation_radius` of any obstacle. The path planner minimises cost, naturally keeping the robot clear of walls.

### Distance Transform: Per-Cell Parallelism

A distance transform assigns to each free cell the Euclidean distance to the nearest obstacle cell, then maps that distance to a cost value. The naive CPU approach is O(N²). The GPU version parallelises this — each work item owns one cell:

```cl
__kernel void inflate(__global const uchar* obstacles,
                      __global uchar* costmap,
                      int width, int height,
                      int radius, float decay) {
    size_t x = get_global_id(0);
    size_t y = get_global_id(1);
    if (x >= (size_t)width || y >= (size_t)height) return;
    float min_dist = (float)radius + 1.0f;
    for (int dy = -radius; dy <= radius; dy++) {
        for (int dx = -radius; dx <= radius; dx++) {
            int nx = (int)x + dx, ny = (int)y + dy;
            if (nx >= 0 && ny >= 0 && nx < width && ny < height)
                if (obstacles[ny * width + nx] > 0)
                    min_dist = fmin(min_dist, sqrt((float)(dx*dx + dy*dy)));
        }
    }
    costmap[y * width + x] = (min_dist <= (float)radius)
        ? (uchar)(255.0f * exp(-decay * min_dist)) : 0;
}
```

The inner loop reads `obstacles` at offsets scattered across a `2×radius` window for every cell. Run the kernel and observe GPU memory bandwidth utilisation — then consider why a scattered neighbourhood read from global memory is inefficient and what loading a tile into `__local` memory would change.

## Mini-Challenge

Write a second kernel that loads a tile of the obstacle map into `__local` memory before the inner loop. Profile naive vs tiled — at what tile size does the local-memory version peak? Does the crossover radius (below which local memory gives no benefit) match your expectation?

> **Local memory primer**: `__local` declares per-workgroup shared memory. Use `barrier(CLK_LOCAL_MEM_FENCE)` to synchronise before reading data filled by other work-items. See [Toolbox: Local Memory](../../05_Toolbox/01_Local_Memory/LocalMemory.md) for the full tiling pattern.

## Troubleshooting

- **`source /opt/ros/jazzy/setup.bash` must run before CMake**: without it, `find_package(rclcpp REQUIRED)` fails.
- **Wrong GPU**: `GPU=NVIDIA ./build/costmap_inflation`, `GPU=AMD ./build/costmap_inflation`.

---

[Path C: Robotics & ROS 2](../RoboticsROS2.md)
