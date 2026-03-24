// divergence_kernel.cl — branching vs branchless conditional blur benchmark.
//
// Teaches: thread divergence on GPU.  When threads in a warp/wavefront
// follow different code paths (if-else), the hardware serialises them.
// The select() version computes both paths unconditionally and merges
// results with a bitselect — no divergence, all threads stay in lockstep.
//
// Kernel signature shared by both variants:
//   input  — source grayscale image (read-only, uchar)
//   mask   — per-pixel class (0 = blur region, 1 = copy region)
//   output — destination image
//   width, height — image dimensions

// ---------------------------------------------------------------------------
// blur1d_row — 1D horizontal box blur of radius 1 at pixel (gx, gy).
// Clamps at image boundaries so border pixels are handled correctly without
// branching in the caller.
// WHY inline macro: OpenCL 1.2 does not support proper inline functions in
// all implementations; a function-like macro avoids duplication while staying
// portable.
// ---------------------------------------------------------------------------
#define blur1d_row(input, gx, gy, width, height)                        \
    (( (int)(input[(gy) * (width) + clamp((int)(gx) - 1, 0, (width)-1)]) \
     + (int)(input[(gy) * (width) + (gx)])                               \
     + (int)(input[(gy) * (width) + clamp((int)(gx) + 1, 0, (width)-1)]) \
    ) / 3)

// ---------------------------------------------------------------------------
// blur_ifelse — branching variant.
// Each work-item reads its mask value, then takes ONE of two paths:
//   mask==0 → apply horizontal box blur (3 global reads)
//   mask!=0 → copy input unchanged (1 global read)
//
// WHY this causes divergence: threads in the same warp will have different
// mask values (50% probability at mask_density=0.5), so the warp splits.
// The GPU must execute both paths sequentially, masking off inactive lanes.
// ---------------------------------------------------------------------------
__kernel void blur_ifelse(__global const uchar* input,
                          __global const uchar* mask,
                          __global       uchar* output,
                          int width, int height)
{
    size_t gx = get_global_id(0);
    size_t gy = get_global_id(1);

    // Boundary guard for padding that rounds up to multiples of local size.
    if (gx >= (size_t)width || gy >= (size_t)height) return;

    size_t id = gy * (size_t)width + gx;

    if (mask[id] == 0) {
        // Blur path: 3 reads + divide — non-trivial cost.
        output[id] = (uchar)blur1d_row(input, gx, gy, width, height);
    } else {
        // Copy path: 1 read — cheap.
        output[id] = input[id];
    }
}

// ---------------------------------------------------------------------------
// blur_select — branchless variant using OpenCL built-in select().
// Both blur and copy values are computed unconditionally for every thread.
// select() picks the correct result without any branch instruction.
//
// select(false_val, true_val, condition):
//   condition == 0  → returns false_val (input, the "copy" result)
//   condition != 0  → returns true_val  (blurred)
//
// WHY this avoids divergence: all threads execute identical instructions.
// The warp never splits — full SIMD width is utilised at all times.
// ---------------------------------------------------------------------------
__kernel void blur_select(__global const uchar* input,
                          __global const uchar* mask,
                          __global       uchar* output,
                          int width, int height)
{
    size_t gx = get_global_id(0);
    size_t gy = get_global_id(1);

    if (gx >= (size_t)width || gy >= (size_t)height) return;

    size_t id = gy * (size_t)width + gx;

    // Both values computed regardless of mask — no divergence.
    uchar original = input[id];
    uchar blurred  = (uchar)blur1d_row(input, gx, gy, width, height);

    // select(a, b, c): returns b where c != 0, else a.
    // mask[id]==0 means "blur this pixel" → condition is (mask[id] == 0).
    output[id] = select(original, blurred, (uchar)(mask[id] == 0));
}
