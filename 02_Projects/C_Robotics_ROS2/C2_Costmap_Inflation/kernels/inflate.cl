// inflate.cl — GPU naive costmap inflation kernel (Applied OpenCL Lab, C2)
//
// Each work-item processes one output cell (gx, gy).
// Algorithm: exact Euclidean distance transform in global memory only.
// For each cell, scan the [-radius_px, radius_px] neighbourhood, find the
// nearest obstacle, and apply an exponential cost decay.
//
// WHY global memory only: this is the naive baseline. Task 038 will add the
// tiled __local version (inflate_tiled) for comparison.

__kernel void inflate(
    __global const uchar* input,   // obstacle map: 1 = obstacle, 0 = free
    __global uchar* output,        // cost map: 0–255
    int width,
    int height,
    int radius_px,                 // inflation_radius / resolution, rounded
    float decay,                   // exponential decay coefficient (e.g. 3.0)
    float resolution               // metres per pixel (e.g. 0.05)
)
{
    // §7.4: get_global_id returns size_t — use size_t to avoid narrowing.
    size_t gx = get_global_id(0);
    size_t gy = get_global_id(1);
    if (gx >= (size_t)width || gy >= (size_t)height) return;
    // Cast to int for signed arithmetic in the neighbourhood scan.
    int x = (int)gx;
    int y = (int)gy;

    size_t idx = (size_t)y * width + x;

    // Obstacle cells always carry maximum cost — they ARE the hazard.
    if (input[idx] != 0) {
        output[idx] = 255;
        return;
    }

    float min_dist_sq = (float)(radius_px + 1) * (float)(radius_px + 1);

    int x0 = max(0, x - radius_px);
    int x1 = min(width  - 1, x + radius_px);
    int y0 = max(0, y - radius_px);
    int y1 = min(height - 1, y + radius_px);

    for (int ny = y0; ny <= y1; ++ny) {
        for (int nx = x0; nx <= x1; ++nx) {
            if (input[(size_t)ny * width + nx] != 0) {
                float dx = (float)(x - nx);
                float dy = (float)(y - ny);
                float dsq = dx * dx + dy * dy;
                if (dsq < min_dist_sq) {
                    min_dist_sq = dsq;
                }
            }
        }
    }

    float radius_sq = (float)radius_px * (float)radius_px;
    if (min_dist_sq > radius_sq) {
        // No obstacle within inflation radius — cell is truly free.
        output[idx] = 0;
    } else {
        float dist_m = sqrt(min_dist_sq) * resolution;
        float cost   = 255.0f * exp(-decay * dist_m);
        // Clamp to [1, 254]: 0 means free, 255 means obstacle.
        output[idx] = (uchar)clamp((int)cost, 1, 254);
    }
}
