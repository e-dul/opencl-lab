# Fast Math

**Symptom**: Kernel calls `sqrt`, `rsqrt`, `sin`, or `cos` in a tight loop (ray traversal, physics, signal processing). Kernel is compute-bound and standard math functions are visible in the profiler hotspot.

## Prerequisites
Prerequisites: OpenCL 1.2+, CMake 3.18+, `clinfo` installed. See [main README](../../README.md) for base requirements.

## Build & Run
```bash
cd 05_Toolbox/05_Fast_Math
cmake -B build && cmake --build build
./build/fast_math --rays 1000000
./build/fast_math --rays 1000000 --relaxed   # also runs -cl-fast-relaxed-math variants
# GPU=NVIDIA ./build/fast_math --rays 10000000
```

## Verify
```
[Standard  ] 1M ray normalizations:  18.4 ms   (IEEE 754 sqrt/rsqrt)
[half_      ] 1M ray normalizations:   9.1 ms   (2.0x faster, ≥10-bit)
[native_    ] 1M ray normalizations:   2.3 ms   (8.0x faster, hw-defined)
[-cl-fast-relaxed-math + native_] 1.9 ms   (8.7x faster)
```

Run with `--rays 10000000` on a discrete GPU to get stable numbers — small counts are dominated by launch overhead.

> **Note:** `native_` gain may be negligible or absent on CPU/PoCL devices — hardware-defined precision means some runtimes map `native_rsqrt` directly to the standard path.

## Concept

OpenCL exposes three precision tiers for transcendental math functions:

| Prefix | ULP error guarantee | Typical speed | Use when |
|:-------|:--------------------|:-------------|:---------|
| (none) `sqrt(x)` | ≤ 3 ULP (IEEE 754) | 1× | Correctness required: physics, finance, medical |
| `half_sqrt(x)` | ≥ 10-bit mantissa (≤ 8192 ULP, §6.12.2) | ~2× | Visual output, signal processing |
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

**When precision trade-offs are acceptable:** see the "Use when" and "Example domains" columns in the table above.

**`native_` vs `-cl-fast-relaxed-math`**: prefer explicit `native_` calls over the build flag. The build flag silently affects every function in the kernel; explicit `native_` calls document exactly where you are trading precision for speed.

## Mini-Challenge

In the BVH traversal kernel from [02_Ray_Tracer_BVH](../../03_GraphicsHPC/02_Ray_Tracer_BVH/RayTracerBVH.md), replace `rsqrt` with `native_rsqrt` in the ray normalisation step. Measure the kernel time before and after. Then enable `-cl-fast-relaxed-math` and compare the output images pixel-by-pixel — quantify the maximum pixel error introduced.

**Second challenge:** Add a `native_sin` variant to the fast_math kernel to simulate an audio oscillator or physics spring (`y = A * native_sin(t * freq)`). Compare `native_sin` vs standard `sin` throughput on 1M samples. On Nvidia hardware, expect 4–8× speedup; on CPU/PoCL runtimes the gain is typically < 1.5×.

## Troubleshooting

- **`native_sqrt` returns NaN for negative inputs**: `sqrt(x < 0)` returns NaN per the OpenCL spec — standard `sqrt` does not clamp; `native_sqrt` has no guarantee at all. Guard with `max(0.0f, x)` before calling either variant.
- **Results differ between devices**: `native_` precision is hardware-defined — AMD, Nvidia, and Intel produce different results. If your algorithm depends on bit-exact reproducibility, do not use `native_`.
- **`-cl-fast-relaxed-math` breaks convergence in iterative solver**: The flag allows FP reordering which changes accumulation order. Use explicit `native_` calls only on the specific functions that need speed instead.

## Used In
- [Track B — 02_Ray_Tracer_BVH](../../03_GraphicsHPC/02_Ray_Tracer_BVH/RayTracerBVH.md) (ray normalisation, AABB slab test)

---

[Back to Toolbox](../Toolbox.md)
