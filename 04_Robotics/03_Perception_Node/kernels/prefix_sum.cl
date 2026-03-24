// prefix_sum.cl — C3 Perception Node
//
// Two-phase Blelloch parallel exclusive prefix sum (scan) on a 1D integer array.
// Used to compute scatter indices for compacting the filtered point cloud.
//
// Multi-pass tile-based design (supports arbitrary array sizes):
//
//   Pass 1 — prefix_sum_tile:
//     Each work-group scans one tile of TILE_SIZE = 2 * LOCAL_SIZE elements.
//     Writes the exclusive prefix sum into data[], and the tile's total sum
//     into tile_sums[group_id].
//
//   Pass 2 — add_tile_offsets:
//     After the host has run an exclusive prefix sum on tile_sums[], each
//     work-group adds tile_sums[group_id] to every element in its tile.
//
// After both passes data[i] = exclusive prefix sum of original data[0..i-1].
//
// WHY Blelloch (not naive sequential scan):
//   O(log n) depth with O(n) work — work-efficient parallel scan.
//   Naive parallel scan is O(n log n) work — wasteful on wide SIMD hardware.

#define LOCAL_SIZE 128
#define TILE_SIZE  (2 * LOCAL_SIZE)   // elements handled per work-group

// ─────────────────────────────────────────────────────────────────────────────
// Scatter kernel: write points that passed the filter into compact_points[].
// Called after the two-pass prefix sum is complete.
// ─────────────────────────────────────────────────────────────────────────────
__kernel void scatter_compact(
    __global const float* points,       // original AoS input
    __global const int*   scan,         // exclusive prefix sum of mask
    __global const uchar* mask,         // original filter mask (1 = keep)
    __global       float* compact,      // output: compacted AoS points
    int point_step_floats,              // floats per point
    int num_points)
{
    size_t gid = get_global_id(0);
    if (gid >= (size_t)num_points) return;

    if (mask[gid] == 0) return;

    int    dst_idx  = scan[gid];
    size_t src_base = gid * (size_t)point_step_floats;
    size_t dst_base = (size_t)dst_idx * (size_t)point_step_floats;

    for (int f = 0; f < point_step_floats; ++f) {
        compact[dst_base + f] = points[src_base + f];
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// prefix_sum_tile — Pass 1
//
// Each work-group processes one tile of TILE_SIZE elements.
// Writes the in-tile exclusive prefix sum to data[] and the tile total to
// tile_sums[get_group_id(0)].
//
// Padding: elements beyond n are treated as 0 (handled by conditional load).
// ─────────────────────────────────────────────────────────────────────────────
__kernel __attribute__((reqd_work_group_size(LOCAL_SIZE, 1, 1)))
void prefix_sum_tile(
    __global int* data,
    __global int* tile_sums,   // one int per work-group; sized ceil(n/TILE_SIZE)
    __local  int* scratch,     // TILE_SIZE ints allocated by host
    int n)
{
    int lid   = get_local_id(0);
    int gid0  = get_group_id(0);

    // Global indices of the two elements this work-item owns.
    int base  = gid0 * TILE_SIZE;
    int idx0  = base + 2 * lid;
    int idx1  = base + 2 * lid + 1;

    // Load; pad with 0 if out of range.
    scratch[2 * lid]     = (idx0 < n) ? data[idx0] : 0;
    scratch[2 * lid + 1] = (idx1 < n) ? data[idx1] : 0;

    barrier(CLK_LOCAL_MEM_FENCE);

    // ── Phase 1: Up-sweep (reduce) ────────────────────────────────────────────
    // WHY stride <= LOCAL_SIZE (not <= TILE_SIZE): the last stride step that
    // touches the root element uses stride = LOCAL_SIZE, which maps exactly to
    // index TILE_SIZE-1.  Going further would address out-of-bounds scratch slots.
    for (int stride = 1; stride <= LOCAL_SIZE; stride <<= 1) {
        // Standard Blelloch index: right child at (2*(lid+1)*stride - 1).
        int index = (lid + 1) * stride * 2 - 1;
        if (index < TILE_SIZE) {
            scratch[index] += scratch[index - stride];
        }
        barrier(CLK_LOCAL_MEM_FENCE);
    }

    // Save total before clearing root (total = sum of all elements in tile).
    // WHY save before clear: after clearing scratch[TILE_SIZE-1] = 0, the
    // original sum is lost.  We need it for the tile_sums array used in pass 2.
    if (lid == 0) {
        tile_sums[gid0]         = scratch[TILE_SIZE - 1];
        scratch[TILE_SIZE - 1]  = 0;
    }
    barrier(CLK_LOCAL_MEM_FENCE);

    // ── Phase 2: Down-sweep ───────────────────────────────────────────────────
    for (int stride = LOCAL_SIZE; stride >= 1; stride >>= 1) {
        int index = (lid + 1) * stride * 2 - 1;
        if (index < TILE_SIZE) {
            int tmp             = scratch[index - stride];
            scratch[index - stride] = scratch[index];
            scratch[index]     += tmp;
        }
        barrier(CLK_LOCAL_MEM_FENCE);
    }

    // Write results back to global memory.
    if (idx0 < n) data[idx0] = scratch[2 * lid];
    if (idx1 < n) data[idx1] = scratch[2 * lid + 1];
}

// ─────────────────────────────────────────────────────────────────────────────
// add_tile_offsets — Pass 2
//
// After the host has computed the exclusive prefix sum of tile_sums[],
// each work-group adds its prefix offset to every element in its tile.
// This completes the global exclusive prefix sum across the full array.
// ─────────────────────────────────────────────────────────────────────────────
__kernel __attribute__((reqd_work_group_size(LOCAL_SIZE, 1, 1)))
void add_tile_offsets(
    __global int*       data,
    __global const int* tile_sums,   // exclusive-prefix-summed tile totals
    int n)
{
    int gid0   = get_group_id(0);
    int offset = tile_sums[gid0];

    int base = gid0 * TILE_SIZE;
    int idx0 = base + 2 * get_local_id(0);
    int idx1 = idx0 + 1;

    if (idx0 < n) data[idx0] += offset;
    if (idx1 < n) data[idx1] += offset;
}
