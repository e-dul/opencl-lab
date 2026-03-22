# Generic Kernel Templates

**Symptom**: You have `blur_float.cl`, `blur_uchar.cl`, `blur_half.cl` — identical logic, different types. Adding a feature means updating three files.

## Prerequisites
Prerequisites: OpenCL 1.2+, CMake 3.18+, `clinfo` installed. See [main README](../../README.md) for base requirements.

## Contents
```
01_Basic_MAD/    Single-type MAD kernel — the starting point
02_Generic_MAD/  Macro-based generic (uchar / float)
03_AutoTune/     Runtime type selection + profiling
kernels/
└── mad_kernel.cl   Shared source with #ifdef TYPE
```

## Build & Run
```bash
cd 99_Toolbox/GenericKernelTemplates
cmake -B build && cmake --build build

# Step 1 — single-type baseline
./build/01_basic_mad --width 1920 --height 1080

# Step 2 — macro-generic (uchar / float variants from one source)
./build/02_generic_mad --width 1920 --height 1080

# Step 3 — runtime autotuning across all types
./build/03_autotune --width 1920 --height 1080 --autotune
```

## Verify
```
[uchar ] MAD kernel: 1.2 ms  ← fastest for 8-bit images
[float ] MAD kernel: 2.1 ms
[half  ] MAD kernel: 1.8 ms  (hardware half support varies)
Autotuner selected: uchar for this device
```

Run with `--autotune` first. See which type wins on your hardware before reading the concept.

## Concept

OpenCL's `clBuildProgram` accepts a `-D` options string, identical to `gcc -D`. One `.cl` source file can be compiled into multiple variants at runtime:

```cl
// mad_kernel.cl — single source, multiple types
#ifndef TYPE
  #define TYPE uchar   // default
#endif
typedef TYPE scalar_t;

__kernel void mad(__global scalar_t* out, __global const scalar_t* in,
                  float contrast, float brightness) {
    int id = get_global_id(0);
    out[id] = (scalar_t)(in[id] * contrast + brightness);
}
```

```cpp
// Host: build three variants from the same source
auto build = [&](const char* type_flag) {
    cl::Program prog(ctx, source);
    prog.build(std::string("-D TYPE=") + type_flag);
    return prog;
};
auto prog_uchar = build("uchar");
auto prog_float = build("float");
auto prog_half  = build("half");
```

**Autotuning**: at startup, run each variant on a representative input, pick the fastest, cache the choice. The `03_AutoTune/` step implements this pattern.

**Performance gate**: generic `float` MAD < 1 ms on a 1080p image. If not, verify the build flag is reaching the compiler: `prog.getBuildInfo<CL_PROGRAM_BUILD_OPTIONS>(device)`.

## Mini-Challenge

Add a fourth type `int` to the kernel template. Measure its performance vs `uchar`. Why does integer arithmetic sometimes underperform on certain GPU architectures?

## Troubleshooting

- **`half` type fails to compile**: Guard with `#pragma OPENCL EXTENSION cl_khr_fp16 : enable` and check device extensions before building.
- **All variants run at the same speed**: The bottleneck is memory bandwidth, not compute. Arithmetic type only matters when the kernel is compute-bound.

## Used In
- [Track A — A2_YUV_Pipeline](../../02_Projects/A_Multimedia/Multimedia.md#a2_yuv_pipeline--see-what-the-camera-actually-sends) (uchar vs float YUV conversion kernel)
- [Track B — B3_Ray_Tracer_BVH](../../02_Projects/B_Graphics_HPC/GraphicsHPC.md#b3_ray_tracer_bvh--flagship-project) (float/half ray payload type selection)
- [Track C — C3_Perception_Node](../../02_Projects/C_Robotics_ROS2/RoboticsROS2.md#c3_perception_node--flagship-project) (multi-type point cloud processing kernels)

---

[Back to Toolbox](../Toolbox.md)
