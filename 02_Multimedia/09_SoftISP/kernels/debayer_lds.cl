// debayer_lds.cl — V2: LDS-tiled bilinear debayer for RGGB Bayer pattern.
//
// Same algorithm as debayer_naive but loads a (TILE_H+2)×(TILE_W+2) halo
// block into __local memory before interpolation, reducing global memory
// traffic for the 8-neighborhood reads required by bilinear demosaicing.
//
// TILE_W and TILE_H are passed as compile-time defines (-DTILE_W=16 -DTILE_H=16).
// WHY halo of 1: bilinear debayer needs at most 1-pixel neighbors in each direction.

#ifndef TILE_W
#define TILE_W 16
#endif
#ifndef TILE_H
#define TILE_H 16
#endif

#define HALO_W (TILE_W + 2)
#define HALO_H (TILE_H + 2)

__kernel void debayer_lds(
    __global const uchar* bayer,
    __global       uchar4* rgba,
    int width, int height)
{
    __local uchar tile[HALO_H * HALO_W];

    int lx = (int)get_local_id(0);
    int ly = (int)get_local_id(1);
    int gx_o = (int)get_group_id(0) * TILE_W;  // tile origin in global X
    int gy_o = (int)get_group_id(1) * TILE_H;  // tile origin in global Y

    // Strided load: each work-item loads one or more halo elements.
    // WHY strided: HALO_W*HALO_H (324) > TILE_W*TILE_H (256) — not all
    // elements are covered by a 1-to-1 mapping; striding covers the remainder.
    int lid        = ly * TILE_W + lx;
    int tile_elems = HALO_W * HALO_H;
    int threads    = TILE_W * TILE_H;

    for (int idx = lid; idx < tile_elems; idx += threads) {
        int ti = idx % HALO_W;               // halo-local column [0, TILE_W+1]
        int tj = idx / HALO_W;               // halo-local row    [0, TILE_H+1]
        int gi = clamp(gx_o + ti - 1, 0, width  - 1);  // global X with halo offset
        int gj = clamp(gy_o + tj - 1, 0, height - 1);  // global Y with halo offset
        tile[idx] = bayer[gj * width + gi];
    }

    barrier(CLK_LOCAL_MEM_FENCE);

    // Global pixel coords for this work-item.
    int gx = gx_o + lx;
    int gy = gy_o + ly;
    if (gx >= width || gy >= height) return;

    // Local tile coords (offset by 1 for the halo border).
    // tile_at(dx,dy) reads a neighbor at (lx+1+dx, ly+1+dy) within the halo tile.
#define TILE_AT(dx, dy) tile[(ly + 1 + (dy)) * HALO_W + (lx + 1 + (dx))]

    int col_parity = gx & 1;
    int row_parity = gy & 1;

    uchar r, g, b;

    if (col_parity == 0 && row_parity == 0) {
        // R pixel
        r = TILE_AT( 0,  0);
        g = (TILE_AT(-1,  0) + TILE_AT( 1,  0) +
             TILE_AT( 0, -1) + TILE_AT( 0,  1) + 2) / 4;
        b = (TILE_AT(-1, -1) + TILE_AT( 1, -1) +
             TILE_AT(-1,  1) + TILE_AT( 1,  1) + 2) / 4;

    } else if (col_parity == 1 && row_parity == 0) {
        // Gr pixel
        g = TILE_AT( 0,  0);
        r = (TILE_AT(-1,  0) + TILE_AT( 1,  0) + 1) / 2;
        b = (TILE_AT( 0, -1) + TILE_AT( 0,  1) + 1) / 2;

    } else if (col_parity == 0 && row_parity == 1) {
        // Gb pixel
        g = TILE_AT( 0,  0);
        r = (TILE_AT( 0, -1) + TILE_AT( 0,  1) + 1) / 2;
        b = (TILE_AT(-1,  0) + TILE_AT( 1,  0) + 1) / 2;

    } else {
        // B pixel
        b = TILE_AT( 0,  0);
        g = (TILE_AT(-1,  0) + TILE_AT( 1,  0) +
             TILE_AT( 0, -1) + TILE_AT( 0,  1) + 2) / 4;
        r = (TILE_AT(-1, -1) + TILE_AT( 1, -1) +
             TILE_AT(-1,  1) + TILE_AT( 1,  1) + 2) / 4;
    }

    rgba[gy * width + gx] = (uchar4)(r, g, b, 255);

#undef TILE_AT
}
