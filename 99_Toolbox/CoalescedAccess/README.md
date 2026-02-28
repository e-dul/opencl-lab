# Coalesced Access

**Symptom**: Kernel runs slower than expected. Memory bandwidth utilization (from Nsight / VTune) is < 50% of peak.

## Prerequisites
See [main README](../../README.md) for base requirements (OpenCL 1.2+, CMake 3.18+).

## Build & Run
```bash
cd 99_Toolbox/CoalescedAccess
cmake -B build && cmake --build build
./build/coalesced_demo --width 1920 --height 1080
```

## Verify
```
[ROW-MAJOR  ] Sequential access: 3.1 ms   (coalesced — fast)
[COL-MAJOR  ] Strided access:   21.8 ms   (uncoalesced — 7x slower)
[TRANSPOSED ] Fixed layout:      3.2 ms   (coalesced — fast)
```

Run the demo first. The 7x gap between row-major and column-major access should be visible before reading the explanation below.

## Concept

A GPU memory controller reads data in transactions of 32–128 bytes. When consecutive work items in a warp read consecutive addresses, one transaction serves the whole warp. When they read strided addresses, each needs a separate transaction — bandwidth utilization collapses.

```cl
// Coalesced: work-item N reads element N
int id = get_global_id(0);
output[id] = input[id] * 2.0f;   // items 0,1,2,3... → addresses 0,4,8,12...

// Uncoalesced: work-item N reads row N of a column
int row = get_global_id(0);
output[row] = input[row * width];  // items 0,1,2... → addresses 0,width,2*width...
```

The fix for column-major access: transpose the data layout (or use 2D local memory tiles to read rows locally, then process columns).

## Mini-Challenge

Add a fourth variant that reads every 4th element (stride=4). Measure whether it is slower or faster than stride=width. Explain the result in terms of cache-line transactions.

## Troubleshooting

- **All three variants show similar times**: Your GPU may have a large L2 cache. Try a larger input (`--width 4096 --height 4096`) to exceed the cache.
- **COL-MAJOR is not 7x slower on your device**: AMD GCN and RDNA caches reduce the penalty. The ratio varies by architecture.

## Used In
- [Track B — B3_Ray_Tracer_BVH](../../02_Projects/B_Graphics_HPC/GraphicsHPC.md#b3_ray_tracer_bvh--flagship-project) (triangle data AoS vs SoA)
- [Track C — C3_Perception_Node](../../02_Projects/C_Robotics_ROS2/RoboticsROS2.md#c3_perception_node--flagship-project) (point cloud layout)

---

[Back to Toolbox](../Toolbox.md)
