# Local Memory (LDS)

**Symptom**: Kernel repeatedly reads the same global memory addresses across different work-items (e.g., convolution, inflation radius, debayering). Memory-bound, not compute-bound.

## Prerequisites
Prerequisites: OpenCL 1.2+, CMake 3.18+, `clinfo` installed. See [main README](../../README.md) for base requirements.

## Build & Run
```bash
cd 05_Toolbox/01_Local_Memory
cmake -B build && cmake --build build
./build/local_memory --kernel blur --radius 5 --width 1920 --height 1080
```

## Verify
```
[Global memory ] Box blur (r=5):  18.4 ms
[Local memory  ] Box blur (r=5):   3.9 ms   4.7x faster
```

Run the demo first. Note the speedup at radius 5. Then increase the radius and observe the crossover point where local memory pressure limits gains.

## Concept

`__local` memory is an explicit, programmer-managed cache inside each compute unit. It is ~100x faster than global memory and shared among all work-items in a work-group.

**Tile-based pattern** (the universal template):

```cl
__kernel void blur(__global uchar* in, __global uchar* out,
                   int width, int height, int radius) {
    int lx = get_local_id(0), ly = get_local_id(1);
    int gx = get_global_id(0), gy = get_global_id(1);
    int TILE = get_local_size(0) + 2 * radius;

    __local uchar tile[TILE][TILE];  // shared tile including halo

    // 1. Cooperatively load tile from global memory (including halo)
    tile[ly + radius][lx + radius] = in[gy * width + gx];
    // ... load halo edges (boundary-clamped) ...
    barrier(CLK_LOCAL_MEM_FENCE);   // all work-items finish loading

    // 2. Read neighbors from fast local memory
    float sum = 0.0f;
    for (int dy = -radius; dy <= radius; dy++)
        for (int dx = -radius; dx <= radius; dx++)
            sum += tile[ly + radius + dy][lx + radius + dx];
    out[gy * width + gx] = (uchar)(sum / ((2*radius+1)*(2*radius+1)));
}
```

**The halo problem**: a tile processing a 16x16 region with radius 5 needs a 26x26 local memory tile (16 + 2x5). Halo cells must be loaded from global memory before the barrier — the tool shows how to handle boundary clamping correctly.

## Mini-Challenge

Increase `--radius` from 5 to 15. At what radius does the local memory variant stop gaining over global? Query `CL_DEVICE_LOCAL_MEM_SIZE` to understand the hardware limit.

## Troubleshooting

- **"Out of resources" at large radius**: Local memory is limited (typically 32–64 KB per compute unit). Reduce tile size or work-group size to fit within budget.
- **Missing the `barrier()` causes incorrect output**: Without `barrier(CLK_LOCAL_MEM_FENCE)`, some work-items read the tile before neighbors finish writing the halo. The result is visually noisy in predictable ways — use this to confirm the bug.

## Used In
- [Track A — 02_YUV_Pipeline](../../02_Multimedia/02_YUV_Pipeline/YUVPipeline.md)
- [Track C — 02_Costmap_Inflation](../../04_Robotics/02_Costmap_Inflation/CostmapInflation.md)
- [Track A — 09 SoftISP](../../02_Multimedia/09_SoftISP/SoftISP.md)

---

[Back to Toolbox](../Toolbox.md)
