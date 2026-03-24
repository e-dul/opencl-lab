// roi_blur.cl — Box blur applied only to a face ROI region.
//
// WHY global_work_offset: the host launches this kernel with
//   offset = (face.x, face.y)
//   gws    = (face.width, face.height)
// so get_global_id(0/1) yields absolute frame coordinates directly.
// No "x_local + roi_x" offset arithmetic is needed inside the kernel,
// keeping the code simpler and the compiler free to vectorise it.
//
// Data layout: BGR packed, 3 bytes per pixel (OpenCV native).
// WHY BGR (not RGBA): OpenCV VideoCapture / imread delivers BGR natively.
// Using 3-byte stride avoids any channel-conversion copy on the host side.

__kernel void roi_blur(__global uchar* img,
                       int full_width,
                       int full_height,
                       int blur_radius)
{
    // Absolute frame coordinates, set by global_work_offset on the host.
    size_t x = get_global_id(0);
    size_t y = get_global_id(1);

    // Guard: should never fire given correct NDRange, but protects against
    // driver rounding of work-group sizes.
    if (x >= (size_t)full_width || y >= (size_t)full_height) return;

    // Accumulate sum of pixels in the box [x-r, x+r] x [y-r, y+r].
    // WHY int accumulators: uchar would overflow for radius > ~7.
    int sum_b = 0, sum_g = 0, sum_r = 0;
    int count = 0;

    int r = blur_radius / 2;
    for (int dy = -r; dy <= r; ++dy) {
        int sy = (int)y + dy;
        if (sy < 0 || sy >= full_height) continue;
        for (int dx = -r; dx <= r; ++dx) {
            int sx = (int)x + dx;
            if (sx < 0 || sx >= full_width) continue;
            int idx = ((size_t)sy * full_width + sx) * 3;
            sum_b += img[idx + 0];
            sum_g += img[idx + 1];
            sum_r += img[idx + 2];
            ++count;
        }
    }

    if (count == 0) return;

    int out_idx = (y * (size_t)full_width + x) * 3;
    img[out_idx + 0] = (uchar)(sum_b / count);
    img[out_idx + 1] = (uchar)(sum_g / count);
    img[out_idx + 2] = (uchar)(sum_r / count);
}
