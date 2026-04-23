# Spec: LBVH Traversal

## Purpose
Defines requirements for the OpenCL ray-traversal kernel over a Karras LBVH: stack-based traversal in local memory, runtime stack sizing via CLI args, visual correctness validation, and mandatory `cl::Event` profiling.

## Requirements

### Requirement: Stack-based ray traversal over Karras tree
The traversal kernel SHALL traverse the LBVH using a per-work-item stack allocated in local memory. The stack SHALL hold up to 32 node indices. Each work-item SHALL push child nodes when a ray hits an internal node's AABB and pop the next candidate when it misses or finishes a leaf.

#### Scenario: Ray hits a triangle
- **WHEN** a ray is cast toward a triangle in the scene
- **THEN** the traversal kernel returns the correct hit distance `t` and the triangle's index

#### Scenario: Ray misses all geometry
- **WHEN** a ray is cast in a direction with no triangles
- **THEN** the traversal kernel returns `t = FLT_MAX` and no triangle index

#### Scenario: Stack does not overflow for bunny.obj
- **WHEN** the traversal kernel runs on bunny.obj (~70 k triangles, expected LBVH depth ≈ 17)
- **THEN** stack depth never exceeds 32 entries for any ray direction

---

### Requirement: Local memory stack sizing
The stack SHALL be declared as `__local int stack[WG_SIZE][STACK_DEPTH]`. Both constants SHALL be injected at `clBuildProgram` time as `-D` flags whose values come from CLI11 args `--wg-size` (default 64) and `--stack-depth` (default 32). No `target_compile_definitions` in CMake; the binary determines values at startup.

#### Scenario: Local memory budget check
- **WHEN** the traversal kernel is compiled
- **THEN** total local memory per work-group (`WG_SIZE × STACK_DEPTH × 4` bytes) does not exceed the device's `CL_DEVICE_LOCAL_MEM_SIZE`; the host SHALL query this value and throw `std::runtime_error` if the budget is exceeded

---

### Requirement: Traversal output matches reference render
The LBVH traversal kernel SHALL produce an output image with no systematic black patches or missing geometry when rendering bunny.obj at 800×600.

#### Scenario: Visual correctness check
- **WHEN** `--output render.bmp` is given with bunny.obj
- **THEN** the binary exits zero, framebuffer mean is in (0.01, 0.99), and framebuffer variance exceeds 0.001 — verified programmatically at runtime

---

### Requirement: cl::Event profiling for traversal timing
The traversal kernel enqueue SHALL use a `cl::Event` to measure GPU execution time. The host SHALL print traversal time in milliseconds to 3 decimal places alongside BVH build time in the per-frame timing table.

#### Scenario: Profiling enabled at queue creation
- **WHEN** the command queue is created with `CL_QUEUE_PROFILING_ENABLE`
- **THEN** `CL_PROFILING_COMMAND_START` and `CL_PROFILING_COMMAND_END` return non-zero values for the traversal event
