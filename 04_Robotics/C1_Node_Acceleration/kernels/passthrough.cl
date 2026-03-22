// Passthrough kernel: copies src to dst element-wise.
// WHY not a no-op: drivers are permitted to elide zero-work dispatches;
// a real memory copy guarantees measurable dispatch time in profiling.
__kernel void passthrough(__global const float* src, __global float* dst, int n) {
    size_t gid = get_global_id(0);
    if (gid < (size_t)n) dst[gid] = src[gid];
}
