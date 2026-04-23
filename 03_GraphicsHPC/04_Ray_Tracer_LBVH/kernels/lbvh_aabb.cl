// lbvh_aabb.cl — Parallel AABB fitting via atomic visit counters.
//
// Algorithm:
//   One thread per leaf node (N threads for N leaves).
//   Each thread:
//     1. Computes its leaf triangle's tight AABB and stores it in nodes[N-1+i].
//     2. Atomically increments parent's visited[] counter.
//        - First to arrive (counter == 1): exit — sibling hasn't run yet.
//        - Second to arrive (counter == 2): union both children's AABBs,
//          write to parent node, increment grandparent counter, repeat.
//   This continues until one thread reaches the root.
//
// WHY float atomics via atomic_cmpxchg:
//   OpenCL 1.2 has no native float atomic min/max. We implement them by
//   reinterpreting float bits as uint and using atomic_cmpxchg spin loops.
//   This is valid for ALL floats (positive and negative) because the
//   cmpxchg checks the EXACT bit pattern before swapping — it always reads
//   and writes the same memory location atomically without any ordering
//   assumption about the float values themselves.
//
// WHY not needed for leaf writes:
//   Each leaf node is written by exactly one thread (no conflict).
//   Float atomics are used for completeness and educational value — in a
//   general parallel AABB merge scenario, atomic writes would be required.

// Must match lbvh_types.hpp LbvhNode layout (48 bytes)
typedef struct {
    int   left_child;
    int   right_child;
    int   parent;
    int   pad;
    float aabb_lo[3];
    float aabb_lo_pad;
    float aabb_hi[3];
    float aabb_hi_pad;
} LbvhNode;

// ---------------------------------------------------------------------------
// Float atomic min — safe for any float value (positive or negative).
//
// WHY atomic_cmpxchg: we read the current value, compute min, then
// attempt an atomic compare-and-swap. If another thread wrote between our
// read and swap, we retry. The spin loop converges because every iteration
// either succeeds or finds that a smaller value was already written.
//
// WHY reinterpret as uint: atomic_cmpxchg operates on integers. as_uint/as_float
// reinterpret the bit pattern without conversion — safe for any bit pattern.
// ---------------------------------------------------------------------------
static void atomic_float_min(__global volatile float* addr, float val) {
    // WHY volatile: prevents the compiler from caching *addr in a register
    // across the loop iteration — we must re-read from global memory each time.
    union { float f; uint u; } old_v, new_v;
    do {
        old_v.f = *addr;
        new_v.f = fmin(old_v.f, val);
        if (new_v.f == old_v.f) return;  // already optimal, no write needed
    } while (atomic_cmpxchg((__global volatile uint*)addr, old_v.u, new_v.u) != old_v.u);
}

// Float atomic max — same pattern as atomic_float_min.
static void atomic_float_max(__global volatile float* addr, float val) {
    union { float f; uint u; } old_v, new_v;
    do {
        old_v.f = *addr;
        new_v.f = fmax(old_v.f, val);
        if (new_v.f == old_v.f) return;
    } while (atomic_cmpxchg((__global volatile uint*)addr, old_v.u, new_v.u) != old_v.u);
}

