// debayer_naive.cl — V1: naive bilinear debayer for RGGB Bayer pattern.
//
// RGGB layout (row-major):
//   even row: R  Gr R  Gr ...    (col%2==0 → R,  col%2==1 → Gr)
//   odd  row: Gb B  Gb B  ...    (col%2==0 → Gb, col%2==1 → B)
//
// Each work-item produces one RGBA output pixel. All reads are clamped to
// [0, width-1] × [0, height-1] to handle image edges without conditionals.

// Clamp-and-read: returns bayer value at (cx, cy), clamped to image bounds.
#define BAYER(cx, cy) \
    bayer[clamp((int)(cy), 0, height-1) * width + clamp((int)(cx), 0, width-1)]

__kernel void debayer_naive(
    __global const uchar* bayer,
    __global       uchar4* rgba,
    int width, int height)
{
    int gx = (int)get_global_id(0);
    int gy = (int)get_global_id(1);
    if (gx >= width || gy >= height) return;

    int col_parity = gx & 1;   // 0 → even col, 1 → odd col
    int row_parity = gy & 1;   // 0 → even row, 1 → odd row

    uchar r, g, b;

    if (col_parity == 0 && row_parity == 0) {
        // R pixel: R direct, G = 4-neighbor avg, B = diagonal avg
        r = BAYER(gx,   gy);
        g = (BAYER(gx-1, gy) + BAYER(gx+1, gy) +
             BAYER(gx,   gy-1) + BAYER(gx, gy+1) + 2) / 4;
        b = (BAYER(gx-1, gy-1) + BAYER(gx+1, gy-1) +
             BAYER(gx-1, gy+1) + BAYER(gx+1, gy+1) + 2) / 4;

    } else if (col_parity == 1 && row_parity == 0) {
        // Gr pixel (green in red row): G direct, R = horiz avg, B = vert avg
        g = BAYER(gx,   gy);
        r = (BAYER(gx-1, gy) + BAYER(gx+1, gy) + 1) / 2;
        b = (BAYER(gx,   gy-1) + BAYER(gx, gy+1) + 1) / 2;

    } else if (col_parity == 0 && row_parity == 1) {
        // Gb pixel (green in blue row): G direct, R = vert avg, B = horiz avg
        g = BAYER(gx,   gy);
        r = (BAYER(gx,   gy-1) + BAYER(gx, gy+1) + 1) / 2;
        b = (BAYER(gx-1, gy) + BAYER(gx+1, gy) + 1) / 2;

    } else {
        // B pixel: B direct, G = 4-neighbor avg, R = diagonal avg
        b = BAYER(gx,   gy);
        g = (BAYER(gx-1, gy) + BAYER(gx+1, gy) +
             BAYER(gx,   gy-1) + BAYER(gx, gy+1) + 2) / 4;
        r = (BAYER(gx-1, gy-1) + BAYER(gx+1, gy-1) +
             BAYER(gx-1, gy+1) + BAYER(gx+1, gy+1) + 2) / 4;
    }

    rgba[gy * width + gx] = (uchar4)(r, g, b, 255);
}
