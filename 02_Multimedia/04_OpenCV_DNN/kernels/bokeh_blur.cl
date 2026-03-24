// bokeh_blur.cl — Conditionally blur background pixels via a float segmentation mask.
//
// WHY a naive if/else branch: thread divergence is intentional per design §3.
// The educational goal is to show the performance cost of divergent branching
// vs. using select() — do NOT replace with select() without updating the design doc.
//
// Box filter: 5×5 neighbourhood average with boundary clamping.
// The mask value [0.0, 1.0] comes from the DNN output; values below threshold
// are treated as background and blurred.

__kernel void bokeh_blur(
    __global const uchar4* input,
    __global       uchar4* output,
    __global const float*  mask,
    int   width,
    int   height,
    float threshold)
{
    // WHY size_t: get_global_id() returns size_t; using int causes implicit
    // truncation and signed/unsigned comparison warnings.
    size_t gid = get_global_id(0);
    if (gid >= (size_t)width * height) return;

    int x = (int)(gid % (size_t)width);
    int y = (int)(gid / (size_t)width);

    if (mask[gid] < threshold) {
        // Background: apply 5×5 box filter to blur.
        // WHY clamping (not wrapping): boundary wrapping would bleed opposite-edge
        // colours into the bokeh — incorrect for a depth-of-field effect.
        int4  acc  = (int4)(0, 0, 0, 0);
        int   cnt  = 0;
        for (int dy = -2; dy <= 2; ++dy) {
            for (int dx = -2; dx <= 2; ++dx) {
                int nx = clamp(x + dx, 0, width  - 1);
                int ny = clamp(y + dy, 0, height - 1);
                uchar4 s = input[(size_t)ny * (size_t)width + (size_t)nx];
                acc.x += s.x;
                acc.y += s.y;
                acc.z += s.z;
                acc.w += s.w;
                cnt++;
            }
        }
        uchar4 blurred;
        blurred.x = (uchar)(acc.x / cnt);
        blurred.y = (uchar)(acc.y / cnt);
        blurred.z = (uchar)(acc.z / cnt);
        blurred.w = (uchar)(acc.w / cnt);
        output[gid] = blurred;
    } else {
        // Foreground: copy pixel unchanged (keep subject sharp).
        output[gid] = input[gid];
    }
}
