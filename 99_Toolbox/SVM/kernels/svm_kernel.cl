// scale_add — applies data[i] = data[i] * scale + offset for each element.
//
// WHY a single kernel for all modes: buffer+map, coarse SVM, and fine-grained SVM
// all expose the same __global float* interface to the device.  The host-side
// allocation strategy (cl::Buffer vs. SVM pointer) is entirely transparent to
// the kernel.
//
// WHY ulong n instead of size_t: the host passes cl_ulong (always 8 bytes).
// Using ulong here ensures the type widths match on both 32-bit and 64-bit
// OpenCL devices — size_t would be 32 bits on a 32-bit device, causing a
// parameter layout mismatch with cl_ulong.
__kernel void scale_add(__global float* data, ulong n, float scale, float offset) {
    size_t gid = get_global_id(0);
    // Guard: NDRange may be rounded up; skip out-of-bounds work-items.
    if (gid >= (size_t)n) return;
    data[gid] = data[gid] * scale + offset;
}
