# 4.7 — SoftISP: Real-Time 4K Bayer Debayering

**When to use**: you're reading raw frames from an industrial or embedded camera (no hardware ISP) and need to convert them to RGB fast enough for real-time processing.

## Prerequisites
See [main README](../../README.md) for base requirements (OpenCL 1.2+, CMake 3.18+).

- **Required**: [Toolbox: Local Memory](../../99_Toolbox/LocalMemory/LocalMemory.md) — the LDS tile pattern used here is the same one from that tool (LDS = Local Data Store, i.e., `__local` shared memory in OpenCL)

## Build & Run
```bash
cd 04_Addons/4_7_SoftISP
cmake -B build && cmake --build build
./build/softISP_demo --input ../../../assets/raw_bayer_4k.raw \
    --width 3840 --height 2160
# GPU=NVIDIA ./build/softISP_demo --input ../../../assets/raw_bayer_4k.raw \
#     --width 3840 --height 2160
```

## Verify
- `output_rgb_v1.bmp` — correctly colored image from naive kernel
- `output_rgb_v2.bmp` — identical output from LDS-optimized kernel
- Console prints:
  ```
  [V1 Naive    ] Debayer 4K: 28.4 ms   (35 FPS — below 4K/60 target)
  [V2 LDS Tile ] Debayer 4K:  5.9 ms   (169 FPS — target met)
  Speedup: 4.8x
  ```

Both outputs must be pixel-identical. Any difference means the LDS kernel has a boundary bug.

## Concept: The Bayer Pattern

A raw camera sensor has one photosite per pixel, covered by a color filter (RGGB pattern):
```
R G R G R G
G B G B G B
R G R G R G
G B G B G B
```
To compute full RGB at each pixel, you interpolate missing channels from neighbors (demosaicing). A green pixel at (x, y) needs R and B from its 4 diagonal neighbors. A red pixel needs G from 4 cardinal neighbors and B from 4 diagonals.

**Why V1 is slow**: each output pixel reads up to 9 neighbors from global memory. For 3840×2160, that's ~75 million global reads per frame. The same source pixel is read 4–9 times by different work-items.

**Why V2 is fast**: a 16×16 work-group loads an 18×18 tile (16 + 1-pixel halo each side) into `__local` memory — 324 global reads for 256 output pixels. All neighbor lookups within the tile hit local memory (~100× faster). Global reads drop from ~75M to ~9M per frame.

```cl
// V2: tile-based debayer (same pattern as LocalMemory toolbox)
__kernel void debayer_lds(__global uchar* raw, __global uchar4* rgba,
                          int width, int height) {
    int lx = get_local_id(0), ly = get_local_id(1);
    int gx = get_global_id(0), gy = get_global_id(1);

    __local uchar tile[18][18];   // 16x16 work-group + 1-pixel halo

    // Cooperative load including halo (boundary clamped)
    tile[ly + 1][lx + 1] = raw[clamp(gy, 0, height-1) * width
                              + clamp(gx, 0, width-1)];
    // ... load halo edges ...
    barrier(CLK_LOCAL_MEM_FENCE);

    // Bilinear interpolation from local memory
    uchar r, g, b;
    int pattern = ((gy & 1) << 1) | (gx & 1);  // 0=R, 1=Gr, 2=Gb, 3=B
    // ... interpolate based on pattern ...
    rgba[gy * width + gx] = (uchar4)(r, g, b, 255);
}
```

## Mini-Challenge

Profile V1 and V2 at 1280×720, 1920×1080, and 3840×2160 using `clGetEventProfilingInfo`. Does the LDS speedup ratio grow, shrink, or stay constant as resolution increases? Explain in terms of the tile reuse ratio (output pixels per global read).

## Troubleshooting

- **Output is a green-pink checkerboard**: raw buffer is being treated as RGB. Confirm `--width` and `--height` match the actual raw file dimensions — no header in `.raw` files.
- **V1 and V2 outputs differ at image borders**: halo boundary clamping is wrong in V2. The clamped index must stay within `[0, width-1]` × `[0, height-1]`.
- **V2 slower than V1 at small resolutions**: LDS overhead dominates when the image fits in GPU cache. The crossover is typically around 1280×720. Below that, V1 may be faster.

---

[Back to Add-ons](../Addons.md)
