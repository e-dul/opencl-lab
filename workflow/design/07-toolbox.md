# Module 7: Optimization Toolbox (`99_Toolbox/`)

**Version:** 1.0
**Status:** Active — Phase 1–5 complete (ZeroCopy, CoalescedAccess, LocalMemory, ThreadDivergence, WorkGroupSizing), Phase 6 next
**Module Path:** `99_Toolbox/`

---

## Goal

Provide a library of isolated, standalone GPU optimization techniques. Each tool is self-contained: it demonstrates one bottleneck, shows a measurable before/after, and links back to the Module 2 project where the technique applies. Users come here when profiling reveals a specific problem — not front-to-back.

## Non-goals

- Teaching OpenCL host API fundamentals (covered in Module 1)
- Project-scale pipelines or end-to-end applications (covered in Module 2)
- vkFFT, CLBlast, or third-party library integration (covered in Module 4 / Path B)
- Theoretical deep-dives on SVM or memory coherency models (deferred to Add-on 4.4)
- ROS 2 loaned messages (covered in Path C)

---

## Roadmap / Status

- [x] Phase 1: ZeroCopy — `CL_MEM_COPY_HOST_PTR` vs `ALLOC_HOST_PTR` vs `USE_HOST_PTR`; visual BMP output + timing table.
  - *Context*: Executive Summary §Tool 1; `99_Toolbox/ZeroCopy/ZeroCopy.md`.
- [x] Phase 2: CoalescedAccess — row-major vs column-major vs transposed access patterns; 7x gap visible on discrete GPU.
  - *Context*: Executive Summary §Tool 1; `99_Toolbox/CoalescedAccess/CoalescedAccess.md`.
- [x] Phase 3: LocalMemory (LDS) — global vs local memory box blur (tile + halo pattern); 4x+ speedup at radius 5.
  - *Context*: Executive Summary §Tool 2; `99_Toolbox/LocalMemory/LocalMemory.md`.
- [x] Phase 4: ThreadDivergence — `if-else` vs `select()` branchless on mask-conditioned blur; 2x gap visible.
  - *Context*: Executive Summary §Tool 2; `99_Toolbox/ThreadDivergence/ThreadDivergence.md`.
- [x] Phase 5: WorkGroupSizing — automated sweep of `local_work_size`; occupancy calculator output.
  - *Context*: Executive Summary §Tool 2; `99_Toolbox/WorkGroupSizing/WorkGroupSizing.md`.
- [ ] Phase 6: Debugging — Oclgrind out-of-bounds and race-condition demos; Nsight/VTune workflow guide.
  - *Context*: Executive Summary §Tool 3; `99_Toolbox/Debugging/Debugging.md`.
- [ ] Phase 7: GenericKernelTemplates — single `.cl` source compiled as `uchar`/`float`/`half`; runtime autotuner selects fastest type.
  - *Context*: Executive Summary §Tool 4; `99_Toolbox/GenericKernelTemplates/GenericKernelTemplates.md`.
- [ ] Phase 8: AsyncMultiThread — blocking baseline → OOO queue → per-thread queues; 2x pipeline overlap visible.
  - *Context*: Executive Summary §Tool 5; `99_Toolbox/AsyncMultiThread/AsyncMultiThread.md`.
- [ ] Phase 9: MultiGPU_Strategy — single-GPU vs dual-GPU on 4K workload; ≥2× speedup gate.
  - *Context*: Executive Summary §Tool 6; `99_Toolbox/MultiGPU_Strategy/MultiGPUStrategy.md`.
- [ ] Phase 10: SVM — coarse vs fine-grained SVM vs buffer+map baseline; OpenCL 2.0 runtime guard.
  - *Context*: Executive Summary §Tool 1 (SVM); `99_Toolbox/SVM/SVM.md`.
- [ ] Phase 11: FastMath — standard vs `half_` vs `native_` math functions; `-cl-fast-relaxed-math` flag demo.
  - *Context*: Executive Summary §Path B B.3; `99_Toolbox/FastMath/FastMath.md`.
