// zero_copy_kernel.cl — passthrough kernel for buffer strategy benchmarking.
//
// WHY this kernel exists: we need a representative GPU workload that touches
// every byte so that transfer cost is truly exercised for each buffer strategy.
// A no-op kernel would only measure launch overhead, not memory bandwidth.

__kernel void passthrough(__global const uchar* in,
                          __global       uchar* out,
                          int                   size)
{
    size_t gid = get_global_id(0);
    // Guard against over-dispatch when global size is rounded up to a multiple
    // of the work-group size.
    if (gid < (size_t)size) {
        out[gid] = in[gid];
    }
}
