// mad_kernel — fused multiply-add on every element of a float buffer.
//
// WHY memory-latency-bound design: we intentionally do 1 read + 1 write with
// minimal arithmetic so the bottleneck is DRAM bandwidth, not ALU throughput.
// This makes work-group sizing effects visible: occupancy changes how well the
// GPU hides memory latency via wavefront/warp switching.

__kernel void mad_kernel(__global float* buf, int n) {
    size_t gid = get_global_id(0);
    // WHY size_t cast: n is cl_int (may be negative if caller passes bad
    // value); comparing size_t gid against a raw int would implicitly
    // promote n to a huge size_t, bypassing the guard and causing UB.
    // The cast is a correctness guard, not merely a warning suppressor.
    if (gid >= (size_t)n) return;

    // One read, one FMA, one write — deliberately un-unrolled to stay
    // memory-latency-bound rather than compute-bound.
    float val = buf[gid];
    buf[gid]  = val * 1.0001f + 0.0001f;
}
