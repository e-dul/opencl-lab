// flip_count.cl — Dynamic voxel detection via occupancy flip counting
//
// One work-item per voxel. Compares the previous frame's OCCUPIED bit to the
// current frame's OCCUPIED bit. If they differ (a "flip"), the per-voxel
// counter is incremented atomically.
//
// Host post-processing (in main.cpp) then zeroes the occupancy of any voxel
// whose flip_count exceeds the dynamic-object threshold, removing moving
// objects from the static map.
//
// WHY separate kernel (not fused with dda_cast): the flip comparison must see
// the fully-settled current grid after all DDA rays have completed. Fusing
// would require a barrier across the whole NDRange, which OpenCL 1.2 does not
// provide. Running flip_count as a separate enqueue (after a finish/barrier)
// is the correct ordering.

#define OCCUPIED_BIT 0x2u

__kernel void count_flips(
    __global const uint* prev_grid,    // occupancy grid from previous frame
    __global const uint* curr_grid,    // occupancy grid after current DDA pass
    __global uint*       flip_counts,  // per-voxel monotonically increasing counter
    int                  total_voxels
) {
    size_t gid = get_global_id(0);
    if (gid >= (size_t)total_voxels) return;

    // Compare OCCUPIED bit between frames — any change is a "flip".
    uint prev_occ = prev_grid[gid] & OCCUPIED_BIT;
    uint curr_occ = curr_grid[gid] & OCCUPIED_BIT;

    if (prev_occ != curr_occ) {
        // WHY atomic_add: although this dispatch is strictly one work-item per
        // voxel (no intra-dispatch race), using atomic_add future-proofs the
        // counter buffer. In multi-queue or concurrent-kernel scenarios —
        // e.g., a second queue incrementing flip counts from a different sensor
        // stream — the atomic ensures correctness without changing this kernel.
        atomic_add(&flip_counts[gid], 1u);
    }
}
