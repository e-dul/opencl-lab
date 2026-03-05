// pipeline_kernel.cl — compute-heavy FMA loop for pipeline overlap demonstration.
//
// WHY iters parameter: allows the caller to calibrate compute time independently
// of buffer size. Target ≥5 ms/frame so transfer overlap (~1-2 ms) is observable.
// WHY size_t gid: get_global_id() returns size_t; int would cause signed/unsigned
// comparison warnings and would wrap on >2^31 element buffers.
__kernel void process(__global const float* input,
                      __global       float* output,
                      int                   size,
                      int                   iters,
                      float                 scale)
{
    size_t gid = get_global_id(0);
    if (gid < (size_t)size) {
        float acc = input[gid] * scale;
        // Iterative FMA: each iteration is a dependent multiply-add so the
        // compiler cannot vectorise it away. This creates measurable compute load.
        for (int i = 0; i < iters; ++i) {
            acc = acc * scale + 1.0f;
        }
        output[gid] = acc;
    }
}
