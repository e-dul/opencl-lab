## Why

`03_Ray_Tracer_BVH_Dynamic` demonstrated that CPU SAH-BVH refit cuts per-frame build time to ~0.9 ms — but the CPU is still involved every frame, creating a synchronisation stall and wasting GPU cycles during build. `04_Ray_Tracer_LBVH` removes the CPU from the BVH pipeline entirely, enabling GPU-autonomous per-frame rebuilds suitable for fully dynamic scenes.

## What Changes

- New submodule `03_GraphicsHPC/04_Ray_Tracer_LBVH` implementing a complete GPU LBVH pipeline.
- Six OpenCL kernels replace the CPU build path: scene-bounds reduction, Morton code generation, 4-pass radix sort, Karras 2012 tree construction, parallel AABB fitting, and stack-based ray traversal.
- New `RayTracerLBVH.md` submodule README consistent with `02_Ray_Tracer_BVH`/`03_Ray_Tracer_BVH_Dynamic` format.
- `GraphicsHPC.md` module index updated with `04_Ray_Tracer_LBVH` row and performance gate.

## Capabilities

### New Capabilities

- `gpu-lbvh-build`: Full GPU BVH construction pipeline — scene AABB reduction → Morton codes → radix sort → Karras 2012 parallel tree build → atomic parallel AABB fitting. No CPU round-trip.
- `radix-sort-opencl`: 4-pass parallel radix sort (8 bits/pass) implemented in OpenCL 1.2: per-pass histogram, prefix scan (Blelloch), and scatter kernels.
- `lbvh-traversal`: Stack-based GPU ray traversal over a Karras binary radix tree using per-work-item local memory stack (32 entries).

### Modified Capabilities

*(none — no existing spec requirements change)*

## Impact

- **New directory**: `03_GraphicsHPC/04_Ray_Tracer_LBVH/` with `main.cpp`, `lbvh_builder.hpp`, `CMakeLists.txt`, `kernels/`, `RayTracerLBVH.md`.
- **Updated**: `03_GraphicsHPC/GraphicsHPC.md` — add `04_Ray_Tracer_LBVH` row to Contents and Performance Gates tables.
- **Dependencies**: tinyobjloader (already used in `02_Ray_Tracer_BVH`/`03_Ray_Tracer_BVH_Dynamic`, FetchContent), CLI11 (already in common.cmake), OpenCL 1.2.
- **No breaking changes** to existing modules.
