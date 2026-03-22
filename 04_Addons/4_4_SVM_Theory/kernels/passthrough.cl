// passthrough.cl — identity copy kernel for benchmarking memory-transfer paths.
// WHY uchar4: RGBA pixels are 4 bytes; packing into uchar4 matches the
// 4-byte alignment OpenCL prefers and avoids unnecessary byte scatter/gather.
__kernel void passthrough(__global const uchar4* in, __global uchar4* out, int size) {
    size_t gid = get_global_id(0);
    if (gid < (size_t)size) out[gid] = in[gid];
}
