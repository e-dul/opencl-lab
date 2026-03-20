// dda_cast.cl — DDA ray casting kernel for voxel occupancy mapping
//
// One work-item per LiDAR point. Each work-item:
//  1. Converts the endpoint (x,y,z) to a voxel index.
//  2. Walks every voxel from the sensor origin to the endpoint via DDA,
//     marking FREE voxels along the ray and OCCUPIED at the endpoint.
//
// WHY atomic_or (not write): multiple rays may hit the same voxel on
// different work-items. atomic_or is race-free and preserves all bits.
//
// Voxel state encoding:
//   bit 0 (FREE_BIT)     — ray has passed through this voxel
//   bit 1 (OCCUPIED_BIT) — a LiDAR return was measured here
//   0                    — unknown (never visited)

#define FREE_BIT     0x1u
#define OCCUPIED_BIT 0x2u

__kernel void dda_cast(
    __global const float* points,    // packed XYZ triplets, stride = 3 floats
    __global uint*        grid,      // flattened voxel grid [x + y*gx + z*gx*gy]
    int4                  grid_dims, // (gx, gy, gz, 0)
    float                 resolution,
    float3                origin     // sensor origin in world metres
) {
    size_t gid = get_global_id(0);
    int    num_points = grid_dims.w; // packed: .w carries point count
    if (gid >= (size_t)num_points) return;

    // Read endpoint in world metres.
    float ex = points[gid * 3u + 0u];
    float ey = points[gid * 3u + 1u];
    float ez = points[gid * 3u + 2u];

    int gx = grid_dims.x;
    int gy = grid_dims.y;
    int gz = grid_dims.z;

    // Convert world coords to voxel indices using sensor origin as grid origin.
    // WHY half-grid offset: place the sensor at the centre of the XY plane so
    // rays can extend in all four horizontal directions.
    float half_x = (float)gx * 0.5f * resolution;
    float half_y = (float)gy * 0.5f * resolution;

    // Sensor voxel at centre of all three axes.
    // WHY centre z: placing the sensor at the z-floor discards any points below
    // the sensor (negative z in sensor frame, e.g. ground returns). Centering
    // gives equal headroom above and below, matching the XY convention.
    int ox = gx / 2;
    int oy = gy / 2;
    int oz = gz / 2;

    float half_z = (float)gz * 0.5f * resolution;

    // Endpoint voxel.
    int ex_v = (int)((ex - origin.x + half_x) / resolution);
    int ey_v = (int)((ey - origin.y + half_y) / resolution);
    int ez_v = (int)((ez - origin.z + half_z) / resolution);

    // Clamp endpoint to grid — points outside world extent are discarded.
    if (ex_v < 0 || ex_v >= gx ||
        ey_v < 0 || ey_v >= gy ||
        ez_v < 0 || ez_v >= gz) return;

    // ── 3-D DDA traversal from (ox,oy,oz) → (ex_v,ey_v,ez_v) ────────────────
    // WHY integer DDA: avoids floating-point accumulation drift over many steps.
    // Each axis advances by its step size; we pick the axis with the largest
    // delta as the driving axis to ensure every voxel is visited exactly once.

    int dx = ex_v - ox;
    int dy = ey_v - oy;
    int dz = ez_v - oz;

    int adx = abs(dx);
    int ady = abs(dy);
    int adz = abs(dz);

    int steps = max(adx, max(ady, adz));
    if (steps == 0) {
        // Origin == endpoint: mark occupied and return.
        // §7.1: promote to size_t before multiplying to prevent 32-bit overflow.
        size_t idx = (size_t)ox + (size_t)oy * gx + (size_t)oz * gx * gy;
        atomic_or(&grid[idx], OCCUPIED_BIT);
        return;
    }

    int sx = (dx > 0) ? 1 : (dx < 0 ? -1 : 0);
    int sy = (dy > 0) ? 1 : (dy < 0 ? -1 : 0);
    int sz = (dz > 0) ? 1 : (dz < 0 ? -1 : 0);

    // We drive on the axis with the most steps.
    int cx = ox, cy = oy, cz = oz;

    if (adx >= ady && adx >= adz) {
        // X is dominant.
        // Bresenham error terms scaled by 2*adx to avoid fractions.
        int ey_err = 2 * ady - adx;
        int ez_err = 2 * adz - adx;
        for (int i = 0; i < steps; ++i) {
            // Mark FREE for intermediate voxels (not the last step = endpoint).
            if (cx >= 0 && cx < gx && cy >= 0 && cy < gy && cz >= 0 && cz < gz) {
                // §7.1: promote to size_t before multiplying.
                size_t idx = (size_t)cx + (size_t)cy * gx + (size_t)cz * gx * gy;
                atomic_or(&grid[idx], FREE_BIT);
            }
            cx += sx;
            if (ey_err > 0) { cy += sy; ey_err -= 2 * adx; }
            ey_err += 2 * ady;
            if (ez_err > 0) { cz += sz; ez_err -= 2 * adx; }
            ez_err += 2 * adz;
        }
    } else if (ady >= adx && ady >= adz) {
        // Y is dominant.
        int ex_err = 2 * adx - ady;
        int ez_err = 2 * adz - ady;
        for (int i = 0; i < steps; ++i) {
            if (cx >= 0 && cx < gx && cy >= 0 && cy < gy && cz >= 0 && cz < gz) {
                // §7.1: promote to size_t before multiplying.
                size_t idx = (size_t)cx + (size_t)cy * gx + (size_t)cz * gx * gy;
                atomic_or(&grid[idx], FREE_BIT);
            }
            cy += sy;
            if (ex_err > 0) { cx += sx; ex_err -= 2 * ady; }
            ex_err += 2 * adx;
            if (ez_err > 0) { cz += sz; ez_err -= 2 * ady; }
            ez_err += 2 * adz;
        }
    } else {
        // Z is dominant.
        int ex_err = 2 * adx - adz;
        int ey_err = 2 * ady - adz;
        for (int i = 0; i < steps; ++i) {
            if (cx >= 0 && cx < gx && cy >= 0 && cy < gy && cz >= 0 && cz < gz) {
                // §7.1: promote to size_t before multiplying.
                size_t idx = (size_t)cx + (size_t)cy * gx + (size_t)cz * gx * gy;
                atomic_or(&grid[idx], FREE_BIT);
            }
            cz += sz;
            if (ex_err > 0) { cx += sx; ex_err -= 2 * adz; }
            ex_err += 2 * adx;
            if (ey_err > 0) { cy += sy; ey_err -= 2 * adz; }
            ey_err += 2 * ady;
        }
    }

    // Mark endpoint as OCCUPIED.
    // §7.1: promote to size_t before multiplying.
    size_t idx = (size_t)ex_v + (size_t)ey_v * gx + (size_t)ez_v * gx * gy;
    atomic_or(&grid[idx], OCCUPIED_BIT);
}
