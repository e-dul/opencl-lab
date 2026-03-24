// nv12_to_rgba.cl — Convert NV12 planar YUV to packed RGBA.
//
// NV12 layout:
//   [0 .. W*H-1]     : Y-plane  (one byte per pixel, full resolution)
//   [W*H .. W*H*3/2) : UV-plane (interleaved U, V pairs, half-resolution)

// WHY BT.601 limited-range:
//   NV12 is the dominant pixel format for webcams, V4L2 capture, and H.264
//   hardware decoders on x86/ARM.  These sources encode Y in [16,235] and
//   UV in [16,240] (limited-range, also called "studio swing"), following the
//   ITU-R BT.601 specification for standard-definition content.
//   Using full-range (0-255) coefficients on limited-range data produces
//   washed-out, oversaturated colours.  BT.601 limited-range coefficients
//   correctly map the signal to full-range 0-255 RGB output.

__kernel void nv12_to_rgba(__global const uchar* nv12,
                           __global uchar4*      rgba,
                           int                   width,
                           int                   height)
{
    size_t gid = get_global_id(0);
    if (gid >= (size_t)width * height) return;

    int x = (int)(gid % (size_t)width);
    int y = (int)(gid / (size_t)width);

    // Y sample — full-resolution, one byte per pixel.
    float Y = (float)nv12[gid] - 16.0f;

    // UV sample — half-resolution in both dimensions.
    // uv_base points to the first UV byte after the Y-plane.
    // WHY size_t arithmetic: width * height can exceed INT_MAX at 4K+
    // resolutions; keeping uv_base and uv_idx as size_t prevents overflow.
    size_t uv_base = (size_t)width * height;
    // Each UV pair covers a 2×2 block; (y/2)*width selects the row,
    // (x & ~1) rounds x down to the even column where U is stored.
    size_t uv_idx  = uv_base + (size_t)(y / 2) * width + (x & ~1);
    float U = (float)nv12[uv_idx]     - 128.0f;
    float V = (float)nv12[uv_idx + 1] - 128.0f;

    // BT.601 limited-range matrix (Y in [16,235], UV in [16,240] → RGB in [0,255]).
    float r = 1.164f * Y                + 1.596f * V;
    float g = 1.164f * Y - 0.392f * U  - 0.813f * V;
    float b = 1.164f * Y + 2.017f * U;

    uchar4 out;
    out.x = (uchar)clamp(r, 0.0f, 255.0f);
    out.y = (uchar)clamp(g, 0.0f, 255.0f);
    out.z = (uchar)clamp(b, 0.0f, 255.0f);
    out.w = 255;  // alpha fully opaque

    rgba[gid] = out;
}
