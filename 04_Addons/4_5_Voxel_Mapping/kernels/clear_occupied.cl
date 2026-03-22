// clear_occupied.cl — Clears only the OCCUPIED_BIT from every voxel.
//
// WHY a separate kernel instead of enqueueFillBuffer(0):
//   enqueueFillBuffer fills with a constant value — it cannot mask individual
//   bits. Using it to clear the grid zeroes FREE_BIT too, which prevents
//   free-space accumulation across frames. This kernel clears only bit 1
//   (OCCUPIED_BIT), leaving bit 0 (FREE_BIT) intact so the accumulated
//   free-space map persists while per-frame occupancy is re-evaluated fresh.
//
// Called once per frame before DDA, only when --enable-flip-filter is active.
// Allows flip_count.cl to compare last-frame occupancy (prev_grid, snapshotted
// before this clear) against current-frame occupancy (grid after DDA) without
// cumulative OCCUPIED bits masking state changes for static voxels.

#define OCCUPIED_BIT 0x2u

__kernel void clear_occupied(__global uint* grid, int total_voxels)
{
    size_t gid = get_global_id(0);
    if (gid >= (size_t)total_voxels) return;
    grid[gid] &= ~OCCUPIED_BIT;
}
