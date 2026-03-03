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

Run the demo first, then read the explanation. The gap you observe depends on your GPU:

```bash
# Default (1920×1080) — may show a small gap if your L2 cache is large
./build/coalesced_demo

# 8192×8192 (256 MiB) — exceeds L2 on most GPUs; shows the real penalty
./build/coalesced_demo --width 8192 --height 8192
```

Expected output at 8192×8192 — actual numbers vary by architecture (see Troubleshooting):

```
Variant                   Kernel Time (ms)
─────────────────────────────────────────
ROW-MAJOR (coalesced)     X.XXX
COL-MAJOR (uncoalesced)   X.XXX          ← target: ≥5× slower than ROW-MAJOR
TRANSPOSED (fixed)        X.XXX
```

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

### Why the penalty is architecture-dependent

The coalescing penalty is not a fixed multiplier — it is proportional to **memory bandwidth starvation**. Two factors control how visible it is:

- **L2 cache size**: A large L2 (e.g. 24 MB on RTX 4060) can absorb strided reads if the working set fits. At 1920×1080 (~16 MB), both coalesced and strided patterns are served mostly from L2, so the gap is small (≈1.8×). Exceeding L2 with `--width 8192` exposes the real DRAM penalty.

- **Memory controller depth**: High-end discrete GPUs issue hundreds of outstanding DRAM requests in parallel. A deep request queue keeps the bus saturated even for scattered access — the penalty exists but is partially hidden. Bandwidth-constrained iGPUs have shallow queues and shared DDR bandwidth, so scatter causes the bus to stall. Measured gap at 8192×8192:

  | GPU | ROW-MAJOR | COL-MAJOR | Ratio |
  |-----|-----------|-----------|-------|
  | RTX 4060 Laptop (discrete, GDDR6) | 2.2 ms | 2.8 ms | **1.3×** |
  | AMD Radeon 680M (iGPU, DDR5 shared) | 10.3 ms | 83.8 ms | **8.1×** |

  Same kernel, same access pattern — 6× difference in penalty ratio. The iGPU hits the design gate (≥5×); the discrete GPU nearly hides it.

## Mini-Challenge

Add a fourth variant that reads every 4th element (stride=4). Measure whether it is slower or faster than stride=width. Explain the result in terms of cache-line transactions.

## Troubleshooting

- **All three variants show similar times at 1920×1080**: Your L2 cache is larger than the 16 MiB working set. Run `--width 8192 --height 8192` to exceed it.
- **COL-MAJOR gap is small even at 8192×8192**: You are on a high-bandwidth discrete GPU with a deep memory request queue (e.g. RTX series). The penalty exists but the hardware partially hides it. This is correct behavior — see the architecture table in Concept above.
- **COL-MAJOR is >5× slower**: You are on an iGPU or a GPU with shared/narrow memory bandwidth. This is the textbook result.

## Used In
- [Track B — B3_Ray_Tracer_BVH](../../02_Projects/B_Graphics_HPC/GraphicsHPC.md#b3_ray_tracer_bvh--flagship-project) (triangle data AoS vs SoA)
- [Track C — C3_Perception_Node](../../02_Projects/C_Robotics_ROS2/RoboticsROS2.md#c3_perception_node--flagship-project) (point cloud layout)

---

[Back to Toolbox](../Toolbox.md)
