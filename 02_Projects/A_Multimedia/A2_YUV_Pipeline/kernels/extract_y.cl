// extract_y.cl — Copy the Y (luminance) plane from an NV12 buffer.
//
// NV12 places the full-resolution Y plane at byte offset 0.
// One work-item per pixel simply copies its byte to the output.

__kernel void extract_y(__global const uchar* nv12,
                        __global uchar*       y_out,
                        int                   width,
                        int                   height)
{
    size_t gid = get_global_id(0);
    if (gid >= (size_t)width * height) return;

    // Y-plane starts at offset 0 in NV12; direct index into the input.
    y_out[gid] = nv12[gid];
}
