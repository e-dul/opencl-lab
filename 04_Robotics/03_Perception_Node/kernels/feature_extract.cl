// feature_extract.cl — C3 Perception Node
//
// Computes cluster-level features from the compacted point cloud.
//
// For C3 the "cluster" is the entire filtered cloud (single-cluster mode).
// Each work-item handles one compacted point; contributions are accumulated
// using atomic adds on fixed-point integer accumulators (scaled by SCALE).
//
// WHY fixed-point atomics (not native float atomic_add):
//   OpenCL 1.2 does not provide native atomic_add on __global float*.
//   Integer atomic_add on scaled integers (e.g. 1000 * float_value) is the
//   standard CL 1.2 workaround. The host divides by SCALE after readback.
//
// Output layout in features[] (all float32, after host converts from int accum):
//   [0] centroid X
//   [1] centroid Y
//   [2] centroid Z
//   [3] intensity mean
//   [4] point count (as float, for /cluster_features PointCloud2 compatibility)

#define SCALE 1000

__kernel void feature_extract(
    __global const float* compact_points,  // compacted AoS input
    __global       int*   accum,           // integer accumulators [5]: x,y,z,intensity,count
    int point_step_floats,                 // floats per point
    int num_compact_points)
{
    size_t gid = get_global_id(0);
    if (gid >= (size_t)num_compact_points) return;

    size_t base = gid * (size_t)point_step_floats;

    float x         = compact_points[base + 0];
    float y         = compact_points[base + 1];
    float z         = compact_points[base + 2];
    float intensity = compact_points[base + 3];

    // Accumulate into fixed-point integer buckets.
    // WHY (int)(val * SCALE): scales float to integer range preserving 3 decimal
    // places of precision; safe for typical Lidar coordinates (< 2M / 1000 = 2000).
    atomic_add(&accum[0], (int)(x         * SCALE));
    atomic_add(&accum[1], (int)(y         * SCALE));
    atomic_add(&accum[2], (int)(z         * SCALE));
    atomic_add(&accum[3], (int)(intensity * SCALE));
    atomic_add(&accum[4], 1);
}