- [ ] Phase 12: Module review and cleanup — verify all tools build standalone, cross-link "Used In" references, confirm all DoDs. Make executables names consistent and code using utils for image operations.

---

## Specifications

> **Inherits**: `.claude/rules/00_master_specs.md`

**Additional constraints for this module:**

- **Standalone Build Mandatory**: Every tool subdirectory must build independently via `cmake -B build && cmake --build build` from within that subdirectory. No shared CMake parent required.
- **Before/After Pattern**: Every tool must implement at minimum two code paths (naive vs optimized) and print timing for both. A single number in isolation is not acceptable output.
- **Visual Artifact**: Tools operating on image data must produce `output.bmp` via `stb_image_write`. Tools that are purely numeric (e.g., MultiGPU_Strategy, SVM, Debugging) must print a structured console timing table.
- **OpenCL 2.0 Gating (SVM)**: All SVM code must be guarded by `#ifdef CL_VERSION_2_0`. The SVM tool must fall back gracefully (print "SVM not supported" and exit cleanly) on OpenCL 1.2 devices.
- **CLI Arguments**: All binaries use CLI11. Minimum required flags per tool class:
  - Image tools: `--width`, `--height`, `--image` (optional input BMP).
  - Numeric/pipeline tools: `--size` or `--rays` or `--gpus` as appropriate.
  - Multi-mode tools: `--mode <variant>` (e.g., `async_demo --mode basic_sync`).
- **Profiling**: All timing via `cl::Event` profiling (`CL_QUEUE_PROFILING_ENABLE`). Wall-clock measurements do not satisfy any performance gate.

---

## Architecture (high-level)

### Components

Each tool is an independent C++17 executable with its own `CMakeLists.txt`. There is no shared Toolbox library — copy-paste is the intended distribution model.

- **ZeroCopy** (`ZeroCopy/`): Three buffer creation strategies compared on the same image upload workload. Outputs timing table + `output.bmp` (processed with the uploaded buffer to confirm correctness).
- **CoalescedAccess** (`CoalescedAccess/`): Three access patterns on a 2D array: row-major (coalesced), column-major (uncoalesced), transposed fix. Outputs timing table + output images.
- **LocalMemory** (`LocalMemory/`): Box blur implemented twice — pure global memory reads, then tile+halo pattern using `__local`. Parameterised by `--radius`. Outputs timing + `output.bmp`.
- **ThreadDivergence** (`ThreadDivergence/`): Mask-conditional per-pixel operation: `if-else` path vs `select()` branchless path. Outputs timing + `output.bmp` (both paths must produce identical pixel output — verified by the demo).
- **WorkGroupSizing** (`WorkGroupSizing/`): Automated sweep of `local_work_size` values for a given kernel. Prints occupancy estimate alongside kernel time at each step.
- **Debugging** (`Debugging/`): Demo binary with injected bugs (out-of-bounds write, race condition). Designed to be run under Oclgrind. No BMP output required — Oclgrind error report is the artifact.
- **GenericKernelTemplates** (`GenericKernelTemplates/`): MAD kernel compiled at runtime for `uchar`, `float`, `half` via `-D TYPE=`. Three sub-steps: `01_Basic_MAD/`, `02_Generic_MAD/`, `03_AutoTune/`. Autotuner selects fastest type and prints the result.
- **AsyncMultiThread** (`AsyncMultiThread/`): Pipeline timing in three configurations: blocking, OOO queue, per-thread queues. Three sub-steps: `01_BasicSync/`, `02_AsyncSingle/`, `03_MultiThreadAsync/`. Verification artifact is an event timeline (start/end timestamps via `cl::Event::getProfilingInfo`) showing compute/transfer overlap — not just a ratio.
- **MultiGPU_Strategy** (`MultiGPU_Strategy/`): Detects all GPU devices, creates shared context, dispatches image slices via round-robin/proximity strategy, merges results on host. Per-GPU `cl::Event` profiling enables load-rebalancing analysis. Compares single-GPU vs N-GPU throughput.
- **SVM** (`SVM/`): Buffer + map/unmap baseline vs coarse SVM vs fine-grained SVM. Runtime guard falls back on OpenCL 1.2 devices.
- **FastMath** (`FastMath/`): Ray normalization kernel compiled in three variants: standard `sqrt`/`rsqrt`, `half_` prefix, `native_` prefix. Optional `-cl-fast-relaxed-math` comparison.

