// bokeh_blur.cl — Conditionally blur background pixels via a float segmentation mask.
//
// Input/output buffers are BGR, 3 bytes per pixel. Channel order is irrelevant —
// the blur is channel-agnostic (treats each channel identically).
//
// The mask is at model resolution (e.g. 256×256); bilinear interpolation maps
// each full-resolution pixel to the corresponding model-space coordinate.
//
// WHY a naive if/else branch: thread divergence is intentional per design §3.
// The educational goal is to show the performance cost of divergent branching
// vs. using select() — do NOT replace with select() without updating the design doc.
//
// Box filter: 5×5 neighbourhood average with boundary clamping.
// Mask values below 0.5 are treated as background and blurred.
//
// Signature per task spec: bokeh_blur(bgr_in, mask, width, height,
//                                      model_w, model_h, bgr_out)

__kernel void bokeh_blur(
    __global const uchar* bgr_in,   // full-resolution BGR input (3 bytes/pixel)
    __global const float* mask,      // model-resolution mask [model_h * model_w]
    int   width,
    int   height,
    int   model_w,
    int   model_h,
    __global uchar* bgr_out)         // full-resolution BGR output (3 bytes/pixel)
{
    // WHY 2D: dim0=x(col), dim1=y(row) — natural image indexing, no modulo/divide.
    int x = (int)get_global_id(0);
    int y = (int)get_global_id(1);
    if (x >= width || y >= height) return;

    size_t gid = (size_t)y * width + x;

    // Bilinear interpolation of mask coordinate.
    // WHY bilinear: nearest-neighbour would create blocky mask edges at full res;
    // bilinear gives smoother subject/background boundaries.
    float u = (x + 0.5f) * model_w / (float)width  - 0.5f;
    float v = (y + 0.5f) * model_h / (float)height - 0.5f;

    int u0 = clamp((int)floor(u), 0, model_w  - 1);
    int v0 = clamp((int)floor(v), 0, model_h  - 1);
    int u1 = clamp(u0 + 1,        0, model_w  - 1);
    int v1 = clamp(v0 + 1,        0, model_h  - 1);

    float fu = u - floor(u);
    float fv = v - floor(v);

    float m00 = mask[(size_t)v0 * model_w + u0];
    float m10 = mask[(size_t)v0 * model_w + u1];
    float m01 = mask[(size_t)v1 * model_w + u0];
    float m11 = mask[(size_t)v1 * model_w + u1];

    float mask_value = m00 * (1.0f - fu) * (1.0f - fv)
                     + m10 * fu          * (1.0f - fv)
                     + m01 * (1.0f - fu) * fv
                     + m11 * fu          * fv;

    // WHY 0.5f threshold: selfie_segmentation outputs probabilities; 0.5 is the
    // natural decision boundary between background (< 0.5) and foreground (>= 0.5).
    if (mask_value < 0.5f) {
        // Background: apply 5×5 box filter to blur.
        // WHY clamping (not wrapping): boundary wrapping would bleed opposite-edge
        // colours into the bokeh — incorrect for a depth-of-field effect.
        int3  acc = (int3)(0, 0, 0);
        int   cnt = 0;
        for (int dy = -2; dy <= 2; ++dy) {
            for (int dx = -2; dx <= 2; ++dx) {
                int nx = clamp(x + dx, 0, width  - 1);
                int ny = clamp(y + dy, 0, height - 1);
                size_t src = ((size_t)ny * width + nx) * 3;
                acc.x += bgr_in[src + 0];
                acc.y += bgr_in[src + 1];
                acc.z += bgr_in[src + 2];
                cnt++;
            }
        }
        size_t dst = gid * 3;
        bgr_out[dst + 0] = (uchar)(acc.x / cnt);
        bgr_out[dst + 1] = (uchar)(acc.y / cnt);
        bgr_out[dst + 2] = (uchar)(acc.z / cnt);
    } else {
        // Foreground: copy pixel unchanged (keep subject sharp).
        size_t src = gid * 3;
        bgr_out[src + 0] = bgr_in[src + 0];
        bgr_out[src + 1] = bgr_in[src + 1];
        bgr_out[src + 2] = bgr_in[src + 2];
    }
}
