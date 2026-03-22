// yuyv_to_rgba_twopass.cl — Two-pass YUYV 4:2:2 → RGBA conversion.
//
// Pass 1 (extract_y_from_yuyv): Extract only the Y (luma) plane from YUYV into
//   a separate buffer. Even pixels use byte 0 of the macropixel; odd use byte 2.
//   WHY a separate pass: decouples luma extraction from chroma reconstruction,
//   enabling potential pipelining and demonstrating staged GPU workload design.
//
// Pass 2 (reconstruct_rgba_twopass): Read Y from the pre-extracted plane, read
//   U/V from the original YUYV buffer, and apply BT.601 limited-range conversion.

// ── Pass 1: Extract Y plane ───────────────────────────────────────────────────
__kernel void extract_y_from_yuyv(__global const uchar* yuyv,
                                  __global uchar*       y_plane,
                                  int                   width,
                                  int                   height)
{
    size_t gid = get_global_id(0);
    if (gid >= (size_t)width * (size_t)height) return;

    int x     = (int)(gid % (size_t)width);
    int y_row = (int)(gid / (size_t)width);

    size_t macro      = (size_t)y_row * (size_t)width + (size_t)(x & ~1);
    size_t byte_start = macro * 2;

    // Even column → byte 0 (Y0), odd column → byte 2 (Y1) of the macropixel.
    y_plane[gid] = (x % 2 == 0) ? yuyv[byte_start] : yuyv[byte_start + 2];
}

// ── Pass 2: Reconstruct RGBA from Y plane + YUYV chroma ──────────────────────
__kernel void reconstruct_rgba_twopass(__global const uchar* yuyv,
                                       __global const uchar* y_plane,
                                       __global uchar4*      rgba,
                                       int                   width,
                                       int                   height)
{
    size_t gid = get_global_id(0);
    if (gid >= (size_t)width * (size_t)height) return;

    int x     = (int)(gid % (size_t)width);
    int y_row = (int)(gid / (size_t)width);

    size_t macro      = (size_t)y_row * (size_t)width + (size_t)(x & ~1);
    size_t byte_start = macro * 2;

    // Read luma from the pre-computed Y plane (avoids recomputing column parity).
    uchar Y = y_plane[gid];
    uchar U = yuyv[byte_start + 1];   // Cb shared by the macropixel pair
    uchar V = yuyv[byte_start + 3];   // Cr shared by the macropixel pair

    // BT.601 limited-range → full-range RGB (same coefficients as single-pass).
    int y_shifted = (int)Y - 16;
    int u_shifted = (int)U - 128;
    int v_shifted = (int)V - 128;

    int r = (298 * y_shifted                  + 409 * v_shifted + 128) >> 8;
    int g = (298 * y_shifted - 100 * u_shifted - 208 * v_shifted + 128) >> 8;
    int b = (298 * y_shifted + 516 * u_shifted                  + 128) >> 8;

    uchar4 out;
    out.x = (uchar)clamp(r, 0, 255);
    out.y = (uchar)clamp(g, 0, 255);
    out.z = (uchar)clamp(b, 0, 255);
    out.w = 255;

    rgba[gid] = out;
}