### Data Flow (per tool)

1. Parse CLI args (CLI11).
2. Select device via `common/ocl_wrapper.hpp → create_context()` (respects `GPU` env var).
3. Load or generate input data (image or synthetic buffer).
4. Run naive path — capture `cl::Event` timing.
5. Run optimized path — capture `cl::Event` timing.
6. Print structured comparison table (ms to 3 decimal places).
7. Save `output.bmp` where applicable.

---

## Key Decisions (and Rationale)

1. **One Tool, One Bottleneck**
   - **Why**: Context-switching between multiple concepts in a single demo obscures the educational signal. Each tool isolates exactly one variable so the profiler output maps directly to one technique.

2. **Sub-step Progression (01_Basic / 02_Optimized / 03_AutoTune)**
   - **Why**: Mirrors the Snapshot-over-Branches convention from master specs. Users can diff `01_Basic_MAD/` against `02_Generic_MAD/` in their IDE without switching git branches. Applied in GenericKernelTemplates and AsyncMultiThread.

3. **No Shared Toolbox Library**
   - **Why**: The standalone-buildable rule requires that copying a single tool folder is sufficient. A shared library would break this. All tools link `common/` headers only (header-only, no compiled artifact).

4. **Before/After Pattern Mandatory**
   - **Why**: A single timing number teaches nothing. The ratio (e.g., "7x slower, uncoalesced") is the educational artifact. Every tool must print both the baseline and the optimized result in the same run.

5. **SVM Treated as Optional / Guarded**
   - **Why**: SVM is OpenCL 2.0. Nvidia (the most common learner GPU) caps at OpenCL 1.2. Requiring SVM for module completion would block the majority of users. The tool is a bonus for APU/iGPU owners.

6. **Debugging Tool Targets Oclgrind, Not a Real GPU**
   - **Why**: Oclgrind instruments CPU-executed OpenCL kernels and catches memory errors that are silent on real hardware (out-of-bounds writes produce no GPU fault). The educational value is confirming that bugs Oclgrind flags are real — this requires injecting known bugs, not writing correct code.

---

## Known Issues / Risks

- **`CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE` Unreliable**: Some drivers (notably older Intel CPU runtimes) silently fall back to in-order execution. AsyncMultiThread must detect this condition and warn the user if the OOO speedup is absent.
- **MultiGPU Mixed-Vendor Context**: A `cl::Context` spanning devices from different platforms is not supported in OpenCL 1.2. MultiGPU_Strategy must document this and implement the cross-platform path as separate per-vendor contexts synchronized on the host.
- **`half` Type in GenericKernelTemplates**: `cl_khr_fp16` is not universally supported. The `half` variant must be guarded by a runtime extension check and skip gracefully if unavailable.
- **WorkGroupSizing `local_work_size` Must Divide `global_work_size`**: Autotuning sweeps must pad the global work size to the next multiple of the tested work-group size. Underpaddded images will produce `CL_INVALID_WORK_GROUP_SIZE` at runtime.
- **Oclgrind Not Installed by Default**: The Debugging tool build must not fail if Oclgrind is absent. CMake should emit a warning, not an error. The executable still builds; the README instructs the user to install Oclgrind before running.
- **`native_` Math Precision Variance Across Vendors**: FastMath `native_rsqrt` output differs between AMD, Nvidia, and Intel at the bit level. The demo must not compare output pixels between vendors as part of its DoD.
- **ZeroCopy Performance Gate Not Met on Discrete GPU (Nvidia RTX 4060)**: Validated 2026-03-03. `ALLOC_HOST_PTR` (0.076 ms) vs `COPY_HOST_PTR` (0.078 ms) shows ~1.03x difference — well below the ≥3x gate. This is expected on discrete GPUs with non-unified memory (HOST_UNIFIED_MEMORY=NO) where all strategies still traverse PCIe for the passthrough kernel. The gate may be achievable on APU/iGPU (unified memory) hardware.

