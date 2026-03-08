// yuyv_to_rgba.cl — Single-pass YUYV 4:2:2 → RGBA conversion (BT.601 limited-range).
//
// WHY macropixel stride of 4 bytes per 2 pixels:
//   YUYV packs every two horizontally adjacent pixels into one 4-byte unit:
//   [Y0][U][Y1][V]. Both pixels share the same U and V chroma samples.
//   To find the chroma for pixel at column x, we round x down to the nearest
//   even column (x & ~1), then index into the 4-byte group at that position.
//   Each macropixel group starts at byte offset: (row * width + (x & ~1)) * 2.

__kernel void yuyv_to_rgba(__global const uchar* yuyv,
                           __global uchar4*      rgba,
                           int                   width,
                           int                   height)
{
    size_t gid = get_global_id(0);

    // Guard against out-of-bounds threads in the last workgroup.
    if (gid >= (size_t)width * (size_t)height) return;

    int x     = (int)(gid % (size_t)width);
    int y_row = (int)(gid / (size_t)width);

    // Locate the 4-byte macropixel this pixel belongs to.
    // x & ~1 clears the LSB, rounding x down to the even column.
    size_t macro      = (size_t)y_row * (size_t)width + (size_t)(x & ~1);
    size_t byte_start = macro * 2;

    // Extract luma: odd columns use Y1 (byte 2), even columns use Y0 (byte 0).
    uchar Y = (x % 2 == 0) ? yuyv[byte_start]     : yuyv[byte_start + 2];
    uchar U = yuyv[byte_start + 1];   // Cb — shared by the macropixel pair
    uchar V = yuyv[byte_start + 3];   // Cr — shared by the macropixel pair

    // BT.601 limited-range (16–235 Y, 16–240 UV) → full-range RGB.
    // Coefficients from ITU-R BT.601: https://www.itu.int/rec/R-REC-BT.601/
    int y_shifted = (int)Y - 16;
    int u_shifted = (int)U - 128;
    int v_shifted = (int)V - 128;

    int r = (298 * y_shifted                 + 409 * v_shifted + 128) >> 8;
    int g = (298 * y_shifted - 100 * u_shifted - 208 * v_shifted + 128) >> 8;
    int b = (298 * y_shifted + 516 * u_shifted                 + 128) >> 8;

    // Clamp to [0, 255] to handle limited-range edge values.
    uchar4 out;
    out.x = (uchar)clamp(r, 0, 255);
    out.y = (uchar)clamp(g, 0, 255);
    out.z = (uchar)clamp(b, 0, 255);
    out.w = 255;  // Alpha fully opaque

    rgba[gid] = out;
}
