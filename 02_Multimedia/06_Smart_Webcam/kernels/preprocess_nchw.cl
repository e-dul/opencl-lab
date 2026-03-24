// preprocess_nchw.cl — GPU-side image preprocessing for OpenVINO inference.
//
// Converts a full-resolution BGR uchar image (3 bytes/pixel, webcam-native format)
// into an NCHW float32 tensor sized [1, 3, model_h, model_w] in a single pass:
//   • Nearest-neighbour resize (src_w×src_h → model_w×model_h)
//   • BGR → RGB channel swap (model expects RGB; B↔R swap done here, not on host)
//   • Normalise to [0, 1] (divide by 255)
//   • NCHW layout: all R values then all G values then all B values
//
// WHY GPU preprocess instead of CPU: the BGR image is already resident on the GPU.
// Preprocessing here avoids a CPU round-trip and second host→device upload.
//
// WHY BGR input: cv::VideoCapture delivers BGR natively; converting to RGB on host
// before upload would waste a full-frame pass. We swap channels here for free.
//
// Global size: {model_w, model_h} — 2D NDRange, one work-item per output pixel.
// WHY 2D: dim0=x(col), dim1=y(row) maps naturally to image coordinates without
// manual flat-index decomposition; avoids the modulo/divide per work-item.

__kernel void preprocess_nchw(
    __global const uchar* bgr_in,   // full-resolution BGR input (3 bytes/pixel)
    int in_w,  int in_h,
    __global float* nchw_out,       // NCHW output: [3, model_h, model_w]
    int model_w, int model_h)
{
    int ox = (int)get_global_id(0);  // output column [0, model_w)
    int oy = (int)get_global_id(1);  // output row    [0, model_h)
    if (ox >= model_w || oy >= model_h) return;

    // Nearest-neighbour: map output pixel to closest source pixel.
    int sx = ox * in_w / model_w;
    int sy = oy * in_h / model_h;

    // BGR stride is 3 bytes per pixel.
    size_t src_idx = ((size_t)sy * in_w + sx) * 3;
    uchar b = bgr_in[src_idx + 0];
    uchar g = bgr_in[src_idx + 1];
    uchar r = bgr_in[src_idx + 2];

    // Write to three separate channel planes (NCHW) as RGB.
    // WHY R→plane0, G→plane1, B→plane2: selfie_segmentation.onnx was trained on
    // RGB-ordered NCHW tensors. The swap is free here vs. an extra host pass.
    // WHY size_t plane: model_w * model_h can exceed INT_MAX for large models.
    size_t gid   = (size_t)oy * model_w + ox;
    size_t plane = (size_t)model_w * model_h;
    nchw_out[0 * plane + gid] = r / 255.0f;  // R → channel 0
    nchw_out[1 * plane + gid] = g / 255.0f;  // G → channel 1
    nchw_out[2 * plane + gid] = b / 255.0f;  // B → channel 2
}
