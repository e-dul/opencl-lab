// mask_resize.cl — Nearest-neighbour upscale of a float segmentation mask.
//
// WHY on GPU: the mask must flow from OpenVINO output tensor to bokeh_blur
// without a host round-trip (task DoD: no enqueueReadBuffer/WriteBuffer on mask
// data between inference and blur kernel dispatch).  A dedicated resize kernel
// keeps the mask entirely on the GPU.
//
// Each work-item writes one output pixel; the source coordinate is derived via
// nearest-neighbour mapping: src_x = dst_x * src_w / dst_w (integer division).

__kernel void mask_resize(
    __global const float* src,   // model output mask: src_w × src_h floats
    __global       float* dst,   // full-resolution mask: dst_w × dst_h floats
    int src_w, int src_h,
    int dst_w, int dst_h)
{
    // WHY size_t: get_global_id() returns size_t; using int risks truncation
    // and signed/unsigned comparison warnings.
    size_t gid = get_global_id(0);
    if (gid >= (size_t)dst_w * dst_h) return;

    int dx = (int)(gid % (size_t)dst_w);
    int dy = (int)(gid / (size_t)dst_w);

    // Nearest-neighbour: map destination pixel to closest source pixel.
    // WHY integer division: avoids float rounding differences that could shift
    // the upscaled mask by a sub-pixel — irrelevant for segmentation masks.
    int sx = dx * src_w / dst_w;
    int sy = dy * src_h / dst_h;

    dst[gid] = src[(size_t)sy * src_w + sx];
}
