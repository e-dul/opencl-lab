// ray_kernel.cl — three normalization variants compiled separately via -D VARIANT=
//
// WHY three separate entry points rather than one with runtime branching:
//   Using compile-time -D flags allows the compiler to emit fully optimized
//   ISA paths per variant. A runtime branch would prevent the compiler from
//   selecting the optimal instruction sequence (e.g., native_rsqrt maps to a
//   single HW instruction; branching around it defeats that purpose).

// ──────────────────────────────────────────────────────────────────────────────
// Standard variant — IEEE-754 compliant sqrt / rsqrt
// ──────────────────────────────────────────────────────────────────────────────
__kernel void ray_normalize_standard(__global const float4* restrict rays_in,
                                     __global       float4* restrict rays_out,
                                     int size)
{
    size_t gid = get_global_id(0);
    if (gid >= (size_t)size) return;

    float4 r   = rays_in[gid];
    // dot(r, r) over xyz only; w is unused sentinel.
    float  len2 = r.x * r.x + r.y * r.y + r.z * r.z;
    // WHY rsqrt: single hardware instruction vs two-op sequence (sqrt + divide);
    //   lower latency, same IEEE-754 precision as 1.0f / sqrt(len2).
    float  inv  = rsqrt(len2);
    rays_out[gid] = (float4)(r.x * inv, r.y * inv, r.z * inv, 0.0f);
}

// ──────────────────────────────────────────────────────────────────────────────
// Half variant — ~11-bit mantissa precision (cl_khr_fp16 required)
// WHY guard: half_ builtins are only valid when the extension is present;
//   missing it causes a build error, not a runtime error.
// ──────────────────────────────────────────────────────────────────────────────
#ifdef USE_HALF
__kernel void ray_normalize_half(__global const float4* restrict rays_in,
                                 __global       float4* restrict rays_out,
                                 int size)
{
    size_t gid = get_global_id(0);
    if (gid >= (size_t)size) return;

    float4 r    = rays_in[gid];
    float  len2 = r.x * r.x + r.y * r.y + r.z * r.z;
    // half_rsqrt: faster than rsqrt, less accurate (~11-bit mantissa).
    float  inv  = half_rsqrt(len2);
    rays_out[gid] = (float4)(r.x * inv, r.y * inv, r.z * inv, 0.0f);
}
#endif  // USE_HALF

// ──────────────────────────────────────────────────────────────────────────────
// Native variant — maximum throughput, implementation-defined precision
// WHY native_rsqrt: maps directly to a single hardware instruction on AMD/NVIDIA.
//   Bit-exact results are NOT guaranteed to match other vendors — use only when
//   the caller can tolerate small errors (e.g., shading, not geometry tests).
// ──────────────────────────────────────────────────────────────────────────────
__kernel void ray_normalize_native(__global const float4* restrict rays_in,
                                   __global       float4* restrict rays_out,
                                   int size)
{
    size_t gid = get_global_id(0);
    if (gid >= (size_t)size) return;

    float4 r    = rays_in[gid];
    float  len2 = r.x * r.x + r.y * r.y + r.z * r.z;
    // native_rsqrt: single-instruction path; precision varies by GPU vendor.
    float  inv  = native_rsqrt(len2);
    rays_out[gid] = (float4)(r.x * inv, r.y * inv, r.z * inv, 0.0f);
}
