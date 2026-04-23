// morton.cl — 30-bit Morton code computation (10 bits per axis).
//
// One thread per triangle:
//   1. Compute triangle centroid from vertex SoA.
//   2. Normalise centroid to [0,1]^3 using scene AABB.
//   3. Map to 10-bit integer grid [0, 1023] per axis.
//   4. Interleave bits via expand_bits() to produce a 30-bit Morton code.
//   5. Write morton_codes[gid] and initialise indices[gid] = gid.
//
// WHY 30-bit codes (not 64-bit): OpenCL 1.2 ulong is optional on some
// embedded devices. 10 bits per axis (1024^3 grid) is sufficient for ~70k
// triangles (bunny.obj) with negligible code collisions.

// expand_bits: spread 10-bit value across 30 bits by inserting two 0-bits
// between each input bit. Pattern: _ _ b9 _ _ b8 _ _ b7 ... _ _ b0
// Combined with Y<<1 and Z<<2, three values interleave into a 30-bit Morton code.
static uint expand_bits(uint v) {
    v &= 0x000003FFu;                // mask to 10 bits
    v = (v | (v << 16u)) & 0x030000FFu;
    v = (v | (v <<  8u)) & 0x0300F00Fu;
    v = (v | (v <<  4u)) & 0x030C30C3u;
    v = (v | (v <<  2u)) & 0x09249249u;
    return v;
}

// 3D Morton code: interleave 10-bit ix (X), iy (Y), iz (Z).
// X bits occupy positions 0,3,6,...; Y: 1,4,7,...; Z: 2,5,8,...
static uint morton3d(uint ix, uint iy, uint iz) {
    return expand_bits(ix) | (expand_bits(iy) << 1u) | (expand_bits(iz) << 2u);
}

__kernel void compute_morton(
    __global const float* v0x, __global const float* v0y, __global const float* v0z,
    __global const float* v1x, __global const float* v1y, __global const float* v1z,
    __global const float* v2x, __global const float* v2y, __global const float* v2z,
    __global const float* scene_aabb,    // [6]: lo.xyz, hi.xyz
    __global       uint*  morton_codes,  // [N]: output Morton codes
    __global       uint*  indices,       // [N]: output indices (initialised 0..N-1)
    int                   N)
{
    size_t gid = get_global_id(0);
    if ((int)gid >= N) return;

    // Compute triangle centroid (average of three vertices)
    float cx = (v0x[gid] + v1x[gid] + v2x[gid]) * (1.0f / 3.0f);
    float cy = (v0y[gid] + v1y[gid] + v2y[gid]) * (1.0f / 3.0f);
    float cz = (v0z[gid] + v1z[gid] + v2z[gid]) * (1.0f / 3.0f);

    // Normalise to [0,1]^3 using scene AABB
    float lo_x = scene_aabb[0], lo_y = scene_aabb[1], lo_z = scene_aabb[2];
    float hi_x = scene_aabb[3], hi_y = scene_aabb[4], hi_z = scene_aabb[5];

    float nx = clamp((cx - lo_x) / (hi_x - lo_x), 0.0f, 1.0f);
    float ny = clamp((cy - lo_y) / (hi_y - lo_y), 0.0f, 1.0f);
    float nz = clamp((cz - lo_z) / (hi_z - lo_z), 0.0f, 1.0f);

    // Map to 10-bit integer grid [0, 1023]
    uint ix = (uint)(nx * 1023.0f);
    uint iy = (uint)(ny * 1023.0f);
    uint iz = (uint)(nz * 1023.0f);

    morton_codes[gid] = morton3d(ix, iy, iz);
    indices[gid]      = (uint)gid;  // indices travel with codes through the sort
}
