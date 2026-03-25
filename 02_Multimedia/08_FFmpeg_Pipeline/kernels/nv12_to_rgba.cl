// nv12_to_rgba.cl — Convert NV12 VAAPI surface planes to RGBA image (zero-copy path)
//
// NV12 layout (as imported via cl_intel_va_api_media_sharing):
//   plane 0 (y_plane):  CL_R  CL_UNORM_INT8, W×H   — Y luma values
//   plane 1 (uv_plane): CL_RG CL_UNORM_INT8, W/2×H/2 — U in .x, V in .y
//
// WHY BT.601 limited range: H.264/H.265 SD/HD streams default to BT.601
// with Y in [16,235] and UV in [16,240], centred at 128.

__kernel void nv12_to_rgba(
    __read_only  image2d_t  y_plane,
    __read_only  image2d_t  uv_plane,
    __write_only image2d_t  rgba_out,
    int width,
    int height)
{
    size_t x = get_global_id(0);
    size_t y = get_global_id(1);
    if (x >= (size_t)width || y >= (size_t)height) return;

    const sampler_t samp = CLK_NORMALIZED_COORDS_FALSE
                         | CLK_ADDRESS_CLAMP_TO_EDGE
                         | CLK_FILTER_NEAREST;

    // Y: normalized [16/255 .. 235/255] for BT.601 limited range
    float Y_raw = read_imagef(y_plane,  samp, (int2)(x, y)).x;
    // UV: normalized [16/255 .. 240/255], centre 128/255
    float2 uv   = read_imagef(uv_plane, samp, (int2)(x / 2, y / 2)).xy;

    // BT.601 limited range → full-range RGB
    float Y = (Y_raw       - 16.0f / 255.0f) * (255.0f / 219.0f);
    float U = (uv.x - 128.0f / 255.0f) * (255.0f / 112.0f);
    float V = (uv.y - 128.0f / 255.0f) * (255.0f / 112.0f);

    float R = Y              + 1.5958f * V;
    float G = Y - 0.3917f * U - 0.8129f * V;
    float B = Y + 2.0170f * U;

    write_imagef(rgba_out, (int2)(x, y),
                 (float4)(clamp(R, 0.f, 1.f),
                          clamp(G, 0.f, 1.f),
                          clamp(B, 0.f, 1.f),
                          1.0f));
}
