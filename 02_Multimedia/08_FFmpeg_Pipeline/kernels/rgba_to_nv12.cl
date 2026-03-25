// rgba_to_nv12.cl — Convert RGBA image to NV12 VAAPI surface planes (zero-copy encode path)
//
// NV12 layout (as exported via cl_intel_va_api_media_sharing):
//   plane 0 (y_plane):  CL_R  CL_UNORM_INT8, W×H   — Y luma
//   plane 1 (uv_plane): CL_RG CL_UNORM_INT8, W/2×H/2 — Cb in .x, Cr in .y
//
// WHY BT.601 limited range: h264_vaapi encoder reads NV12 surfaces and expects
// Y in [16,235] and UV in [16,240], centred at 128 (studio swing convention).
// Using full-range coefficients would produce a washed-out encode.
//
// WHY two kernels: Y plane covers W×H pixels; UV plane covers W/2×H/2 due to
// 4:2:0 chroma subsampling. A single kernel dispatched over W×H would require
// conditional logic and two dispatch sizes; separate kernels are cleaner.

// ── rgba_to_nv12_y ──────────────────────────────────────────────────────────
// Dispatched over the full W×H luma resolution.
__kernel void rgba_to_nv12_y(
    __read_only  image2d_t  rgba_in,
    __write_only image2d_t  y_plane,
    int width,
    int height)
{
    size_t x = get_global_id(0);
    size_t y = get_global_id(1);
    if (x >= (size_t)width || y >= (size_t)height) return;

    const sampler_t samp = CLK_NORMALIZED_COORDS_FALSE
                         | CLK_ADDRESS_CLAMP_TO_EDGE
                         | CLK_FILTER_NEAREST;

    float4 rgba = read_imagef(rgba_in, samp, (int2)(x, y));
    float R = rgba.x, G = rgba.y, B = rgba.z;

    // BT.601 limited range: Y ∈ [16/255, 235/255]
    float Y = (16.0f + 65.481f * R + 128.553f * G + 24.966f * B) / 255.0f;

    write_imagef(y_plane, (int2)(x, y),
                 (float4)(clamp(Y, 0.0f, 1.0f), 0.0f, 0.0f, 1.0f));
}

// ── rgba_to_nv12_uv ─────────────────────────────────────────────────────────
// Dispatched over the W/2 × H/2 chroma resolution.
// width/height are the FULL frame dimensions; kernel derives UV dims.
__kernel void rgba_to_nv12_uv(
    __read_only  image2d_t  rgba_in,
    __write_only image2d_t  uv_plane,
    int width,
    int height)
{
    size_t x = get_global_id(0);
    size_t y = get_global_id(1);
    size_t uv_w = (size_t)((width  + 1) / 2);
    size_t uv_h = (size_t)((height + 1) / 2);
    if (x >= uv_w || y >= uv_h) return;

    const sampler_t samp = CLK_NORMALIZED_COORDS_FALSE
                         | CLK_ADDRESS_CLAMP_TO_EDGE
                         | CLK_FILTER_NEAREST;

    // WHY 2×2 average: 4:2:0 subsampling — one Cb/Cr pair represents a 2×2
    // luma block. Averaging the four RGBA pixels reduces chroma aliasing.
    float4 s00 = read_imagef(rgba_in, samp, (int2)(2*x,   2*y));
    float4 s10 = read_imagef(rgba_in, samp, (int2)(2*x+1, 2*y));
    float4 s01 = read_imagef(rgba_in, samp, (int2)(2*x,   2*y+1));
    float4 s11 = read_imagef(rgba_in, samp, (int2)(2*x+1, 2*y+1));
    float4 avg = (s00 + s10 + s01 + s11) * 0.25f;
    float R = avg.x, G = avg.y, B = avg.z;

    // BT.601 limited range: Cb/Cr ∈ [16/255, 240/255], centred at 128/255
    float Cb = (128.0f - 37.797f * R - 74.203f * G + 112.0f  * B) / 255.0f;
    float Cr = (128.0f + 112.0f  * R - 93.786f * G - 18.214f * B) / 255.0f;

    write_imagef(uv_plane, (int2)(x, y),
                 (float4)(clamp(Cb, 0.0f, 1.0f), clamp(Cr, 0.0f, 1.0f), 0.0f, 1.0f));
}
