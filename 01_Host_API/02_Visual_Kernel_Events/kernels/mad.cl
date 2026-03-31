/*
 * mad_kernel — Multiply-Add pixel filter (flat byte buffer).
 *
 * One work-item per byte: output[gid] = clamp(src[gid] * contrast + brightness, 0, 255)
 *
 * WHY flat bytes: avoids channel-index arithmetic in Phase 1; every component
 * (R, G, B) is treated identically, keeping the kernel minimal.
 *
 * Select via: --kernel scalar (default)
 */
__kernel void mad_kernel(
    __global const uchar* src,
    __global       uchar* dst,
    float contrast,
    int   brightness,
    int   total_bytes
) {
    // WHY size_t: get_global_id() returns size_t; using int truncates on large
    // images and causes signed/unsigned comparison warnings.
    size_t gid = get_global_id(0);
    // WHY bounds guard: NDRange may exceed the actual element count when rounded
    // up to a local work-group multiple — excess work-items must be discarded.
    if (gid >= (size_t)total_bytes) return;
    float val = (float)src[gid] * contrast + (float)brightness;
    dst[gid] = (uchar)clamp(val, 0.0f, 255.0f);
}

/*
 * mad_vec_kernel — Vectorized MAD filter (one work-item per RGB pixel).
 *
 * WHY vload3/vstore3: uchar3 is padded to 4 bytes in OpenCL; direct pointer
 * arithmetic would stride 4 bytes and corrupt packed RGB. vload3/vstore3
 * always stride exactly 3 bytes, matching the host-side packed buffer layout.
 *
 * WHY mad(): float3 fused multiply-add (OpenCL §6.12.3). One intrinsic covers
 * all three channels vs. three separate multiply+add ops in the scalar kernel.
 *
 * WHY convert_uchar3_sat: saturating conversion clamps to [0,255] without a
 * separate clamp() call — one instruction, no wrap-around artefacts.
 *
 * Select via: --kernel vec3
 */
__kernel void mad_vec_kernel(
    __global const uchar* src,
    __global       uchar* dst,
    float contrast,
    int   brightness,
    int   total_bytes
) {
    // WHY size_t: get_global_id() returns size_t; pixel index matches size_t
    // domain of vload3/vstore3 offset parameter.
    size_t gid = get_global_id(0);          // pixel index (not byte index)
    // WHY bounds guard: NDRange may exceed the actual pixel count when rounded
    // up to a local work-group multiple — excess work-items must be discarded.
    // total_bytes holds the pixel count when --kernel vec3 is selected
    // (caller passes work_size = width * height).
    if (gid >= (size_t)total_bytes) return;
    uchar3 pixel   = vload3(gid, src);
    float3 pixel_f = convert_float3(pixel);
    float3 result  = mad(pixel_f, (float3)(contrast), (float3)((float)brightness));
    vstore3(convert_uchar3_sat(result), gid, dst);
}
