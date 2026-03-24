// filter.cl — C3 Perception Node
//
// Single-pass point filter: ground removal (z < ground_z) and intensity
// threshold (intensity < min_intensity). Writes a 1/0 predicate mask used
// by the subsequent prefix_sum compaction stage.
//
// Input layout: AoS float array. Each point occupies `point_step_floats`
// consecutive floats. Default XYZI layout (16 bytes / point):
//   offset 0: x (float)
//   offset 1: y (float)
//   offset 2: z (float)
//   offset 3: intensity (float)
//
// WHY point_step_floats is a parameter (not hard-coded 4):
// sensor_msgs/PointCloud2 supports arbitrary point formats. Hard-coding 4
// breaks with XYZ-only (12-byte) or XYZRGB (24-byte) clouds. The host reads
// point_step from the message header and passes point_step/4 here.

__kernel void filter_points(
    __global const float* points,       // AoS input, stride = point_step_floats
    __global       uchar* mask,         // output: 1 = keep, 0 = discard
    float ground_z,                     // discard points with z < ground_z
    float min_intensity,                // discard points with intensity < min_intensity
    int   point_step_floats,            // floats per point (point_step / 4)
    int   num_points)
{
    size_t gid = get_global_id(0);
    if (gid >= (size_t)num_points) return;

    // Base index of this point's float data in the AoS array.
    size_t base = gid * (size_t)point_step_floats;

    float z         = points[base + 2];
    float intensity = points[base + 3];

    // Keep the point only if it passes both filters.
    mask[gid] = (z >= ground_z && intensity >= min_intensity) ? 1 : 0;
}