// ---------------------------------------------------------------------------
// lbvh_fit_aabb kernel
//
// One thread per leaf (N threads). Each thread propagates upward until it
// loses the "second-to-arrive" race at some internal node.
// ---------------------------------------------------------------------------
__kernel void lbvh_fit_aabb(
    __global       LbvhNode* nodes,      // [2N-1]: topology from lbvh_build
    __global volatile int*   visited,   // [N-1]: atomic visit counters, zeroed by host
    __global const uint*     sorted_idx, // [N]: sorted triangle indices
    __global const float* v0x, __global const float* v0y, __global const float* v0z,
    __global const float* v1x, __global const float* v1y, __global const float* v1z,
    __global const float* v2x, __global const float* v2y, __global const float* v2z,
    int N)
{
    size_t gid = get_global_id(0);
    if ((int)gid >= N) return;

    int leaf_slot = (int)gid;
    int tri_idx   = (int)sorted_idx[leaf_slot];
    int leaf_node = N - 1 + leaf_slot;  // leaf node index in flat array

    // ── Step 1: Initialise leaf AABB from triangle vertices ─────────────────
    // Leaf is written by exactly one thread — no atomics needed here.
    float lx0 = v0x[tri_idx], ly0 = v0y[tri_idx], lz0 = v0z[tri_idx];
    float lx1 = v1x[tri_idx], ly1 = v1y[tri_idx], lz1 = v1z[tri_idx];
    float lx2 = v2x[tri_idx], ly2 = v2y[tri_idx], lz2 = v2z[tri_idx];

    float lo_x = fmin(fmin(lx0, lx1), lx2);
    float lo_y = fmin(fmin(ly0, ly1), ly2);
    float lo_z = fmin(fmin(lz0, lz1), lz2);
    float hi_x = fmax(fmax(lx0, lx1), lx2);
    float hi_y = fmax(fmax(ly0, ly1), ly2);
    float hi_z = fmax(fmax(lz0, lz1), lz2);

    // WHY float atomics for leaf write: educational — demonstrates the pattern.
    // Functionally equivalent to direct assignment since leaf is single-writer.
    atomic_float_min((__global volatile float*)&nodes[leaf_node].aabb_lo[0], lo_x);
    atomic_float_min((__global volatile float*)&nodes[leaf_node].aabb_lo[1], lo_y);
    atomic_float_min((__global volatile float*)&nodes[leaf_node].aabb_lo[2], lo_z);
    atomic_float_max((__global volatile float*)&nodes[leaf_node].aabb_hi[0], hi_x);
    atomic_float_max((__global volatile float*)&nodes[leaf_node].aabb_hi[1], hi_y);
    atomic_float_max((__global volatile float*)&nodes[leaf_node].aabb_hi[2], hi_z);

    // ── Step 2: Propagate upward ─────────────────────────────────────────────
    int current = leaf_node;

    // 30-bit Morton codes → tree depth ≤ 30; 32 gives one extra margin.
    for (int depth = 0; depth < 32; ++depth) {
        int parent = nodes[current].parent;
        if (parent < 0) break;  // reached root — done

        // Atomically increment parent's visit counter.
        // WHY atomic_add on int* cast: visited[] is int but we're using it as
        // an atomic counter; the volatile qualifier is already present.
        int prev_count = atomic_inc(&visited[parent]);

        if (prev_count == 0) {
            // We are the first thread to arrive — sibling hasn't written its AABB yet.
            // Exit: sibling will union both when it arrives.
            break;
        }

        // prev_count == 1: we are the SECOND thread to arrive.
        // Both children's AABBs are now fully written — union them.
        // WHY memory_fence before reading children: ensures that sibling's stores
        // (to its leaf node AABB) are visible to us after the atomic counter confirms
        // the sibling has completed. Without this, we might read stale AABB data.
        mem_fence(CLK_GLOBAL_MEM_FENCE);

        int lc = nodes[parent].left_child;
        int rc = nodes[parent].right_child;

        float p_lo_x = fmin(nodes[lc].aabb_lo[0], nodes[rc].aabb_lo[0]);
        float p_lo_y = fmin(nodes[lc].aabb_lo[1], nodes[rc].aabb_lo[1]);
        float p_lo_z = fmin(nodes[lc].aabb_lo[2], nodes[rc].aabb_lo[2]);
        float p_hi_x = fmax(nodes[lc].aabb_hi[0], nodes[rc].aabb_hi[0]);
        float p_hi_y = fmax(nodes[lc].aabb_hi[1], nodes[rc].aabb_hi[1]);
        float p_hi_z = fmax(nodes[lc].aabb_hi[2], nodes[rc].aabb_hi[2]);

        // Write parent AABB (single writer — the second-to-arrive thread)
        nodes[parent].aabb_lo[0] = p_lo_x;
        nodes[parent].aabb_lo[1] = p_lo_y;
        nodes[parent].aabb_lo[2] = p_lo_z;
        nodes[parent].aabb_hi[0] = p_hi_x;
        nodes[parent].aabb_hi[1] = p_hi_y;
        nodes[parent].aabb_hi[2] = p_hi_z;

        current = parent;  // continue propagating upward
    }
}
