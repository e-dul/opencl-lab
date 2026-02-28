# Thread Divergence

**Symptom**: Kernel contains `if-else` branching on data-dependent conditions. Kernel time is 2–4x slower than a branchless equivalent would predict.

## Prerequisites
See [main README](../../README.md) for base requirements (OpenCL 1.2+, CMake 3.18+).

## Build & Run
```bash
cd 99_Toolbox/ThreadDivergence
cmake -B build && cmake --build build
./build/divergence_demo --width 1920 --height 1080
```

## Verify
```
[IF-ELSE  ] Conditional blur:  9.8 ms
[SELECT() ] Branchless blur:   4.1 ms   2.4x faster
```

Run the demo first. Confirm the 2x gap on your hardware before reading the explanation.

## Concept

GPU threads execute in lock-step groups (warps/wavefronts). When threads in the same warp take different branches, the hardware serializes both paths — threads that don't take the active branch are masked off but still consume cycles.

```cl
// Divergent: threads in the same warp take different paths
if (mask[id] == BACKGROUND) {
    output[id] = blur(input, id, width);   // background threads run this
} else {
    output[id] = input[id];                // foreground threads run this
}
// Both paths execute for every warp that contains mixed pixels — 2x cost

// Branchless: all threads execute the same instruction
float blurred = blur(input, id, width);
output[id] = select(input[id], blurred, mask[id] == BACKGROUND);
// select() is a single instruction — no divergence
```

`select(false_val, true_val, condition)` maps to a hardware conditional-select instruction. No branch, no divergence, no serialization.

**When divergence is unavoidable**: restructure the workload so divergent work items are in different work-groups (sort by mask value before dispatch). This is the basis of stream compaction.

## Mini-Challenge

Modify the demo to use a checkerboard mask (alternating pixels, maximum divergence within each warp). Measure the time vs a solid mask (no divergence). What is the worst-case divergence penalty on your hardware?

## Troubleshooting

- **`select()` shows no speedup**: Some compilers optimize `if-else` to `select` automatically when the branch bodies are side-effect-free. Inspect the generated ISA with `clGetProgramInfo(CL_PROGRAM_BINARIES)` to confirm.
- **Divergence penalty varies by device**: AMD RDNA uses 32-wide waves; Nvidia uses 32-wide warps; Intel Arc uses 16-wide SIMD. The penalty scales with wave width.

## Used In
- [Track A — A4_Smart_Webcam](../../02_Projects/A_Multimedia/Multimedia.md#a4_smart_webcam--flagship-project) (Bokeh mask conditional)
- [Track B — B3_Ray_Tracer_BVH](../../02_Projects/B_Graphics_HPC/GraphicsHPC.md#b3_ray_tracer_bvh--flagship-project) (BVH traversal branches)

---

[Back to Toolbox](../Toolbox.md)
