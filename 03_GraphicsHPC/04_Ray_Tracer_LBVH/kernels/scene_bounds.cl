// scene_bounds.cl — Two-pass parallel reduction to compute the scene AABB.
//
// Pass 1 (scene_bounds_pass1): each work-group reduces its tile of triangles
//   to a local min/max for all 6 components (lo.xyz, hi.xyz), then writes one
//   float6 result per group into the partial buffer.
//
// Pass 2 (scene_bounds_pass2): a single work-group reduces all partial results
//   down to the final scene_aabb[6].
//
// WHY two passes: the single-pass reduction would require all N work-items to
// share one local memory — impossible when N >> max work-group size. Two passes
// keep each stage within a single work-group's local memory.
//
// Output layout: scene_aabb[0..2] = lo.xyz,  scene_aabb[3..5] = hi.xyz

// ---------------------------------------------------------------------------
// Pass 1: tile reduction
// ---------------------------------------------------------------------------
__kernel void scene_bounds_pass1(
    __global const float* v0x, __global const float* v0y, __global const float* v0z,
    __global const float* v1x, __global const float* v1y, __global const float* v1z,
    __global const float* v2x, __global const float* v2y, __global const float* v2z,
    __global float*       partial,   // [num_groups * 6]: each group writes 6 floats
    __local  float*       lmem,      // [WG_SIZE * 6]: local scratch (caller provides)
    int                   N)
{
    size_t lid   = get_local_id(0);
    size_t gid   = get_global_id(0);
    size_t grp   = get_group_id(0);
    size_t wg    = get_local_size(0);

    // Initialise local accumulators to inverse extremes
    float lo_x = +1e30f, lo_y = +1e30f, lo_z = +1e30f;
    float hi_x = -1e30f, hi_y = -1e30f, hi_z = -1e30f;

    // Each work-item accumulates a strided subset of triangles
    if ((int)gid < N) {
        float ax0 = v0x[gid], ay0 = v0y[gid], az0 = v0z[gid];
        float ax1 = v1x[gid], ay1 = v1y[gid], az1 = v1z[gid];
        float ax2 = v2x[gid], ay2 = v2y[gid], az2 = v2z[gid];

        lo_x = fmin(fmin(ax0, ax1), ax2);
        lo_y = fmin(fmin(ay0, ay1), ay2);
        lo_z = fmin(fmin(az0, az1), az2);
        hi_x = fmax(fmax(ax0, ax1), ax2);
        hi_y = fmax(fmax(ay0, ay1), ay2);
        hi_z = fmax(fmax(az0, az1), az2);
    }

    // Store in local memory: 6 planes, each of size WG_SIZE
    // Layout: lmem[0..wg-1]=lo_x, lmem[wg..2wg-1]=lo_y, ..., lmem[5*wg..6*wg-1]=hi_z
    lmem[0 * wg + lid] = lo_x;
    lmem[1 * wg + lid] = lo_y;
    lmem[2 * wg + lid] = lo_z;
    lmem[3 * wg + lid] = hi_x;
    lmem[4 * wg + lid] = hi_y;
    lmem[5 * wg + lid] = hi_z;

    barrier(CLK_LOCAL_MEM_FENCE);

    // Tree reduction within work-group
    for (size_t s = wg / 2; s > 0; s >>= 1) {
        if (lid < s) {
            lmem[0 * wg + lid] = fmin(lmem[0 * wg + lid], lmem[0 * wg + lid + s]);
            lmem[1 * wg + lid] = fmin(lmem[1 * wg + lid], lmem[1 * wg + lid + s]);
            lmem[2 * wg + lid] = fmin(lmem[2 * wg + lid], lmem[2 * wg + lid + s]);
            lmem[3 * wg + lid] = fmax(lmem[3 * wg + lid], lmem[3 * wg + lid + s]);
            lmem[4 * wg + lid] = fmax(lmem[4 * wg + lid], lmem[4 * wg + lid + s]);
            lmem[5 * wg + lid] = fmax(lmem[5 * wg + lid], lmem[5 * wg + lid + s]);
        }
        barrier(CLK_LOCAL_MEM_FENCE);
    }

    // Thread 0 writes group result to partial buffer
    if (lid == 0) {
        partial[grp * 6 + 0] = lmem[0];
        partial[grp * 6 + 1] = lmem[1 * wg];
        partial[grp * 6 + 2] = lmem[2 * wg];
        partial[grp * 6 + 3] = lmem[3 * wg];
        partial[grp * 6 + 4] = lmem[4 * wg];
        partial[grp * 6 + 5] = lmem[5 * wg];
    }
}

