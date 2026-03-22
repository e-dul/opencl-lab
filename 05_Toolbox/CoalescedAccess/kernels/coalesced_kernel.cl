// CoalescedAccess kernels — three access patterns on a 2D float array.
//
// All kernels multiply each element by 2 as a minimal compute workload.
// The work varies ONLY in how threads address memory, making the timing
// difference a pure measure of cache/coalescing effects.
//
// Work-items are dispatched on a 2D grid: get_global_id(0) = x (column),
//                                          get_global_id(1) = y (row).

// ─── Kernel 1: row_major (coalesced) ─────────────────────────────────────────
// Adjacent threads in the same warp/wavefront read consecutive addresses
// (x, x+1, x+2 ...) because they share the same row y.
// WHY this is fast: the hardware can merge all lane reads into a single
// cache-line transaction, maximising bus utilisation.
__kernel void row_major(
    __global const float* input,
    __global       float* output,
    int width,
    int height)
{
    size_t x = get_global_id(0);
    size_t y = get_global_id(1);

    // Guard: NDRange is padded to a multiple of the local size; skip padding.
    if (x >= (size_t)width || y >= (size_t)height) return;

    size_t idx = y * (size_t)width + x;
    output[idx] = input[idx] * 2.0f;
}

// ─── Kernel 2: col_major (uncoalesced) ───────────────────────────────────────
// Adjacent threads read addresses separated by `height` elements
// (x*height, (x+1)*height, ...), striding across multiple cache lines.
// WHY this is slow: each warp lane touches a different cache line, turning
// one ideal burst into N separate transactions.
__kernel void col_major(
    __global const float* input,
    __global       float* output,
    int width,
    int height)
{
    size_t x = get_global_id(0);
    size_t y = get_global_id(1);

    if (x >= (size_t)width || y >= (size_t)height) return;

    // Column-major index: element (x, y) lives at x*height + y.
    size_t idx = x * (size_t)height + y;
    output[idx] = input[idx] * 2.0f;
}

// ─── Kernel 3: transposed (layout-change fix) ────────────────────────────────
// Reads from a row-major input (coalesced) and writes to a column-major
// output layout. This pattern is the canonical fix when a downstream consumer
// requires column-major data: the write scatter is unavoidable, but the
// bottleneck is moved from the read (hot path) to the write (cold path),
// and write combining on most GPUs partially mitigates the scatter penalty.
__kernel void transposed(
    __global const float* input,
    __global       float* output,
    int width,
    int height)
{
    size_t x = get_global_id(0);
    size_t y = get_global_id(1);

    if (x >= (size_t)width || y >= (size_t)height) return;

    // Read coalesced (row-major), write transposed (column-major).
    size_t read_idx  = y * (size_t)width  + x;   // sequential per warp row
    size_t write_idx = x * (size_t)height + y;   // scattered per warp row
    output[write_idx] = input[read_idx] * 2.0f;
}
