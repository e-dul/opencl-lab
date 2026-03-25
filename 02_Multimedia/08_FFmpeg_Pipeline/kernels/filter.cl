// filter.cl — Apply a visual effect to each RGBA frame.
//
// Compiled with exactly one of:
//   -D EFFECT_BLUR   → 5×5 box blur
//   -D EFFECT_SEPIA  → sepia tone matrix
// Default (neither flag): passthrough copy.
//
// WHY CLK_NORMALIZED_COORDS_FALSE | CLK_ADDRESS_CLAMP_TO_EDGE | CLK_FILTER_NEAREST:
//   - NORMALIZED_COORDS_FALSE: pixel coords are passed as integers (0..width-1),
//     so no normalisation is needed and float rounding errors are avoided.
//   - ADDRESS_CLAMP_TO_EDGE: border samples replicate the edge pixel instead of
//     returning zero; prevents dark halos in the blur near image boundaries.
//   - FILTER_NEAREST: we compute blur weights manually in the kernel; letting the
//     GPU interpolate as well would double-apply filtering and corrupt the result.
__constant sampler_t SAMPLER = CLK_NORMALIZED_COORDS_FALSE
                              | CLK_ADDRESS_CLAMP_TO_EDGE
                              | CLK_FILTER_NEAREST;

__kernel void apply_filter(
    __read_only  image2d_t src,
    __write_only image2d_t dst,
    int width,
    int height)
{
    size_t gid_x = get_global_id(0);
    size_t gid_y = get_global_id(1);

    if (gid_x >= (size_t)width || gid_y >= (size_t)height) return;

    int2 coord = (int2)((int)gid_x, (int)gid_y);

#ifdef EFFECT_BLUR
    // 5×5 box blur — uniform average over 25 neighbouring pixels.
    // Boundary clamping is handled by SAMPLER (CLK_ADDRESS_CLAMP_TO_EDGE),
    // so no explicit bounds checks are needed inside the loop.
    float4 acc = (float4)(0.0f, 0.0f, 0.0f, 0.0f);
    for (int dy = -2; dy <= 2; ++dy) {
        for (int dx = -2; dx <= 2; ++dx) {
            int2 nc = (int2)(coord.x + dx, coord.y + dy);
            acc += read_imagef(src, SAMPLER, nc);
        }
    }
    float4 result = acc / 25.0f;

#elif defined(EFFECT_SEPIA)
    // Sepia tone: standard photographic sepia matrix applied to RGB channels.
    // Alpha channel is passed through unchanged.
    float4 px = read_imagef(src, SAMPLER, coord);
    float r = px.x * 0.393f + px.y * 0.769f + px.z * 0.189f;
    float g = px.x * 0.349f + px.y * 0.686f + px.z * 0.168f;
    float b = px.x * 0.272f + px.y * 0.534f + px.z * 0.131f;
    float4 result = (float4)(clamp(r, 0.0f, 1.0f),
                              clamp(g, 0.0f, 1.0f),
                              clamp(b, 0.0f, 1.0f),
                              px.w);

#else
    // Passthrough: copy pixel without modification.
    float4 result = read_imagef(src, SAMPLER, coord);
#endif

    write_imagef(dst, coord, result);
}
