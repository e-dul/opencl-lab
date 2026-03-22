# Fast Math

**Symptom**: Kernel calls `sqrt`, `rsqrt`, `sin`, or `cos` in a tight loop (ray traversal, physics, signal processing). Kernel is compute-bound and standard math functions are visible in the profiler hotspot.

## Prerequisites
Prerequisites: OpenCL 1.2+, CMake 3.18+, `clinfo` installed. See [main README](../../README.md) for base requirements.

## Build & Run
```bash
cd 99_Toolbox/FastMath
cmake -B build && cmake --build build
./build/fast_math --rays 1000000
# GPU=NVIDIA ./build/fast_math --rays 10000000
```

## Verify
```
[Standard  ] 1M ray normalizations:  18.4 ms   (IEEE 754 sqrt/rsqrt)
[half_      ] 1M ray normalizations:   9.1 ms   (2.0x faster, ≥11-bit)
[native_    ] 1M ray normalizations:   2.3 ms   (8.0x faster, hw-defined)
[-cl-fast-relaxed-math + native_] 1.9 ms   (8.7x faster)
```

Run with `--rays 10000000` on a discrete GPU to get stable numbers — small counts are dominated by launch overhead.

## Concept

OpenCL exposes three precision tiers for transcendental math functions:

| Prefix | ULP error guarantee | Typical speed | Use when |
|:-------|:--------------------|:-------------|:---------|
| (none) `sqrt(x)` | ≤ 3 ULP (IEEE 754) | 1× | Correctness required: physics, finance, medical |
| `half_sqrt(x)` | ≥ 11-bit mantissa | ~2× | Visual output, signal processing (if `cl_khr_fp16` supported) |
| `native_sqrt(x)` | Hardware-defined, no guarantee | 4–10× | Ray tracing, particle systems, games |

**`-cl-fast-relaxed-math`**: a `clBuildProgram` flag that enables the compiler to:
- Assume inputs are finite and non-NaN (skips IEEE edge-case handling)
- Reorder floating-point operations (may change results by small epsilon)
- Replace standard functions with `native_` equivalents automatically
- Enable fused multiply-add (FMA) where available

```cpp
// Host: enable fast math at kernel compile time
program.build("-cl-fast-relaxed-math");
```

```cl
// Kernel: explicit native functions (readable, no surprise behaviour)
float3 normalize_ray(float3 dir) {
    float inv_len = native_rsqrt(dir.x*dir.x + dir.y*dir.y + dir.z*dir.z);
    return dir * inv_len;
}

// Ray-AABB slab test: native_recip replaces 3 divisions
float3 inv_dir = (float3)(native_recip(dir.x),
                          native_recip(dir.y),
                          native_recip(dir.z));
```

**When precision trade-offs are acceptable:**

| Domain | Standard math | Fast math |
|:-------|:-------------|:----------|
| Ray tracing (visual) | ✗ unnecessary | ✓ — sub-pixel error invisible |
| BVH traversal (hit/miss) | ✗ unnecessary | ✓ — false hit/miss rate negligible |
| Particle systems | ✗ unnecessary | ✓ |
| Physics simulation | ✓ | depends on step size |
| Financial / medical | ✓ mandatory | ✗ never |

**`native_` vs `-cl-fast-relaxed-math`**: prefer explicit `native_` calls over the build flag. The build flag silently affects every function in the kernel; explicit `native_` calls document exactly where you are trading precision for speed.

## Mini-Challenge

In the BVH traversal kernel from [B3_Ray_Tracer_BVH](../../02_Projects/B_Graphics_HPC/GraphicsHPC.md#b3_ray_tracer_bvh--flagship-project), replace `rsqrt` with `native_rsqrt` in the ray normalisation step. Measure the kernel time before and after. Then enable `-cl-fast-relaxed-math` and compare the output images pixel-by-pixel — quantify the maximum pixel error introduced.

## Troubleshooting

- **`native_sqrt` returns NaN for negative inputs**: Standard `sqrt` clamps negative inputs; `native_sqrt` does not. Guard with `max(0.0f, x)` before calling.
- **Results differ between devices**: `native_` precision is hardware-defined — AMD, Nvidia, and Intel produce different results. If your algorithm depends on bit-exact reproducibility, do not use `native_`.
- **`-cl-fast-relaxed-math` breaks convergence in iterative solver**: The flag allows FP reordering which changes accumulation order. Use explicit `native_` calls only on the specific functions that need speed instead.

## Used In
- [Track B — B3_Ray_Tracer_BVH](../../02_Projects/B_Graphics_HPC/GraphicsHPC.md#b3_ray_tracer_bvh--flagship-project) (ray normalisation, AABB slab test)

---

[Back to Toolbox](../Toolbox.md)
