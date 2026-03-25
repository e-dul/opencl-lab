// preprocess_nchw.cl — GPU-side image preprocessing for OpenVINO inference.
//
// Converts a full-resolution RGBA uchar image into an NCHW float32 tensor
// sized [1, 3, model_h, model_w] in a single kernel pass:
//   • Nearest-neighbour resize (src_w×src_h → model_w×model_h)
//   • RGBA → RGB channel strip (alpha discarded)
//   • Normalise to [0, 1] (divide by 255)
//   • NCHW layout: all R values then all G values then all B values
//
// WHY GPU preprocess instead of CPU: the RGBA image is already resident on the
// GPU (uploaded once for reuse by the bokeh blur kernel).  Preprocessing on GPU
// avoids a CPU round-trip and a second host→device upload of the normalised data.
//
// Global size: model_w * model_h (one work-item per output pixel).

__kernel void preprocess_nchw(
    __global const uchar4* src,      // full-resolution RGBA input
    __global       float*  dst,      // NCHW output: [3, model_h, model_w]
    int src_w,  int src_h,
    int model_w, int model_h)
{
    size_t gid = get_global_id(0);
    if (gid >= (size_t)model_w * model_h) return;

    int dx = (int)(gid % (size_t)model_w);
    int dy = (int)(gid / (size_t)model_w);

    // Nearest-neighbour: map output pixel to closest source pixel.
    int sx = dx * src_w / model_w;
    int sy = dy * src_h / model_h;

    uchar4 px = src[(size_t)sy * src_w + sx];

    // Write to three separate channel planes (NCHW).
    // WHY size_t plane: model_w * model_h can exceed INT_MAX for large models;
    // size_t prevents signed overflow in the offset calculation.
    size_t plane = (size_t)model_w * model_h;
    dst[0 * plane + gid] = px.x / 255.0f;  // R
    dst[1 * plane + gid] = px.y / 255.0f;  // G
    dst[2 * plane + gid] = px.z / 255.0f;  // B
}
