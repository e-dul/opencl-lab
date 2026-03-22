// blur_kernel.cl — Box blur in two variants for local memory benchmarking.
//
// Both kernels operate on a 2D grayscale image (uchar, 1 channel).
// Compile-time defines (set by host via -D):
//   LOCAL_SIZE  — work-group edge dimension (default 16)
//   MAX_RADIUS  — maximum blur radius that fits in the static tile array

// ---------------------------------------------------------------------------
// blur_global: naive box blur reading directly from global memory.
//
// WHY this is the baseline: every accumulator step issues a global load,
// causing (2r+1)^2 round-trips to DRAM per output pixel.  Adjacent work-items
// re-read overlapping neighbourhoods, so there is no data reuse.
// ---------------------------------------------------------------------------
__kernel void blur_global(
    __global const uchar* input,
    __global       uchar* output,
    int width,
    int height,
    int radius)
{
    // WHY size_t: get_global_id() returns size_t; using int would cause
    // signed/unsigned mismatch warnings and potential wrapping on large grids.
    size_t gx = get_global_id(0);
    size_t gy = get_global_id(1);

    // Guard extra work-items introduced by NDRange padding to a multiple of LOCAL_SIZE.
    if (gx >= (size_t)width || gy >= (size_t)height) return;

    int sum = 0;
    int count = 0;

    for (int dy = -radius; dy <= radius; ++dy) {
        for (int dx = -radius; dx <= radius; ++dx) {
            // Clamp to valid image bounds (mirror-free clamping).
            int sx = clamp((int)gx + dx, 0, width  - 1);
            int sy = clamp((int)gy + dy, 0, height - 1);
            // WHY (size_t)sy: promotes first operand before multiplying to
            // prevent int overflow when sy * width exceeds INT_MAX.
            sum += input[(size_t)sy * width + sx];
            ++count;
        }
    }

    // WHY (size_t)gy: same overflow prevention for the output write index.
    output[(size_t)gy * width + gx] = (uchar)(sum / count);
}

// ---------------------------------------------------------------------------
// blur_local: tile + halo pattern — the canonical local-memory optimisation.
//
// WHY this is faster: the (LOCAL_SIZE + 2*MAX_RADIUS)^2 tile is loaded once
// from global memory cooperatively by the work-group, placed in __local
// (on-chip SRAM, ~100x lower latency than global).  The blur loop then reads
// only from the tile — eliminating redundant global accesses entirely.
//
// Tile layout (example: LOCAL_SIZE=16, radius=5 → tile is 26×26):
//
//   ┌────────────────────┐
//   │  halo (radius px)  │  ← loaded from clamped global coords
//   │  ┌──────────────┐  │
//   │  │  core 16×16  │  │  ← each work-item loads its own pixel
//   │  └──────────────┘  │
//   │  halo (radius px)  │
//   └────────────────────┘
// ---------------------------------------------------------------------------

// WHY file-scope: OpenCL requires __local array sizes to be compile-time
// constants. Defining TILE_DIM here ensures it is visible before the
// kernel body and avoids issues with some OpenCL compilers that reject
// macro definitions inside function bodies.
#define TILE_DIM (LOCAL_SIZE + 2 * MAX_RADIUS)

__kernel void blur_local(
    __global const uchar* input,
    __global       uchar* output,
    int width,
    int height,
    int radius)
{
    // WHY compile-time tile size: OpenCL 1.2 requires __local array size to be
    // a compile-time constant.  MAX_RADIUS is injected via -D at build time,
    // sized to the user-requested radius so the tile is never over-allocated.
    __local uchar tile[TILE_DIM * TILE_DIM];

    int lx = (int)get_local_id(0);
    int ly = (int)get_local_id(1);
    size_t gx_s = get_global_id(0);
    size_t gy_s = get_global_id(1);
    int gx = (int)gx_s;
    int gy = (int)gy_s;

    // Top-left corner of the tile in global image coordinates (includes halo offset).
    int tile_origin_x = gx - lx - radius;
    int tile_origin_y = gy - ly - radius;

    // --- Phase 1: Cooperatively load the full tile (core + halo) ---------------
    // WHY flat-index distribution: TILE_DIM^2 may exceed the work-group size
    // (LOCAL_SIZE^2), so we use a stride loop: each work-item handles cells
    // whose flat index in [0, TILE_DIM^2) is congruent to its flat local id
    // modulo the work-group size.  This ensures all cells are covered regardless
    // of TILE_DIM vs work-group-size ratio.
    int local_flat_id = ly * LOCAL_SIZE + lx;
    int group_size    = LOCAL_SIZE * LOCAL_SIZE;
    int tile_pixels   = TILE_DIM * TILE_DIM;

    for (int i = local_flat_id; i < tile_pixels; i += group_size) {
        int tx = i % TILE_DIM;
        int ty = i / TILE_DIM;

        // Map tile-local coordinates to global image coordinates (clamp to border).
        int sx = clamp(tile_origin_x + tx, 0, width  - 1);
        int sy = clamp(tile_origin_y + ty, 0, height - 1);

        // WHY (size_t)sy: promotes first operand before multiplying to
        // prevent int overflow when sy * width exceeds INT_MAX.
        tile[i] = input[(size_t)sy * width + sx];
    }

    // WHY barrier here: ALL work-items in the group must finish writing the tile
    // before ANY work-item reads from it.  Without this, a work-item may read a
    // halo cell that a neighbour has not yet written — producing data races and
    // visually noisy output.
    barrier(CLK_LOCAL_MEM_FENCE);

    // WHY: guard is placed AFTER barrier() — all work-items including out-of-bounds
    // ones must reach the barrier to avoid deadlock. The tile-load loop uses clamped
    // coordinates, so OOB work-items load valid data and their writes are harmless.
    if (gx >= width || gy >= height) return;

    // --- Phase 2: Box blur from local tile (no global reads) -------------------
    int sum   = 0;
    int count = 0;

    // Local coordinates of this work-item *within the tile* (offset by radius).
    int tile_lx = lx + radius;
    int tile_ly = ly + radius;

    for (int dy = -radius; dy <= radius; ++dy) {
        for (int dx = -radius; dx <= radius; ++dx) {
            sum += tile[(tile_ly + dy) * TILE_DIM + (tile_lx + dx)];
            ++count;
        }
    }

    // WHY (size_t)gy: same overflow prevention for the output write index.
    output[(size_t)gy * width + gx] = (uchar)(sum / count);
}
