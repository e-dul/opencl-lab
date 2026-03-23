# global_work_offset & Tiled Benchmark

Use `global_work_offset` to restrict an NDRange dispatch to a sub-region of a buffer, launching zero threads outside the region of interest. This tool benchmarks a 256×256 tile against a full 4K frame to make the cost of full-frame dispatch visible.

## Symptom

You are processing a full 4K frame when only a small region of interest (ROI) needs work. Two common workarounds — skipping pixels with an `if`-statement inside the kernel, or launching a separate copy of the full NDRange — both waste GPU threads. In-kernel masking (`if (x < roi_right && y < roi_bottom)`) still launches dead threads for every pixel outside the ROI; they consume dispatch slots and scheduling resources even though they do no useful work.

**Root cause:** The NDRange is always the full frame even though only a 256×256 window matters.

## Prerequisites

- Module 1 complete (`01_Host_API/`): `cl::Event` profiling, `CL_CHECK`, `cl::Buffer`.
- OpenCL 1.2+ runtime (no extensions required — `global_work_offset` is part of the core spec).
- CMake 3.18+, a C++17 compiler.

## Build & Run

```bash
cd 05_Toolbox/GlobalWorkOffset
cmake -B build && cmake --build build

# Default: 4K frame, 256×256 tile at (0,0), 10 iterations
./build/global_work_offset

# Custom tile position
./build/global_work_offset --offset-x 512 --offset-y 256 --tile-x 512 --tile-y 512

# Pin to a specific GPU vendor
GPU=AMD ./build/global_work_offset
GPU=NVIDIA ./build/global_work_offset

# Print all flags
./build/global_work_offset --help
```

## Verify

Expected console output (values depend on hardware):

```
Platform : AMD Accelerated Parallel Processing
Device   : gfx1100

===== global_work_offset Benchmark =====
Image:        3840 x 2160
Tile:         256 x 256  @ (0, 0)
Iterations:   10

Mode                Avg (ms)    Min (ms)    Max (ms)
------------------------------------------------------
Tile (256x256)         0.042       0.038       0.051
Full frame 4K          6.831       6.714       7.012
Speedup:         162.64x  (tile vs full frame)
```

Key checks:
- Both rows appear with 3 decimal places.
- Speedup is positive and > 1×.
- No errors or exceptions on exit.

The speedup is proportional to `(frame_pixels) / (tile_pixels)`. For the default 256×256 tile on a 3840×2160 frame the theoretical ceiling is ~126×. Values in the range 50×–200× are all normal — above-theoretical results reflect memory controller effects on the full-frame pass (tile fits in L2 while full-frame saturates DRAM bandwidth).

## How it Works

### NDRange offset semantics

`enqueueNDRangeKernel` accepts three NDRange parameters:

| Parameter | Role |
|:----------|:-----|
| `global_work_offset` | Where the dispatch window *starts* in the global index space |
| `global_work_size`   | How many work-items to launch per dimension |
| `local_work_size`    | Work-group tile size |

Setting `global_work_offset = {offset_x, offset_y}` tells the runtime to dispatch threads only for the rectangle `[offset_x .. offset_x+tile_x) × [offset_y .. offset_y+tile_y)`. The GPU does not launch any work-items for pixels outside this window — no dead-thread overhead.

**Critical subtlety (OpenCL 1.2 spec §6.12.1):** `get_global_id()` returns the *absolute* global work-item ID, which **includes** the `global_work_offset`. A work-item launched at absolute position `(offset_x + 3, offset_y + 7)` sees `gx = offset_x + 3`, `gy = offset_y + 7` — not `gx = 3, gy = 7`. The IDs are already image coordinates and are used directly as buffer indices:

```c
size_t gx  = get_global_id(0);   // absolute column in the full image
size_t gy  = get_global_id(1);   // absolute row    in the full image
size_t idx = gy * (size_t)width + gx;
```

The stride is `width` (the full image row width), not `tile_x`. This is the pitch formula — you are indexing into a flat buffer that represents the entire frame. No manual offset addition is needed or correct; adding `offset_x`/`offset_y` again would double-offset every pixel access.

### Why round up global_work_size

OpenCL 1.2 requires `global_work_size` to be an exact multiple of `local_work_size` when `local_work_size` is specified. The host rounds `tile_x` and `tile_y` up to the next multiple of 16 before dispatch. The rounded-up global size may exceed the actual ROI dimensions, so the kernel guards both `gx` and `gy` against exceeding the tile boundary:

```c
if (gx >= get_global_offset(0) + (size_t)tile_w ||
    gy >= get_global_offset(1) + (size_t)tile_h) return;
```

`get_global_offset(dim)` returns the `global_work_offset` value for that dimension — always in sync with the dispatch, no extra kernel parameter required. Without this guard, work-items in the extra padded columns or rows would compute an index beyond the intended tile and corrupt adjacent buffer data.

**Alternative — skip rounding entirely:** Pass `cl::NullRange` as `local_work_size`. The runtime picks the work-group size; `global_work_size` no longer needs to be a multiple of anything, so no rounding is needed, no extra work-items are dispatched, and `tile_w`/`tile_h` kernel parameters are unnecessary. The trade-off: you lose explicit control over work-group size and occupancy.

### Thread count comparison (4K example)

| Mode | Threads dispatched | Relative cost |
|:-----|-------------------:|:--------------|
| Full frame 3840×2160 | 8,294,400 | 1× |
| Tile 256×256 | 65,536 | ~0.008× |

The speedup is proportional to `(frame_pixels) / (tile_pixels)` on compute-bound workloads and approaches the theoretical ratio on warm caches.

## Mini-Challenge

1. Change `--tile-x` and `--tile-y` to 512. The theoretical speedup ceiling drops from ~126× to ~32×. Verify the measured speedup tracks that change.
2. Move the tile to `--offset-x 1920 --offset-y 1080` (centre of the frame). The speedup should stay the same — confirm that `global_work_offset` only shifts the window, not the cost.
3. Remove the `get_global_offset()` guard from the kernel and replace it with a hard-coded offset constant. Launch with a tile size that is not a multiple of 16 (e.g. `--tile-x 100`). Observe the corrupted output and explain why it occurs.

## Troubleshooting

- **Speedup is near 1× or smaller than expected:** Your tile fits in the GPU's L2 cache while the full-frame pass is bandwidth-limited. Try `--tile-x 1024 --tile-y 1024` to push both passes into bandwidth-bound territory and get a more predictable ratio.
- **Wrong pixels written (tile appears double-shifted):** The kernel is manually adding `offset_x`/`offset_y` in addition to using `global_work_offset`. Remove the manual addition — `get_global_id()` already returns the absolute coordinate.
- **Build error — `global_work_offset` symbol not found:** Your OpenCL headers are older than 1.2. Run `sudo apt install opencl-headers` to update.
- **Tile and full-frame times are identical:** The command queue was not created with `CL_QUEUE_PROFILING_ENABLE`. Without it, `cl::Event` timestamps return zero and the benchmark reports wall-clock time, which includes driver overhead that swamps sub-millisecond tile kernels.

---

[Back to Toolbox](../Toolbox.md)
