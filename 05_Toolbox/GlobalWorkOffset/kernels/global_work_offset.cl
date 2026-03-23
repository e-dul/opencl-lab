// global_work_offset.cl — ROI invert kernel demonstrating global_work_offset semantics.
//
// KEY INSIGHTS (OpenCL 1.2 spec §6.11.1):
//
// 1. get_global_id() returns the *absolute* global work-item ID, which INCLUDES
//    the global_work_offset supplied by the host.  With global_work_offset={100,200}
//    and global_work_size={256,256}, work-items receive gx ∈ [100,355] and
//    gy ∈ [200,455] — they are already image coordinates.
//
// 2. get_global_offset(dim) returns the exact global_work_offset for that dimension.
//    This is always in sync with the dispatch and requires no host-side argument.
//    Use it instead of passing offset_x/offset_y as explicit kernel parameters.
//
// Example: host dispatches global_work_offset={100,200}, global_work_size={256,256}.
//   Work-item (0,0) gets gx=100, gy=200 → buffer index = 200*width+100.
//   Work-item (1,0) gets gx=101, gy=200 → buffer index = 200*width+101.
// No work-item is launched for pixels outside [100..355] x [200..455].

__kernel void roi_invert(
    __global const uchar* input,
    __global       uchar* output,
    int width,    // full image width (stride/pitch in pixels)
    int height,   // full image height (rows)
    int tile_w,   // tile width  — actual ROI columns, before round-up to LOCAL_SIZE
    int tile_h    // tile height — actual ROI rows,    before round-up to LOCAL_SIZE
)
{
    // WHY size_t: get_global_id() returns size_t; using int would require a
    // signed/unsigned cast and could produce warnings on 64-bit platforms.
    // gx and gy are already absolute pixel coordinates in the full image because
    // the runtime added global_work_offset before the kernel sees these IDs.
    size_t gx = get_global_id(0);
    size_t gy = get_global_id(1);

    // Full-image bounds guard: protects against any pathological dispatch.
    if (gx >= (size_t)width || gy >= (size_t)height) return;

    // Tile-bounds guard: the host rounds tile_w/tile_h up to the LOCAL_SIZE boundary
    // so that global_work_size is a multiple of local_work_size (required by OpenCL 1.2).
    // The rounded-up global size may exceed the actual ROI, launching work-items
    // beyond offset+tile.  Those items must not write to the buffer.
    // get_global_offset() retrieves the host-supplied global_work_offset — no
    // explicit param needed; the value is always in sync with the dispatch.
    if (gx >= get_global_offset(0) + (size_t)tile_w ||
        gy >= get_global_offset(1) + (size_t)tile_h) return;

    // gx and gy are absolute image coordinates — use them directly as buffer indices.
    // The stride is `width` (one full row of the image), not the tile width.
    size_t idx = gy * (size_t)width + gx;

    // Simple pixel invert — makes the ROI visually distinct in a real pipeline.
    output[idx] = 255 - input[idx];
}