// ---------------------------------------------------------------------------
// Pass 2: reduce all partial results to a single scene_aabb[6].
//
// Launched with a single work-group of WG_SIZE threads.
// num_groups <= WG_SIZE (enforced by host: SORT_WG=256 groups maximum).
// ---------------------------------------------------------------------------
__kernel void scene_bounds_pass2(
    __global const float* partial,   // [num_groups * 6]
    __global       float* scene_aabb,// [6]: final scene bounds
    int                   num_groups)
{
    size_t lid = get_local_id(0);
    size_t wg  = get_local_size(0);

    float lo_x = +1e30f, lo_y = +1e30f, lo_z = +1e30f;
    float hi_x = -1e30f, hi_y = -1e30f, hi_z = -1e30f;

    // Each thread reduces a subset of group results
    for (int g = (int)lid; g < num_groups; g += (int)wg) {
        lo_x = fmin(lo_x, partial[g * 6 + 0]);
        lo_y = fmin(lo_y, partial[g * 6 + 1]);
        lo_z = fmin(lo_z, partial[g * 6 + 2]);
        hi_x = fmax(hi_x, partial[g * 6 + 3]);
        hi_y = fmax(hi_y, partial[g * 6 + 4]);
        hi_z = fmax(hi_z, partial[g * 6 + 5]);
    }

    // WHY volatile __local: prevents compiler from hoisting barrier-adjacent
    // stores out of the reduction loop when optimising for throughput.
    __local volatile float lx0[256], lx1[256], lx2[256];
    __local volatile float lx3[256], lx4[256], lx5[256];
    lx0[lid] = lo_x; lx1[lid] = lo_y; lx2[lid] = lo_z;
    lx3[lid] = hi_x; lx4[lid] = hi_y; lx5[lid] = hi_z;

    barrier(CLK_LOCAL_MEM_FENCE);

    for (size_t s = wg / 2; s > 0; s >>= 1) {
        if (lid < s) {
            lx0[lid] = fmin(lx0[lid], lx0[lid + s]);
            lx1[lid] = fmin(lx1[lid], lx1[lid + s]);
            lx2[lid] = fmin(lx2[lid], lx2[lid + s]);
            lx3[lid] = fmax(lx3[lid], lx3[lid + s]);
            lx4[lid] = fmax(lx4[lid], lx4[lid + s]);
            lx5[lid] = fmax(lx5[lid], lx5[lid + s]);
        }
        barrier(CLK_LOCAL_MEM_FENCE);
    }

    if (lid == 0) {
        float final_lo_x = lx0[0];
        float final_lo_y = lx1[0];
        float final_lo_z = lx2[0];
        float final_hi_x = lx3[0];
        float final_hi_y = lx4[0];
        float final_hi_z = lx5[0];

        // Task 3.2: guard degenerate axis extents with epsilon to avoid
        // division by zero when normalising centroids to [0,1]^3.
        const float EPSILON = 1e-5f;
        if (final_hi_x - final_lo_x < EPSILON) final_hi_x = final_lo_x + EPSILON;
        if (final_hi_y - final_lo_y < EPSILON) final_hi_y = final_lo_y + EPSILON;
        if (final_hi_z - final_lo_z < EPSILON) final_hi_z = final_lo_z + EPSILON;

        scene_aabb[0] = final_lo_x;
        scene_aabb[1] = final_lo_y;
        scene_aabb[2] = final_lo_z;
        scene_aabb[3] = final_hi_x;
        scene_aabb[4] = final_hi_y;
        scene_aabb[5] = final_hi_z;
    }
}