- **CoalescedAccess Performance Gate Not Met on RTX 4060 Laptop GPU**: Validated 2026-03-03. COL-MAJOR (0.054 ms) vs ROW-MAJOR (0.036 ms) shows ~1.5× ratio — below the ≥5× gate at 1920×1080. Mobile GPU L2 cache absorbs the uncoalesced access penalty at this array size. The gate is expected to be observable on desktop discrete GPUs or iGPUs with unified memory. To reproduce the gate threshold, use 8192×8192 or run on an iGPU.

- **LocalMemory Performance Gate Not Met on NVIDIA RTX 4060 Laptop GPU (Ada Lovelace)**: Validated 2026-03-04. `blur_local` is ~1.77× faster than `blur_global` at radius 5 — below the ≥3× gate. Ada Lovelace's large L2 cache absorbs global memory latency, reducing the observable benefit of `__local` tiling. The speedup is real and measurable but bounded by on-die bandwidth rather than GDDR bandwidth. The ≥3× gate assumes a native GPU driver (CUDA/ROCm) with discrete GDDR memory and an architecture whose L2 does not fully cache the blur neighbourhood. Laptop mobile GPUs will not reliably meet this gate.

- **LocalMemory Performance Gate — AMD 680M via rusticl (Mesa/LLVM)**: `blur_local` is SLOWER than `blur_global` at radius ≥10 (observed 0.94× at r=10, 0.85× at r=20). Root cause: the rusticl driver does not optimize `__local` tile+halo patterns effectively. The hardware has LDS (Local Data Share) but the Mesa/LLVM backend emits suboptimal IR for cooperative tile-load patterns, causing the synchronization overhead to exceed the data-reuse benefit. This is a driver-quality limitation, not a hardware limitation. Re-test with ROCm `opencl-clang` driver when available.

- **LocalMemory Performance Gate — Hardware Requirement Clarification**: The ≥3× gate in the Performance Gates table assumes a native GPU driver (CUDA or ROCm) and discrete GDDR memory. iGPUs and Mesa rusticl backends will not meet this gate. The gate is retained as-is to reflect the target hardware class; results on integrated or driver-emulated devices should be documented as hardware-limited.

- **ThreadDivergence Performance Gate — AMD 680M iGPU via rusticl**: Validated 2026-03-04. `select()` speedup observed at 1.197× — below the ≥1.5× gate. Root cause: AMD 680M is an integrated GPU sharing the memory subsystem; thread divergence penalty is lower than on discrete GPUs and rusticl may serialize warp branches differently than a native ROCm driver. The ≥1.5× gate applies to discrete GPUs only. Results on iGPU/rusticl should be treated as hardware-class limited.
- **WorkGroupSizing `--kernel blur_r5` Not Implemented**: The CLI option `--kernel blur_r5` is accepted (parsed) but throws `std::runtime_error` at runtime. The `mad` kernel is the only implemented variant. `blur_r5` is deferred to a future task.
---

## Performance Gates (Module Completion)

| Tool | Metric | Target |
| :--- | :--- | :--- |
| ZeroCopy | `ALLOC_HOST_PTR` vs `COPY_HOST_PTR` upload speedup | ≥ 3× at 1080p on discrete GPU |
| CoalescedAccess | Row-major vs column-major kernel time ratio | ≥ 5× at 1920×1080 |
| LocalMemory | Local memory blur vs global memory blur at radius 5 | ≥ 3× faster |
| ThreadDivergence | `select()` vs `if-else` on divergent mask | ≥ 1.5× faster |
| WorkGroupSizing | Autotuner identifies sub-optimal vs optimal `local_work_size` | ≥ 2× gap visible in sweep |
| GenericKernelTemplates | Generic `float` MAD kernel on 1080p | < 1 ms |
| AsyncMultiThread | OOO async pipeline vs blocking baseline | ≥ 2× faster |
| MultiGPU_Strategy | Dual-GPU vs single-GPU on 4K workload | ≥ 2× speedup |
| FastMath | `native_rsqrt` vs standard `rsqrt` on 1M ray normalizations | ≥ 4× faster |

