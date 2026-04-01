// mad_kernel.cl — single source compiled at runtime for multiple types.
//
// WHY -D TYPE=uchar/float/half at build time (not runtime branching):
//   Compile-time specialisation lets the GPU backend optimise vector loads
//   and arithmetic per type without any runtime overhead. A single kernel
//   with runtime branches would compile to a union of all three code paths.
//
// The caller injects the type via build options, e.g.:
//   program.build({device}, "-D TYPE=float")
//
// For the half variant the caller additionally prepends:
//   "-D TYPE=half -DOPENCL_EXTENSION_ENABLE"
// ... and the extension pragma is injected via build options string prefix
// (see host code).  The source remains type-agnostic — no #pragma in source.

#ifndef TYPE
  #define TYPE uchar   // default: uchar (safe to compile without -D)
#endif

typedef TYPE scalar_t;

__kernel void mad_kernel(__global scalar_t* restrict out,
                         __global const scalar_t* restrict in,
                         float contrast,
                         float brightness,
                         uint  size) {
    // WHY size_t: get_global_id() returns size_t; using int would produce
    // signed/unsigned comparison warnings and is wrong on >2G-element buffers.
    size_t gid = get_global_id(0);

    // Guard against over-dispatch when global_work_size is rounded up to a
    // work-group multiple but the buffer is not a multiple of that size.
    // WHY uint size: avoids the negative-int pitfall where (size_t)(-1) would
    // be UINT64_MAX, causing the guard to never fire.
    if (gid >= (size_t)size) return;

    // WHY clamp: contrast/brightness can produce values outside [0, TYPE_MAX].
    // For uchar, out-of-range float→uchar conversion is implementation-defined.
    // clamp to [0, 255] before the cast prevents implementation-defined overflow.
    float result = (float)in[gid] * contrast + brightness;
    out[gid] = (scalar_t)clamp(result, 0.0f, 255.0f);
}
