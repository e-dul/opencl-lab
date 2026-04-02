# Task T072: Silicon Realities Lessons

## Context
- **Design Feature:** `workflow/design/D12_v2_2_improvements.md`
- **Milestone:** Phase 3 — Silicon Realities Lessons
- **Relevant Files:**
  - `workflow/design/D12_v2_2_improvements.md` — (read-only: spec)
  - `03_GraphicsHPC/02_Ray_Tracer_BVH/RayTracerBVH.md` — (to modify)
  - `04_Robotics/03_Perception_Node/PerceptionNode.md` — (to modify)

## Objective

Add two inline "Stop and Read" engineering callout sections to project module READMEs: a `float3` alignment trap lesson in the BVH Ray Tracer README, and a hardware-safe C++ struct lesson in the Perception Node README.

## Constraints & Rules

- **Documentation only.** Zero modifications to `.cpp`, `.cl`, or `CMakeLists.txt` files.
- **Callout box format:** `> **Stop and Read: <Title>**` blockquote per the design spec.
- **AMD NaN note framing:** The `normalize()` / zero-vector `float3` behavior must be framed as a hardware reality check, not a spec bug, to avoid misleading students on other platforms.
- All standard constraints from `00_master_specs.md` are inherited but not applicable (no code changes).

---

## Implementation

### 3a — `float3` Alignment Trap → `03_GraphicsHPC/02_Ray_Tracer_BVH/RayTracerBVH.md`

**Where to insert:** Append as a new top-level section near the bottom of the README, before any back-link footer. Title: `## Silicon Realities`.

**Content to add:**

```markdown
## Silicon Realities

> **Stop and Read: The float3 Alignment Trap**
>
> OpenCL aligns `float3` to **16 bytes** — the same as `float4`. A struct with a `float3` member
> therefore contains an invisible 4-byte padding hole after it:
>
> ```c
> // Host C++ struct — appears to be 12 bytes, is actually 16
> typedef struct { float x, y, z; } Ray;  // + 4 bytes silent padding
> ```
>
> This matters because a `float3` array on the host (`std::vector<cl_float3>`) lays out elements
> at 16-byte strides, not 12. If you pack ray data as `float x, y, z` with no padding field, the
> host and device see different memory layouts — producing corrupted ray directions with zero
> symptoms at launch.
>
> **Rule:** Either use `float4` (explicit `w = 0`) or add an explicit `float pad` field and verify
> with `static_assert(sizeof(Ray) == 16, "Ray struct ABI mismatch")`.
>
> **AMD-specific reality:** On some AMD drivers, calling `normalize()` on a zero-length `float3`
> (e.g., a miss ray hitting the background) silently produces `NaN` components rather than an
> implementation-defined result. This causes `NaN` to propagate through shading and surface as
> black or corrupted pixels. Defensive fix: replace `dot(a, b)` with an explicit
> `dot3(a, b) = a.x*b.x + a.y*b.y + a.z*b.z` helper for `float3` operands, and guard
> `normalize()` calls with a length check.
```

### 3b — Hardware-Safe C++ Structs → `04_Robotics/03_Perception_Node/PerceptionNode.md`

**Where to insert:** Append as a new top-level section near the bottom of the README, before any back-link footer. Title: `## Silicon Realities`.

**Content to add:**

```markdown
## Silicon Realities

> **Stop and Read: Hardware-Safe C++ Structs**
>
> When sharing a C++ struct between host code and an OpenCL kernel, the device imposes its own
> alignment rules — which may differ from the host compiler's layout. Silent corruption results
> when the two disagree.
>
> Two mandatory defences:
>
> **1. Explicit padding fields**
>
> ```cpp
> struct PointCloud {
>     cl_float x, y, z;
>     cl_int   pad;     // satisfies 16-byte device alignment; never access on host
> };
> ```
>
> **2. Compile-time size assertion**
>
> ```cpp
> static_assert(sizeof(PointCloud) == 16,
>     "PointCloud ABI mismatch — check device alignment");
> ```
>
> The `static_assert` catches ABI drift at compile time, not at runtime with silent data
> corruption. Add one for every struct that crosses the host/device boundary. If the assertion
> fires after a refactor, fix the struct layout before touching any kernel code.
```

---

## Definition of Done (DoD)

- [x] `03_GraphicsHPC/02_Ray_Tracer_BVH/RayTracerBVH.md` contains a `## Silicon Realities` section with a `> **Stop and Read: The float3 Alignment Trap**` callout block.
- [x] `04_Robotics/03_Perception_Node/PerceptionNode.md` contains a `## Silicon Realities` section with a `> **Stop and Read: Hardware-Safe C++ Structs**` callout block.
- [x] Zero `.cpp`, `.cl`, or `CMakeLists.txt` files are modified.
- [x] The AMD `normalize()` / `float3` zero-vector note is framed as a hardware reality (not a spec bug).
- [x] Both callouts include at least one concrete code snippet illustrating the problem and the fix.
- [x] The `static_assert` pattern appears in the 3b callout.

---

## Execution Report
<!-- Filled by @coder after implementation. -->

- **Status:** COMPLETE
- **Session:** 2026-03-28

### Validation
```
03_GraphicsHPC/02_Ray_Tracer_BVH/RayTracerBVH.md:89:## Silicon Realities
04_Robotics/03_Perception_Node/PerceptionNode.md:147:## Silicon Realities
03_GraphicsHPC/02_Ray_Tracer_BVH/RayTracerBVH.md:91:> **Stop and Read: The float3 Alignment Trap**
04_Robotics/03_Perception_Node/PerceptionNode.md:149:> **Stop and Read: Hardware-Safe C++ Structs**
04_Robotics/03_Perception_Node/PerceptionNode.md:169:> static_assert(sizeof(PointCloud) == 16,
03_GraphicsHPC/02_Ray_Tracer_BVH/RayTracerBVH.md:109:> **AMD-specific reality:** On some AMD drivers, calling `normalize()` on a zero-length `float3`
git diff --name-only HEAD | grep -E '\.(cpp|cl)$|CMakeLists\.txt' → NONE
```

### Changed Files
| File | Change |
|------|--------|
| `03_GraphicsHPC/02_Ray_Tracer_BVH/RayTracerBVH.md` | Modified — appended Silicon Realities section |
| `04_Robotics/03_Perception_Node/PerceptionNode.md` | Modified — appended Silicon Realities section |