---

## Specifications & Standards

- **Directory Structure**:
  ```
  99_Toolbox/
  ├── Toolbox.md                     (existing user-facing index — do not modify)
  ├── ZeroCopy/
  │   ├── CMakeLists.txt
  │   ├── main.cpp
  │   └── kernels/zero_copy_kernel.cl
  ├── CoalescedAccess/
  │   ├── CMakeLists.txt
  │   ├── main.cpp
  │   └── kernels/coalesced_kernel.cl
  ├── LocalMemory/
  │   ├── CMakeLists.txt
  │   ├── main.cpp
  │   └── kernels/blur_kernel.cl
  ├── ThreadDivergence/
  │   ├── CMakeLists.txt
  │   ├── main.cpp
  │   └── kernels/divergence_kernel.cl
  ├── WorkGroupSizing/
  │   ├── CMakeLists.txt
  │   ├── main.cpp
  │   └── kernels/mad_kernel.cl
  ├── Debugging/
  │   ├── CMakeLists.txt
  │   ├── main.cpp
  │   └── kernels/debug_kernel.cl
  ├── GenericKernelTemplates/
  │   ├── CMakeLists.txt
  │   ├── 01_Basic_MAD/main.cpp
  │   ├── 02_Generic_MAD/main.cpp
  │   ├── 03_AutoTune/main.cpp
  │   └── kernels/mad_kernel.cl
  ├── AsyncMultiThread/
  │   ├── CMakeLists.txt
  │   ├── 01_BasicSync/main.cpp
  │   ├── 02_AsyncSingle/main.cpp
  │   ├── 03_MultiThreadAsync/main.cpp
  │   └── kernels/pipeline_kernel.cl
  ├── MultiGPU_Strategy/
  │   ├── CMakeLists.txt
  │   ├── main.cpp
  │   └── kernels/slice_kernel.cl
  ├── SVM/
  │   ├── CMakeLists.txt
  │   ├── main.cpp
  │   └── kernels/svm_kernel.cl
  └── FastMath/
      ├── CMakeLists.txt
      ├── main.cpp
      └── kernels/ray_kernel.cl
  ```
- **Verification Standard**:
  - Image tools (ZeroCopy, CoalescedAccess, LocalMemory, ThreadDivergence): produce `output.bmp` confirming the optimized path yields identical pixel output to the baseline.
  - Numeric tools (WorkGroupSizing, MultiGPU_Strategy, SVM, AsyncMultiThread, FastMath): structured console table with before/after timing. No BMP required.
  - Debugging tool: Oclgrind error output for the injected bug. Clean run (no error) after the fix.
  - GenericKernelTemplates: autotuner console output declaring the winning type for the current device.
- **Tooling**: See `> **Inherits**: .claude/rules/00_master_specs.md` — no module-specific additions.

---

## Prerequisites

- Module 1 completed (`01_Host_API/`): `cl::Event` profiling, `cl.hpp` usage, `CL_CHECK`.
- `common/ocl_wrapper.hpp` and `common/opencl_utils.hpp` present in the repository root.
- For Debugging tool: `sudo apt install oclgrind` (optional at build time, required at runtime).
- For SVM tool: OpenCL 2.0+ device (AMD APU, Intel iGPU, ARM Mali). Demo falls back gracefully without one.
- For MultiGPU_Strategy: two OpenCL-capable GPUs on the same system (optional; single-GPU baseline always runs).

See [main README](../README.md) for base requirements (OpenCL 1.2+, CMake 3.18+).
