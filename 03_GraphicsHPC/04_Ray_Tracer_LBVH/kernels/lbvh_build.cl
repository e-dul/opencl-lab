// lbvh_build.cl — Karras 2012 parallel binary radix tree construction.
//
// Reference: Tero Karras, "Maximizing Parallelism in the Construction of BVHs,
// Octrees, and k-d Trees", HPG 2012.
//
// Algorithm overview:
//   One thread per internal node (N-1 threads for N sorted leaves).
//   Each thread i:
//     1. determine_range(i) → [left, right]: the range of Morton-sorted leaves
//        whose common prefix is longest at position i.
//     2. find_split(left, right) → gamma: the split point where the LCP changes.
//     3. Assign left_child and right_child (leaf or internal node indices).
//     4. Write parent pointers to both children (racy but safe: each child has
//        exactly one parent, so concurrent writes never conflict).
//
// Node layout (flat array of 2N-1 LbvhNodes):
//   Internal nodes: [0 .. N-2]
//   Leaf nodes:     [N-1 .. 2N-2]   (leaf i is at index N-1+i)
//
// WHY no atomics on topology writes: each internal node is written by exactly
// one thread (thread i), and each child's parent field is written by exactly
// one thread (the thread that owns the parent). No conflicts.

// Must match lbvh_types.hpp LbvhNode layout (48 bytes, verified by static_assert)
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
// delta(i, j): longest common prefix (LCP) length of Morton codes at i and j.
//
// When codes differ: clz(codes[i] ^ codes[j]) — number of identical high bits.
// When codes are identical: clz(i ^ j) + 32 — tie-break by index.
//   WHY +32: ensures the tie-broken value is strictly greater than any 30-bit
//   XOR result (clz of a 32-bit value is in [0,31]); prevents ambiguity in
//   determine_range direction selection.
//
// Returns -1 for out-of-range j (boundary sentinel).
// ---------------------------------------------------------------------------
static int delta(__global const uint* codes, int N, int i, int j) {
    if (j < 0 || j >= N) return -1;
    if (codes[i] != codes[j])
        return clz(codes[i] ^ codes[j]);
    // WHY cast to uint before clz: clz() in OpenCL 1.2 is defined on uint.
    // XOR of two non-negative ints fits in uint safely.
    return clz((uint)(i ^ j)) + 32;
}

// ---------------------------------------------------------------------------
// determine_range(i) → [left, right]
//
// Finds the extent of the minimal range that internal node i covers.
// Uses exponential search (doubling) to bound the range, then binary search
// to find the exact boundary.
//
// WHY sign of delta difference: the range always extends in the direction
// where the LCP is longer (nodes with longer prefix are "closer" in Morton order).
// ---------------------------------------------------------------------------
static void determine_range(__global const uint* codes, int N, int i,
                             int* out_left, int* out_right)
{
    int d_right = delta(codes, N, i, i + 1);
    int d_left  = delta(codes, N, i, i - 1);
    int d       = (d_right > d_left) ? 1 : -1;
    int delta_min = delta(codes, N, i, i - d);  // LCP in opposite direction

    // Exponential search for upper bound on range length
    int l_max = 2;
    while (delta(codes, N, i, i + l_max * d) > delta_min)
        l_max <<= 1;

    // Binary search for exact range length
    int l = 0;
    for (int t = l_max >> 1; t >= 1; t >>= 1) {
        if (delta(codes, N, i, i + (l + t) * d) > delta_min)
            l += t;
    }

    *out_left  = min(i, i + l * d);
    *out_right = max(i, i + l * d);
}

// ---------------------------------------------------------------------------
// find_split(left, right) → gamma
//
// Binary search for the position gamma within [left, right-1] where the LCP
// of codes[left..gamma] and codes[gamma+1..right] differs.
// The split point divides the range into two sub-ranges for the children.
// ---------------------------------------------------------------------------
static int find_split(__global const uint* codes, int N,
                      int left, int right)
{
    int delta_node = delta(codes, N, left, right);
    int s = 0;
    int step = right - left;

    do {
        step = (step + 1) >> 1;
        if (delta(codes, N, left, left + s + step) > delta_node)
            s += step;
    } while (step > 1);

    return left + s;
}

// ---------------------------------------------------------------------------
// lbvh_build kernel
//
// One thread per internal node (gid in [0, N-2]).
// Writes left_child, right_child, and parent fields.
// WHY not writing AABBs here: the parallel AABB kernel runs after topology is
// established; keeping them separate avoids ordering dependencies between
// topology threads.
// ---------------------------------------------------------------------------
__kernel void lbvh_build(
    __global const uint*     codes,  // sorted Morton codes [N]
    __global       LbvhNode* nodes,  // flat node array [2N-1]
    int                      N)
{
    size_t gid = get_global_id(0);
    if ((int)gid >= N - 1) return;

    int i = (int)gid;

    int left, right;
    determine_range(codes, N, i, &left, &right);

    int gamma = find_split(codes, N, left, right);

    // Determine left child: leaf if gamma == left, else internal node gamma
    int left_child  = (gamma == left)     ? (N - 1 + gamma)     : gamma;
    // Determine right child: leaf if gamma+1 == right, else internal node gamma+1
    int right_child = (gamma + 1 == right) ? (N - 1 + gamma + 1) : (gamma + 1);

    nodes[i].left_child  = left_child;
    nodes[i].right_child = right_child;

    // WHY no memory fence between writes: parent writes are non-conflicting.
    // Each child has exactly one parent (proved by Karras 2012 theorem 1).
    // The ordering guarantee is provided by the host's queue.finish() after
    // this kernel completes — the AABB kernel reads parent fields only after.
    nodes[left_child].parent  = i;
    nodes[right_child].parent = i;
}
