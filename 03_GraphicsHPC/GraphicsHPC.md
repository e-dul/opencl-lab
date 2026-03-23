# Path B: Graphics & HPC

Build a ray tracer from scratch and make it fast enough to render complex triangle scenes at 60 FPS. You start by putting the framebuffer on the GPU and displaying it without a copy, discover where naive ray tracing breaks down at scale, then implement the spatial acceleration structure that fixes it.

## Prerequisites

- Base requirements: see [main README](../README.md) (OpenCL, CMake 3.18+).
- OpenGL + GLFW (live window in B2/B3): `sudo apt install libglfw3-dev libgl-dev` — optional; headless `--output render.bmp` works without it.
- tinyobjloader (B3/B3 Dynamic): fetched automatically by CMake at configure time. Offline: `-DCMAKE_PREFIX_PATH=/path/to/install`.
- Assets: `assets/bunny.obj` (Stanford Bunny, ~70k triangles), `assets/cornell_box.obj`.
- CLBlast and Device Enqueue have moved to [06_Bonus/](../06_Bonus/Bonus.md).

## Contents

| Sub-module | Goal | Doc |
| :--------- | :--- | :-- |
| B2 — Ray Tracer Basic | Minimal ray tracer + OpenGL interop (no CPU copies) | [RayTracerBasic.md](B2_Ray_Tracer_Basic/RayTracerBasic.md) |
| B3 — Ray Tracer BVH | Flagship: stackless BVH for 100k-triangle scenes at 60 FPS | [RayTracerBVH.md](B3_Ray_Tracer_BVH/RayTracerBVH.md) |
| B3 Dynamic | BVH rebuild vs refit vs static on a moving scene | [RayTracerBVHDynamic.md](B3_Ray_Tracer_BVH_Dynamic/RayTracerBVHDynamic.md) |

## Performance Gates

| Sub-module | Metric | Target |
| :--------- | :----- | :----- |
| B3 Ray Tracer BVH | Render time (`cl::Event`) | ≥ 60 FPS @ bunny.obj (~70k triangles), 1920×1080 |
| B3 Dynamic — refit | BVH build time | Measurably less than rebuild (~1 ms vs ~16 ms) |
| B3 Dynamic | `--max-depth 1` render time | Measurably higher than `--max-depth 0` (~85 ms vs ~0.6 ms) |

**Measure with `cl::Event` profiling.** Requires `CL_QUEUE_PROFILING_ENABLE` at queue creation — without it, timestamps return zero.

## Troubleshooting

- **OpenGL interop on Optimus/hybrid GPU**: force GLFW onto NVIDIA GPU with PRIME render offload:

  ```bash
  __NV_PRIME_RENDER_OFFLOAD=1 __GLX_VENDOR_LIBRARY_NAME=nvidia GPU=NVIDIA ./build/b2_ray_tracer --live
  ```

- **BVH renders black patches**: miss-link pointers are wrong — verify parent-child-sibling linkage.
- **`cl_khr_gl_sharing` not available**: check `clinfo | grep gl_sharing`. Not available on all CPU-fallback runtimes (PoCL).
- **Wrong GPU**: `GPU=NVIDIA ./build/ray_tracer_bvh`.

## What's Next

[Track A: Multimedia & AI](../02_Multimedia/Multimedia.md) — zero-copy video pipelines and edge AI inference.

[Track C: Robotics & ROS 2](../04_Robotics/RoboticsROS2.md) — GPU acceleration inside a ROS 2 node.

[Optimization Toolbox](../05_Toolbox/Toolbox.md) — Thread Divergence, Local Memory, Async Pipelines.

[Bonus Modules](../06_Bonus/Bonus.md) — CLBlast MatMul and Device Enqueue (OpenCL 2.0).
