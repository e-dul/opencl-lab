// sub_buffer_demo.cl — fill a sub-buffer region with a uniform color.
//
// WHY sub-buffer vs. parent buffer:
//   The host passes a cl::Buffer created via createSubBuffer(), which is an
//   aliased view into a contiguous slice of the parent buffer.  The runtime
//   adjusts the base pointer so that get_global_id(0) == 0 always maps to the
//   FIRST element of THIS sub-buffer region, NOT to element 0 of the parent.
//   This means the kernel can use a simple 0-based index without knowing where
//   in the parent buffer the sub-buffer starts — the aliasing is handled below
//   the kernel ABI boundary by the OpenCL driver.

__kernel void fill_strip(
    __global uchar4* pixels,  // sub-buffer alias — element 0 = sub-buffer origin
    uchar4            fill_color,
    int               count)
{
    // WHY size_t: get_global_id() returns size_t; using int would silently
    // truncate on very large buffers (>2 GB).
    size_t gid = get_global_id(0);

    // Guard against over-dispatch (global work size rounded up to local size).
    if (gid < (size_t)count) {
        pixels[gid] = fill_color;
    }
}
