// radix_sort.cl — 4-pass LSD radix sort over uint keys with uint payload.
//
// Three kernels compose one radix sort pass (8 bits / pass = 4 passes total):
//
//   1. radix_histogram  — count per-group digit frequencies into hist[G×256]
//   2. radix_prefix_scan — Blelloch exclusive scan over global digit totals;
//                          also computes per-group-per-digit prefix sums for scatter
//   3. radix_scatter    — write (key, index) pairs to sorted destination
//
// Host dispatch loop (lbvh_builder.hpp):
//   for pass in 0..3:
//     histogram(key_in, hist) → scan(hist, global_offset) → scatter(key_in, idx_in, key_out, idx_out)
//     swap(key_in, key_out); swap(idx_in, idx_out)
//   After 4 swaps result is back in the _a buffers.
//
// WHY 8 bits per pass: trades 4 kernel launches for a 256-bucket histogram.
// Fewer passes = fewer kernel-launch overheads; 256 buckets fit in local memory.

// ---------------------------------------------------------------------------
// 1. radix_histogram — per-group digit frequency table
//
// Each work-group processes a contiguous chunk of the key array.
// Local memory holds 256 uint counters, atomically incremented.
// On completion, writes hist[group_id * 256 + digit] = count.
//
// WHY local memory atomics: avoids global atomic contention across groups.
// Each group has its own 256-bucket histogram in fast local memory.
// ---------------------------------------------------------------------------
__kernel void radix_histogram(
    __global const uint* keys,   // input key array
    __global       uint* hist,   // output [num_groups * 256]
    int                  N,
    int                  pass,   // 0..3: which byte to extract (LSB first)
    __local        uint* lhist)  // [256]: local digit counters
{
    size_t lid  = get_local_id(0);
    size_t gid  = get_global_id(0);
    size_t grp  = get_group_id(0);
    size_t wg   = get_local_size(0);

    // Zero local histogram
    for (size_t i = lid; i < 256; i += wg)
        lhist[i] = 0;
    barrier(CLK_LOCAL_MEM_FENCE);

    // Tally digit for this work-item's element
    if ((int)gid < N) {
        uint digit = (keys[gid] >> (pass * 8)) & 0xFFu;
        atomic_inc(&lhist[digit]);
    }
    barrier(CLK_LOCAL_MEM_FENCE);

    // Flush local histogram to global buffer
    // hist layout: hist[grp * 256 + digit] = count of digit in this group
    for (size_t i = lid; i < 256; i += wg)
        hist[grp * 256 + i] = lhist[i];
}

// ---------------------------------------------------------------------------
// 2. radix_prefix_scan — Blelloch exclusive scan over global histogram.
//
// Two responsibilities in one kernel (single work-group of 256 threads):
//   a) Reduce per-group histograms to global digit totals.
//   b) Blelloch exclusive prefix scan over the 256 global totals →
//      global_offset[256]: the scatter base address for each digit.
//   c) Back-propagate to build per-group scatter offsets in hist[]:
//      hist[g * 256 + d] ← global_offset[d] + sum_{g'<g} original_hist[g'*256+d]
//      These become the scatter base for group g, digit d.
//
// WHY Blelloch (not serial): O(N) work, O(log N) depth — demonstrates
// the canonical GPU prefix scan pattern. For 256 elements it's one wave.
// ---------------------------------------------------------------------------
__kernel void radix_prefix_scan(
    __global uint*  hist,          // [num_groups * 256] in; scatter offsets out
    __global uint*  global_offset, // [256] out: exclusive prefix sums
    int             num_groups,
    __local  uint*  ldata)         // [256]: Blelloch scan scratch
{
    size_t lid = get_local_id(0);  // lid in [0, 255]

    // ── Step a: sum column 'lid' across all groups ─────────────────────────
    uint col_sum = 0;
    for (int g = 0; g < num_groups; ++g)
        col_sum += hist[g * 256 + lid];

    ldata[lid] = col_sum;
    barrier(CLK_LOCAL_MEM_FENCE);

    // ── Step b: Blelloch exclusive prefix scan on ldata[256] ───────────────
    // Up-sweep (reduce)
    for (int stride = 1; stride < 256; stride <<= 1) {
        int idx = ((int)lid + 1) * (stride << 1) - 1;
        if (idx < 256)
            ldata[idx] += ldata[idx - stride];
        barrier(CLK_LOCAL_MEM_FENCE);
    }

    // Set last element to 0 (exclusive scan identity)
    if (lid == 255) ldata[255] = 0;
    barrier(CLK_LOCAL_MEM_FENCE);

    // Down-sweep
    for (int stride = 128; stride >= 1; stride >>= 1) {
        int idx = ((int)lid + 1) * (stride << 1) - 1;
        if (idx < 256) {
            uint tmp      = ldata[idx - stride];
            ldata[idx - stride] = ldata[idx];
            ldata[idx]          = tmp + ldata[idx];
        }
        barrier(CLK_LOCAL_MEM_FENCE);
    }

    // ldata[lid] is now the exclusive prefix sum (global_offset[lid])
    global_offset[lid] = ldata[lid];
    barrier(CLK_LOCAL_MEM_FENCE);

    // ── Step c: rewrite hist[g*256+d] as scatter base for (g, d) ──────────
    // scatter_base[g][d] = global_offset[d] + sum_{g'<g} original_hist[g'*256+d]
    //
    // WHY in-place: the scatter kernel only needs the final scatter base per
    // (group, digit); storing it back in hist[] avoids an extra buffer.
    uint base = ldata[lid];  // global_offset[lid] (digit = lid)
    for (int g = 0; g < num_groups; ++g) {
        uint count = hist[g * 256 + lid];
        hist[g * 256 + lid] = base;  // overwrite with scatter base
        base += count;
    }
}

