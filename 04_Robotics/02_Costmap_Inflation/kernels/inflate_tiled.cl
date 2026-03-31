// inflate_tiled.cl — C2 Challenge: LDS Tiled Inflate Kernel (Task 038)
//
// Each work-group cooperatively loads a TILE_W × TILE_H patch of the input
// plus a halo of radius_px cells on every side into __local memory.  Once
// local memory is populated the distance scan reads exclusively from __local,
// reducing global-memory traffic from O(radius_px²) reads per work-item to
// O(1) amortised reads per work-item.
//
// WHY 16×16 tile: 16 is the de-facto sweet spot on most discrete GPUs
// (matches common SIMD widths and LDS bank counts).  A full parameter sweep
// (4, 8, 16, 32) can be added later without changing the kernel signature.

// WHY ifndef guard: TILE_W / TILE_H are exposed as -D TILE_W=... -D TILE_H=...
// build-time macros from build_program() so students can vary tile size without
// editing the kernel source. The defaults match the classic GPU sweet spot.
#ifndef TILE_W
#  define TILE_W 16
#endif
#ifndef TILE_H
#  define TILE_H 16
#endif

__kernel void inflate_tiled(
    __global const uchar* input,   // obstacle map: 1 = obstacle, 0 = free
    __global uchar* output,        // cost map: 0–255
    int width,
    int height,
    int radius_px,
    float decay,
    float resolution,
    __local uchar* tile            // arg 7: halo-padded local tile, size set by host
)
{
    // Halo-padded local tile dimensions.
    // lw × lh = (TILE_W + 2*radius_px) × (TILE_H + 2*radius_px)
    int lw = TILE_W + 2 * radius_px;
    int lh = TILE_H + 2 * radius_px;

    // Work-group origin in global coords (top-left corner of the TILE_W×TILE_H patch).
    int group_origin_x = (int)get_group_id(0) * TILE_W;
    int group_origin_y = (int)get_group_id(1) * TILE_H;

    // Each work-item's position within the tile (0..TILE_W-1, 0..TILE_H-1).
    int lx = (int)get_local_id(0);
    int ly = (int)get_local_id(1);

    // ─── WHY block: halo design rationale ────────────────────────────────────
    // WHY halo width = radius_px on every side:
    //   A work-item at the tile edge (e.g., lx=0) must scan up to radius_px
    //   cells to its left in global space.  The halo must cover exactly that
    //   reach so every output cell can read its full search window from local
    //   memory without falling back to global memory.
    //
    // WHY barrier(CLK_LOCAL_MEM_FENCE) is mandatory:
    //   OpenCL does not guarantee that a write by work-item A is visible to
    //   work-item B before the barrier.  A work-item that finishes its halo
    //   stores early could read stale (uninitialized) data written by slower
    //   work-items if there were no barrier.  The barrier synchronises ALL
    //   work-items in the group and flushes local memory before any work-item
    //   proceeds to the scan phase.
    // ─────────────────────────────────────────────────────────────────────────

    // ── Cooperative halo load ─────────────────────────────────────────────────
    // Total cells in the halo tile: lw * lh, which can exceed TILE_W * TILE_H
    // when radius_px > 0.  We use a strided flat-index loop so that larger
    // tiles are still fully loaded even when radius_px > 8.
    int tile_area     = lw * lh;
    int group_size    = TILE_W * TILE_H;
    int flat_start    = ly * TILE_W + lx;   // this work-item's first assignment

    for (int fi = flat_start; fi < tile_area; fi += group_size) {
        int local_col  = fi % lw;
        int local_row  = fi / lw;

        // Map local tile coordinate to global input coordinate, clamping to
        // image bounds (replicates border pixels — avoids branching on edges).
        int gx_load = group_origin_x - radius_px + local_col;
        int gy_load = group_origin_y - radius_px + local_row;
        gx_load = clamp(gx_load, 0, width  - 1);
        gy_load = clamp(gy_load, 0, height - 1);

        tile[fi] = input[(size_t)gy_load * width + gx_load];
    }

    // Synchronise: every work-item must finish its stores before ANY work-item
    // reads from tile[] in the distance-scan phase below.
    barrier(CLK_LOCAL_MEM_FENCE);

    // ── Out-of-bounds guard (global output index) ─────────────────────────────
    // §7.4: get_global_id returns size_t.
    size_t gx = get_global_id(0);
    size_t gy = get_global_id(1);
    if (gx >= (size_t)width || gy >= (size_t)height) return;

    size_t out_idx = gy * (size_t)width + gx;

    // Obstacle cells are assigned maximum cost immediately.
    // Local tile coord for this cell: (lx + radius_px, ly + radius_px)
    if (tile[(ly + radius_px) * lw + (lx + radius_px)] != 0) {
        output[out_idx] = 255;
        return;
    }

    // ── Distance scan in local memory ─────────────────────────────────────────
    // Window in local tile coords: [lx, lx+2*radius_px] × [ly, ly+2*radius_px]
    // This corresponds exactly to the [-radius_px, +radius_px] search window
    // in global space, but accessed via __local memory.
    float min_dist_sq = (float)(radius_px + 1) * (float)(radius_px + 1);

    for (int dy = 0; dy <= 2 * radius_px; ++dy) {
        for (int dx = 0; dx <= 2 * radius_px; ++dx) {
            int tc = (lx + dx);   // tile column
            int tr = (ly + dy);   // tile row
            if (tile[tr * lw + tc] != 0) {
                float fdx   = (float)(dx - radius_px);
                float fdy   = (float)(dy - radius_px);
                float dsq   = fdx * fdx + fdy * fdy;
                if (dsq < min_dist_sq) {
                    min_dist_sq = dsq;
                }
            }
        }
    }

    // Apply exponential cost decay; clamp to [1,254] to distinguish inflated
    // cells from true obstacles (255) and truly free cells (0).
    float radius_sq = (float)radius_px * (float)radius_px;
    if (min_dist_sq > radius_sq) {
        output[out_idx] = 0;
    } else {
        float dist_m = sqrt(min_dist_sq) * resolution;
        float cost   = 255.0f * exp(-decay * dist_m);
        output[out_idx] = (uchar)clamp((int)cost, 1, 254);
    }
}
