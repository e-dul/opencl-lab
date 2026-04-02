// Invert kernel applied to a horizontal slice of an RGBA image.
// WHY separate kernel file: loaded at runtime so it can be inspected and modified
//   without recompiling host code (educational transparency).
//

// WHY ITERATIONS=1001 (odd):
//   The simple single-pass invert takes < 1 ms on a 4K image and is dominated
//   by PCIe transfers, making multi-GPU kernel speedup invisible in the table.
//   Repeating the invert ITERATIONS times burns ALU cycles so kernel_ms
//   dominates transfer time and the N-GPU speedup becomes measurable.
//   WHY odd: each invert toggles the pixel; an odd count leaves a net single
//   invert, so applying the kernel twice still returns the original (the
//   correctness check's invert-twice round-trip remains valid).
//   WHY GPU JIT won't fold this loop: OpenCL JIT compilers lack the
//   global analysis to prove that N integer toggles collapse to one; each
//   iteration executes on the shader cores.
#define ITERATIONS 1001

__kernel void process_slice(
    __global const uchar4* in,
    __global       uchar4* out,
    int width,
    int height)
{
    size_t gid = get_global_id(0);
    size_t slice_pixels = (size_t)width * (size_t)height;

    // Guard against over-dispatch on workgroup-aligned boundaries.
    if (gid >= slice_pixels) return;

    uchar4 val = in[gid];
    for (int k = 0; k < ITERATIONS; k++) {
        val = (uchar4)(255 - val.x, 255 - val.y, 255 - val.z, 255 - val.w);
    }
    out[gid] = val;
}