// ---------------------------------------------------------------------------
// 3. radix_scatter — stable scatter of (key, index) pairs to sorted output.
//
// WHY stable: LSD radix sort correctness depends on each pass being a stable
// sort. If two elements have the same digit in pass k, their relative order
// (established by passes 0..k-1) must be preserved. An unstable scatter
// (e.g. atomic_inc in execution order) breaks this invariant and corrupts
// the final sort when the same key appears more than once.
//
// Stability implementation: each work-item computes its within-group rank by
// counting how many elements earlier in the group (lower lid) have the same
// digit. This is O(WG_SIZE) per work-item (O(WG_SIZE²) per group total) but
// executes from L1-speed local memory and is correct for all inputs.
//
// dest = scatter_base_for(group, digit) + within_group_rank
// ---------------------------------------------------------------------------
__kernel void radix_scatter(
    __global const uint* keys_in,       // input keys
    __global const uint* idx_in,        // input triangle indices
    __global       uint* keys_out,      // output sorted keys
    __global       uint* idx_out,       // output sorted indices
    __global const uint* hist,          // [num_groups * 256]: scatter bases from scan
    __global const uint* global_offset, // [256]: unused (bases already baked into hist)
    int                  N,
    int                  pass,
    __local        uint* ldigits)       // [WG_SIZE]: local digit cache
{
    size_t lid  = get_local_id(0);
    size_t gid  = get_global_id(0);
    size_t grp  = get_group_id(0);
    size_t wg   = get_local_size(0);

    // Cache all digits for this group in local memory for fast rank computation
    uint my_key   = ((int)gid < N) ? keys_in[gid] : 0xFFFFFFFFu;
    uint my_idx   = ((int)gid < N) ? idx_in[gid]  : 0u;
    uint my_digit = (my_key >> (pass * 8)) & 0xFFu;

    ldigits[lid] = my_digit;
    barrier(CLK_LOCAL_MEM_FENCE);

    if ((int)gid < N) {
        // Count elements before me (lid 0..lid-1) with the same digit.
        // WHY scan over local memory (not atomic_inc): atomic_inc assigns
        // positions in execution order — not input order — breaking stability.
        // This serial scan over local memory is O(WG_SIZE) per thread but
        // reads from fast on-chip local memory (~1 cycle per access).
        uint rank = 0;
        for (size_t j = 0; j < lid; ++j)
            rank += (ldigits[j] == my_digit) ? 1u : 0u;

        uint dest      = hist[grp * 256 + my_digit] + rank;
        keys_out[dest] = my_key;
        idx_out[dest]  = my_idx;
    }
}
