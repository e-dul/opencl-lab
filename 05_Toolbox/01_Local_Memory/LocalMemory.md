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

- **"Out of resources" at large radius**: Local memory is limited per compute unit — check your device's actual limit with `clinfo | grep "Local memory"` or query `CL_DEVICE_LOCAL_MEM_SIZE` at runtime (typical range: 32–128 KB depending on GPU generation). Reduce tile size or work-group size to fit within budget.
- **Missing the `barrier()` causes incorrect output**: Without `barrier(CLK_LOCAL_MEM_FENCE)`, some work-items read the tile before neighbors finish writing the halo. The result is visually noisy in predictable ways — use this to confirm the bug.

## Used In
- [Track A — 02_YUV_Pipeline](../../02_Multimedia/02_YUV_Pipeline/YUVPipeline.md)
- [Track C — 02_Costmap_Inflation](../../04_Robotics/02_Costmap_Inflation/CostmapInflation.md)
- [Track A — 09 SoftISP](../../02_Multimedia/09_SoftISP/SoftISP.md)

---

## Advanced Challenge: Bank Conflicts in LDS

Local Data Store (LDS) on modern GPUs is divided into **32 banks** (Nvidia, AMD GCN/RDNA). Each bank is an independent memory module — when all work-items in a warp/wavefront access *different* banks simultaneously, the hardware serves all requests in one cycle. That is the goal.

**The problem — stride-32 access:**

```cl
__local float tile[ROWS][COLS];

// Thread i reads tile[0][i * 32]
// Every thread maps to bank (i * 32) % 32 == 0 — the SAME bank.
// Result: 32 serialized accesses instead of 1 parallel read. 32× slowdown.
float val = tile[0][get_local_id(0) * 32];
```

The bank index for element `e` is `(e % 32)`. A stride of 32 means every thread hits bank 0, causing a **32-way bank conflict** — all reads are serialized.

**The fix — +1 column padding:**

```cl
// Add one padding element per row to shift each row's bank alignment
__local float tile[ROWS][COLS + 1];

// Thread i now reads tile[0][i * 32], but the row stride is COLS+1, not COLS.
// Element address = row * (COLS+1) + col → bank = (row*(COLS+1) + col) % 32
// The +1 offsets each row's start by one bank, spreading accesses across banks.
float val = tile[0][get_local_id(0) * 32];  // now conflict-free
```

The extra element costs `ROWS * sizeof(float)` bytes — a small price for conflict-free access. This is the same tile array used in the box-blur kernel above: replacing `tile[TILE][TILE]` with `tile[TILE][TILE + 1]` eliminates bank conflicts in the halo load phase without changing any other logic.

**When does this matter in practice?**

The tile-based convolution kernel in this module loads halo rows cooperatively. If `TILE` happens to be a multiple of 32 (common: 32, 64), the column indices align to the same bank for every row-boundary thread. Add the `+1` pad whenever `COLS % 32 == 0`.

**How to detect conflicts:** Run with Nvidia Nsight or AMD Radeon GPU Profiler (RGP) and inspect the `LDS bank conflicts` counter. A non-zero value on the tile-load barrier confirms the problem.

---

[Back to Toolbox](../Toolbox.md)
