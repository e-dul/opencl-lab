# Task 016: Toolbox — FastMath

## Context
- **Design Feature:** `workflow/design/07-toolbox.md`
- **Milestone:** Phase 11 — FastMath (`99_Toolbox/FastMath/`)
- **Relevant Files:**
  - `workflow/design/07-toolbox.md` — (read-only: specs, performance gates, directory layout)
  - `.claude/rules/00_master_specs.md` — (read-only: CLI11, CMake, profiling rules)
  - `common/ocl_wrapper.hpp` — (read-only: `create_context()`)
  - `common/opencl_utils.hpp` — (read-only: `CL_CHECK`)
  - `99_Toolbox/FastMath/` — (new directory)

## Objective

Implement the FastMath toolbox tool: a ray-normalization kernel compiled in three variants (standard `sqrt`/`rsqrt`, `half_` prefix, `native_` prefix) with an optional `-cl-fast-relaxed-math` comparison, demonstrating ≥ 4× throughput gain for `native_rsqrt` over standard `rsqrt` on 1 million ray normalizations.

## Constraints & Rules
- **No Design Changes:** Do not modify `workflow/design/*.md`. If architecture is wrong, stop and ask.
- **Language/Standard:** C++17.
- **Dependencies:** `cl.hpp` (OpenCL 1.2), CLI11 (via `common/common.cmake`), `stb_image_write.h` (not needed — numeric tool).
- **Error Handling:** Throw `std::runtime_error` on CL errors; use `CL_CHECK()` macro.
- **Profiling:** All timing via `cl::Event` (`CL_QUEUE_PROFILING_ENABLE`). Wall-clock results do NOT satisfy the performance gate.
- **Precision Note:** Do NOT compare output values between vendors for `native_rsqrt` — bit-level results differ. Correctness check (max error vs reference) is vendor-local only.
- **`half_` guard:** Check `cl_khr_fp16` extension at runtime; skip `half_rsqrt` variant and print "cl_khr_fp16 not available — skipping half_ variant" if absent.

---

## Implementation

1. **Directory scaffold:**
   ```
   99_Toolbox/FastMath/
   ├── CMakeLists.txt
   ├── main.cpp
   └── kernels/
       └── ray_kernel.cl
   ```

2. **`kernels/ray_kernel.cl`:**
   - Single `.cl` file with three kernel entry points (or one entry point selected via `-D VARIANT=`):
     - `ray_normalize_standard`: uses `sqrt` / `rsqrt` (standard precision).
     - `ray_normalize_half`: uses `half_rsqrt` (fast approximation, guarded by `#ifdef USE_HALF`).
     - `ray_normalize_native`: uses `native_rsqrt` (maximum speed, implementation-defined precision).
   - Input: buffer of `float4` rays (xyz = direction, w = unused).
   - Output: buffer of `float4` normalized rays.
   - Prefer compile-time dispatch (`-D VARIANT=STANDARD|HALF|NATIVE`) so each kernel variant is a separate binary object — avoids runtime branching inside the kernel.

3. **`main.cpp`:**
   - CLI11 flags:
     - `--rays <N>` (default: 1 000 000) — number of `float4` ray vectors.
     - `--relaxed` — if set, append `-cl-fast-relaxed-math` to all build options.
   - Workflow:
     1. Generate `N` random unit-ish `float4` rays on the host.
     2. Allocate input/output `cl::Buffer` (READ_ONLY / WRITE_ONLY).
     3. Build three program variants by compiling `ray_kernel.cl` three times with different `-D VARIANT=` flags. Skip HALF variant if `cl_khr_fp16` absent.
     4. For each variant, run the kernel with `CL_QUEUE_PROFILING_ENABLE`, capture `cl::Event`, compute kernel time in ms.
     5. Correctness check (host-side): read back STANDARD result; compute max relative error vs host `sqrtf`. Print max error. Skip cross-vendor pixel comparison.
     6. Print structured table:
        ```
        Variant           Rays       Kernel (ms)   Speedup vs Standard
        standard          1000000    12.345        1.00×
        half_rsqrt        1000000     4.210        2.93×
        native_rsqrt      1000000     2.987        4.13×
        ```
     7. If `--relaxed` was passed, re-run all three variants with `-cl-fast-relaxed-math` and print a second table.

4. **`CMakeLists.txt`:**
   - `cmake_minimum_required(VERSION 3.18)`, `project(fast_math)`.
   - `find_package(OpenCL REQUIRED)`.
   - Include `common/common.cmake` (CLI11 + shared headers).
   - `add_executable(fast_math main.cpp)`.
   - Link `OpenCL::OpenCL`, `CLI11::CLI11`.
   - `add_custom_command` to copy `kernels/` to binary dir post-build.
   - Standalone build: `cmake -B build && cmake --build build` from `99_Toolbox/FastMath/`.

## Definition of Done (DoD)

- [ ] `cmake -B build && cmake --build build` succeeds from `99_Toolbox/FastMath/` with no errors or warnings.
- [ ] `./build/fast_math --rays 1000000` prints a 3-row timing table (standard, half_ [or skipped], native_).
- [ ] `native_rsqrt` kernel time is ≥ 4× faster than `standard` on a discrete GPU (gate from design doc).
- [ ] `--relaxed` flag produces a second timing table using `-cl-fast-relaxed-math` compile options.
- [ ] `half_rsqrt` variant is skipped gracefully (printed notice, clean exit) on devices without `cl_khr_fp16`.
- [ ] All timing values are sourced from `cl::Event` profiling — not wall-clock.
- [ ] `--help` prints CLI11-generated usage with all flags documented.

---

## Execution Report
<!-- Filled by @coder after implementation. -->

- **Status:** PENDING
- **Session:** —

### Validation
```
[output here]
```

### Changed Files
| File | Change |
|------|--------|
| `99_Toolbox/FastMath/CMakeLists.txt` | Created |
| `99_Toolbox/FastMath/main.cpp` | Created |
| `99_Toolbox/FastMath/kernels/ray_kernel.cl` | Created |

### Remaining
- [ ] Implementation
