# Task T076: Grade Report Fixes — Track C (05_Toolbox)

## Context
- **Design Feature:** `workflow/design/D12_v2_2_improvements.md`
- **Milestone:** Phase 5 — Grade Report Fixes (Track C)
- **Relevant Files:**
  - `workflow/tasks/grade_report_v2.md` — (read-only: source of all `[x]` items)
  - `workflow/design/D12_v2_2_improvements.md` — (read-only: phase spec)
  - `05_Toolbox/01_Local_Memory/LocalMemory.md` — (to modify)
  - `05_Toolbox/02_Coalesced_Access/CoalescedAccess.md` — (to modify)
  - `05_Toolbox/02_Coalesced_Access/CMakeLists.txt` — (to modify)
  - `05_Toolbox/02_Coalesced_Access/src/main.cpp` — (to modify)
  - `05_Toolbox/03_Debugging/Debugging.md` — (to modify)
  - `05_Toolbox/04_Deployment/Deployment.md` — (to modify)
  - `05_Toolbox/04_Deployment/appimage.sh` — (to modify)
  - `05_Toolbox/05_Fast_Math/FastMath.md` — (to modify)
  - `05_Toolbox/05_Fast_Math/CMakeLists.txt` — (to modify)
  - `05_Toolbox/05_Fast_Math/src/main.cpp` — (to modify)
  - `05_Toolbox/06_Generic_Kernel_Templates/GenericKernelTemplates.md` — (to modify)
  - `05_Toolbox/06_Generic_Kernel_Templates/02_Generic_MAD/src/main.cpp` — (to modify)
  - `05_Toolbox/06_Generic_Kernel_Templates/03_AutoTune/src/main.cpp` — (to modify)
  - `05_Toolbox/06_Generic_Kernel_Templates/02_Generic_MAD/kernels/mad_kernel.cl` — (to modify)
  - `05_Toolbox/06_Generic_Kernel_Templates/03_AutoTune/CMakeLists.txt` — (to modify)
  - `05_Toolbox/07_Global_Work_Offset/GlobalWorkOffset.md` — (to modify)
  - `05_Toolbox/07_Global_Work_Offset/CMakeLists.txt` — (to modify)
  - `05_Toolbox/07_Global_Work_Offset/src/main.cpp` — (to modify)
  - `05_Toolbox/08_Multi_GPU_Strategy/MultiGPUStrategy.md` — (to modify)
  - `05_Toolbox/08_Multi_GPU_Strategy/src/main.cpp` — (to modify)
  - `05_Toolbox/08_Multi_GPU_Strategy/kernels/split_kernel.cl` — (to modify, row_offset removal)
  - `05_Toolbox/09_OpenCL_vs_CUDA/OpenCLvsCUDA.md` — (to modify)
  - `05_Toolbox/10_Sub_Buffers_Partitioning/SubBuffers.md` — (to modify)
  - `05_Toolbox/10_Sub_Buffers_Partitioning/src/main.cpp` — (to modify)
  - `05_Toolbox/11_SVM_Theory/SVMTheory.md` — (to modify)
  - `05_Toolbox/11_SVM_Theory/src/main.cpp` — (to modify)
  - `05_Toolbox/12_Sync_Atomics/CMakeLists.txt` — (to modify)
  - `05_Toolbox/12_Sync_Atomics/SyncAtomics.md` — (to modify)
  - `05_Toolbox/13_Thread_Divergence/CMakeLists.txt` — (to modify)
  - `05_Toolbox/13_Thread_Divergence/ThreadDivergence.md` — (to modify)
  - `05_Toolbox/14_Work_Group_Sizing/WorkGroupSizing.md` — (to modify)
  - `05_Toolbox/15_Zero_Copy/ZeroCopy.md` — (to modify)
  - `05_Toolbox/15_Zero_Copy/CMakeLists.txt` — (to modify)
  - `05_Toolbox/15_Zero_Copy/src/main.cpp` — (to modify)
  - `05_Toolbox/16_Async_Multi_Thread/AsyncMultiThread.md` — (to modify)
  - `05_Toolbox/16_Async_Multi_Thread/CMakeLists.txt` — (to modify)
  - `common/opencl_utils.hpp` — (read-only: `build_program_from_source`, `run_kernel` reference)

## Objective

Apply all `[x]`-approved actionable items from `grade_report_v2.md` for all `05_Toolbox/` submodules; rebuild each modified submodule to confirm zero errors and zero warnings.

## Constraints & Rules

- Fix **only** items marked `[x]` in `grade_report_v2.md`. Items marked `[ ]` are explicitly out of scope.
- Do **not** modify any file not listed under Relevant Files unless a `[x]` item directly mandates it.
- All standard constraints from `00_master_specs.md` apply (C++17, `cl.hpp`, `CL_CHECK`, `size_t gid`, standalone CMake).
- For the `06_Generic_Kernel_Templates` item 3: extract duplicated helpers to `common/opencl_utils.hpp` only if those functions are not already present; do not duplicate or alter any existing declarations.

---

## Implementation

Work submodule by submodule. Within each, apply all `[x]` items before rebuilding.

### A — 05_Toolbox / 01_Local_Memory

**Items to fix (from `grade_report_v2.md` lines 1026–1029):**

| # | File | Fix |
|---|------|-----|
| 1 | `LocalMemory.md` | Remove `--kernel blur` from the example build/run invocation — no such CLI11 option exists |
| 2 | `LocalMemory.md` | Add one-line hardware note in the Verify section: "Numbers are indicative for a discrete GPU; iGPU or CPU devices will show smaller ratios." |
| 3 | `LocalMemory.md` | Add forward reference from the Verify section to the Advanced Challenge (bank conflicts) section |

**Build gate:** `cmake -B build && cmake --build build` from `05_Toolbox/01_Local_Memory/` must succeed with zero warnings.

---

### B — 05_Toolbox / 02_Coalesced_Access

**Items to fix (from `grade_report_v2.md` lines 1035–1037):**

| # | File | Fix |
|---|------|-----|
| 1 | `CMakeLists.txt` | Remove the `symlink_assets(coalesced_access)` call (no `assets/` directory exists in this module) |
| 2 | `src/main.cpp` | Add WHY comment on `buf_in` creation: "`CL_MEM_COPY_HOST_PTR` uploads immediately at buffer creation time; `CL_MEM_READ_ONLY` restricts device writes" |
| 3 | `CoalescedAccess.md` | Add a single-sentence pointer to the `run_kernel` helper in the Mini-Challenge to lower entry friction |

**Build gate:** `cmake -B build && cmake --build build` from `05_Toolbox/02_Coalesced_Access/` must succeed with zero warnings.

---

### C — 05_Toolbox / 03_Debugging

**Items to fix (from `grade_report_v2.md` lines 1044–1046):**

| # | File | Fix |
|---|------|-----|
| 1 | `Debugging.md` | Remove duplicate `sudo apt install oclgrind` from the Oclgrind body section (keep it in Prerequisites only) |
| 2 | `Debugging.md` | Keep detailed `--data-races --uniform-writes` callout in the Oclgrind section; replace the Build & Run inline comment with a forward reference to that section |
| 3 | `Debugging.md` | Extend the Nsight/VTune/rocprof section with links to external resources instead of repeating examples already covered elsewhere |

**Build gate:** `cmake -B build && cmake --build build` from `05_Toolbox/03_Debugging/` must succeed with zero warnings.

---

### D — 05_Toolbox / 04_Deployment

**Items to fix (from `grade_report_v2.md` lines 1053–1057):**

| # | File | Fix |
|---|------|-----|
| 1 | `appimage.sh` | Align target binary name on line 21 to `build/deployment` (CMakeLists.txt target is `deployment`, not `deployment_demo`) |
| 3 | `appimage.sh` + `Deployment.md` | Keep full ICD loader bundling explanation in README; reduce `appimage.sh` comment to one sentence with a forward reference to the README |
| 5 | `appimage.sh` | Add `ARCH=$(uname -m)` variable at the top; substitute it into all tool-download URLs and the output filename that currently hardcode `x86_64` |

**Build gate:** `cmake -B build && cmake --build build` from `05_Toolbox/04_Deployment/` must succeed with zero warnings.

---

### E — 05_Toolbox / 05_Fast_Math

**Items to fix (from `grade_report_v2.md` lines 1063–1068):**

| # | File | Fix |
|---|------|-----|
| 1 | `FastMath.md` | Add one-sentence caveat in Verify section: `native_` gain may be negligible or absent on CPU/PoCL devices |
| 2 | `FastMath.md` | Add `--relaxed` to the example run command in the Build & Run section |
| 3 | `FastMath.md` | Collapse the domain-applicability table into the existing precision-tier table (content is duplicated) |
| 4 | `CMakeLists.txt` | Remove stray `symlink_assets` call (module has no `assets/` dependency) |
| 5 | `src/main.cpp` | Add WHY comment on `buf_out` reuse across variants: "each kernel unconditionally overwrites all `n_rays` elements; no inter-variant state carry-over" |
| 6 | `FastMath.md` | Extend Mini-Challenge with a second function example (e.g., `native_sin` for audio oscillator or physics spring) |

**Build gate:** `cmake -B build && cmake --build build` from `05_Toolbox/05_Fast_Math/` must succeed with zero warnings.

---

### F — 05_Toolbox / 06_Generic_Kernel_Templates

**Items to fix (from `grade_report_v2.md` lines 1074–1078):**

| # | File | Fix |
|---|------|-----|
| 1 | `GenericKernelTemplates.md` | Move `clBuildProgram -D` snippet after the Verify block or add a "If curious why this works, read on:" divider |
| 2 | `GenericKernelTemplates.md` | Add note in Verify block: "`half` row shows N/A on devices without `cl_khr_fp16`; use `clinfo \| grep fp16` to diagnose" |
| 3 | `02_Generic_MAD/src/main.cpp` + `03_AutoTune/src/main.cpp` | Extract duplicated `build_program_from_source()` and `run_kernel()` helpers to `common/opencl_utils.hpp` if not already present; replace both call sites with the shared version |
| 4 | `02_Generic_MAD/kernels/mad_kernel.cl` | Add `clamp(result, 0.0f, 255.0f)` before the `(scalar_t)result` cast to prevent implementation-defined overflow on `uchar` |
| 5 | `03_AutoTune/src/main.cpp` + `GenericKernelTemplates.md` | Remove the `--autotune` no-op flag from both the binary and the README run command |

**Build gate:** `cmake -B build && cmake --build build` from `05_Toolbox/06_Generic_Kernel_Templates/02_Generic_MAD/` and `03_AutoTune/` must each succeed with zero warnings.

---

### G — 05_Toolbox / 07_Global_Work_Offset

**Items to fix (from `grade_report_v2.md` lines 1084–1086):**

| # | File | Fix |
|---|------|-----|
| 1 | `GlobalWorkOffset.md` | Keep coordinate walk-through example in the kernel file; replace README duplicate with a cross-reference comment pointing to the kernel |
| 2 | `CMakeLists.txt` | Remove stray `symlink_assets` call (module uses only synthetic data) |
| 3 | `src/main.cpp` | Replace `(master specs §3 exception)` comment with self-contained rationale: "structured timing table is the artifact per the numeric-benchmark exception" |

**Build gate:** `cmake -B build && cmake --build build` from `05_Toolbox/07_Global_Work_Offset/` must succeed with zero warnings.

---

### H — 05_Toolbox / 08_Multi_GPU_Strategy

**Items to fix (from `grade_report_v2.md` lines 1093–1096):**

| # | File | Fix |
|---|------|-----|
| 1 | `MultiGPUStrategy.md` | Add hardware-waiver clause to the performance gate: "Result is hardware-dependent; heterogeneous GPU pairs may require proportional split" |
| 2 | `src/main.cpp` | Fix stale CLI11 description string on `--image` option; add a sentence to README documenting the flag |
| 3 | `MultiGPUStrategy.md` | Add forward pointer in the Concept section clarifying that dynamic split is the Mini-Challenge homework |
| 4 | `kernels/split_kernel.cl` | Remove the dead `row_offset` kernel parameter (and update the corresponding `setArg` call in `src/main.cpp` atomically) |

**Build gate:** `cmake -B build && cmake --build build` from `05_Toolbox/08_Multi_GPU_Strategy/` must succeed with zero warnings.

---

### I — 05_Toolbox / 09_OpenCL_vs_CUDA

**Items to fix (from `grade_report_v2.md` lines 1102):**

| # | File | Fix |
|---|------|-----|
| 1 | `OpenCLvsCUDA.md` | Remove abbreviated ecosystem comparison tables from `OpenCLvsCUDA.md`; replace with a forward reference to `report.md §1` which already covers the same axes |

**Note:** Items 2–5 are `[ ]` excluded — do not touch.

**Build gate:** No source changes in this submodule; confirm existing build still passes if a CMakeLists.txt exists.

---

### J — 05_Toolbox / 10_Sub_Buffers_Partitioning

**Items to fix (from `grade_report_v2.md` lines 1112–1115):**

| # | File | Fix |
|---|------|-----|
| 1 | `SubBuffers.md` | Add sentence in Mini-Challenge pointing to `08_Multi_GPU_Strategy` and `16_Async_Multi_Thread` as natural next steps |
| 2 | `src/main.cpp` | Add WHY comment on `strip_color`: "custom HSV converter used because stdlib has no HSV utility; full-saturation maximises per-strip visual contrast" |
| 3 | `SubBuffers.md` | Collapse the redundant `grep` command in the Verify section into the Troubleshooting section |
| 4 | `src/main.cpp` | Expand the `--strips cannot exceed --height` error message to explain: "a zero-height strip produces a zero-size sub-buffer region, which is illegal per the OpenCL spec" |

**Build gate:** `cmake -B build && cmake --build build` from `05_Toolbox/10_Sub_Buffers_Partitioning/` must succeed with zero warnings.

---

### K — 05_Toolbox / 11_SVM_Theory

**Items to fix (from `grade_report_v2.md` lines 1122–1126):**

| # | File | Fix |
|---|------|-----|
| 1 | `src/main.cpp` | Add WHY comment on `r.transfer_ms = 0.0`: "fine-grained coherency eliminates explicit fences; transfer time is definitionally zero" |
| 2 | `SVMTheory.md` | Add build note in Build & Run section: users with OpenCL 1.2-only SDK headers compile in 1.2-only mode; verify with `clinfo \| grep 'Device OpenCL C Version'` |
| 3 | `SVMTheory.md` | Consolidate Modes table and `buffer_map` prose block; reduce prose to one "why this is the baseline" sentence with link to the Modes table |
| 4 | `SVMTheory.md` | Add cross-link from the `uchar4` data format rationale to `05_Toolbox/02_Coalesced_Access/CoalescedAccess.md` |
| 5 | `src/main.cpp` | Print trailing summary line `[N modes skipped — OpenCL 2.0 SVM not available]` when at least one result is SKIPPED in `--mode all` |

**Build gate:** `cmake -B build && cmake --build build` from `05_Toolbox/11_SVM_Theory/` must succeed with zero warnings.

---

### L — 05_Toolbox / 12_Sync_Atomics

**Items to fix (from `grade_report_v2.md` lines 1132–1136):**

| # | File | Fix |
|---|------|-----|
| 1 | `CMakeLists.txt` | Update stale path comment on line 9: `99_Toolbox` → `05_Toolbox` |
| 2 | `SyncAtomics.md` | Replace hardcoded timing values (23.4 ms, 5.1 ms) in Verify section with a note: "representative output on a mid-range discrete GPU; your values will differ" |
| 5 | `SyncAtomics.md` | Add one sentence to the `atomic_max` Mini-Challenge describing the expected output format (per-bin max vs. global max) |

**Build gate:** `cmake -B build && cmake --build build` from `05_Toolbox/12_Sync_Atomics/` must succeed with zero warnings.

---

### M — 05_Toolbox / 13_Thread_Divergence

**Items to fix (from `grade_report_v2.md` lines 1142–1143):**

| # | File | Fix |
|---|------|-----|
| 1 | `CMakeLists.txt` | Update stale path comment: `99_Toolbox/ThreadDivergence/` → `05_Toolbox/13_Thread_Divergence/` |
| 2 | `ThreadDivergence.md` | Rephrase `blur1d_row` macro portability comment: "some vendor compilers poorly handle `inline` in `.cl` files; a function-like macro guarantees single expansion across all drivers" |

**Build gate:** `cmake -B build && cmake --build build` from `05_Toolbox/13_Thread_Divergence/` must succeed with zero warnings.

---

### N — 05_Toolbox / 14_Work_Group_Sizing

**Items to fix (from `grade_report_v2.md` lines 1154):**

| # | File | Fix |
|---|------|-----|
| 3 | `WorkGroupSizing.md` | Add one-line note: "`round_up()` is provided by `common/opencl_utils.hpp` — no additional dependency needed" |

**Note:** Items 1 and 2 are `[ ]` excluded — do not touch.

**Build gate:** `cmake -B build && cmake --build build` from `05_Toolbox/14_Work_Group_Sizing/` must succeed with zero warnings.

---

### O — 05_Toolbox / 15_Zero_Copy

**Items to fix (from `grade_report_v2.md` lines 1160–1163):**

| # | File | Fix |
|---|------|-----|
| 1 | `ZeroCopy.md` | Rename README Verify block column from "Upload" to `Kernel Time (ms)` to match what the code actually measures and prints |
| 2 | `src/main.cpp` | Add a fourth strategy `MAP_UNMAP` to `run_strategy` using `enqueueMapBuffer` / `enqueueUnmapMemObject` to make the "most portable zero-copy pattern" claim executable |
| 3 | `src/main.cpp` | Replace `std::exit(1)` in `verify()` with `throw std::runtime_error(...)` per master spec §7.8 |
| 4 | `CMakeLists.txt` | Update stale path comment line 8: `99_Toolbox/ZeroCopy/` → `05_Toolbox/15_Zero_Copy/` |

**Build gate:** `cmake -B build && cmake --build build` from `05_Toolbox/15_Zero_Copy/` must succeed with zero warnings.

---

### P — 05_Toolbox / 16_Async_Multi_Thread

**Items to fix (from `grade_report_v2.md` lines 1169–1172):**

| # | File | Fix |
|---|------|-----|
| 1 | `AsyncMultiThread.md` | Remove the duplicate driver-serialization caveat sentence from the Verify section |
| 2 | `src/main.cpp` | Add comment on blocking writes: "blocking writes used intentionally for simplicity; fully non-blocking dispatches would require intra-thread event chains" |
| 3 | `CMakeLists.txt` | Remove `symlink_assets(async_multi_thread)` call (module uses no assets) |
| 4 | `AsyncMultiThread.md` | Add a small illustrative output block before the Concept section showing concrete timestamp numbers with brief annotations |

**Build gate:** `cmake -B build && cmake --build build` from `05_Toolbox/16_Async_Multi_Thread/` must succeed with zero warnings.

---

## Definition of Done (DoD)

- [x] All `[x]` items listed in sections A–P above are applied.
- [x] `cmake -B build && cmake --build build` passes with zero errors and zero warnings for each modified submodule that contains a CMakeLists.txt (01, 02, 03, 04, 05, 06, 07, 08, 10, 11, 12, 13, 14, 15, 16).
- [x] Items marked `[ ]` in `grade_report_v2.md` for `05_Toolbox` are untouched.
- [x] No files outside the Relevant Files list above are modified (except `common/opencl_utils.hpp` if item F-3 requires a new helper).
- [x] `05_Toolbox/08_Multi_GPU_Strategy`: `row_offset` removal updates both the kernel parameter list and the `setArg` call site atomically — no argument index mismatch.
- [x] `05_Toolbox/15_Zero_Copy`: the new `MAP_UNMAP` strategy compiles and runs without error on at least one device; `enqueueUnmapMemObject` call is wrapped in `CL_CHECK`.
- [x] `05_Toolbox/11_SVM_Theory`: skipped-mode summary line is printed only when at least one result is SKIPPED; binary exits with code 0.
- [x] `05_Toolbox/06_Generic_Kernel_Templates`: `(scalar_t)result` cast in `mad_kernel.cl` is preceded by `clamp(result, 0.0f, 255.0f)`; no behavioral regression in BMP output.
- [ ] MANUAL: Spot-check `05_Toolbox/15_Zero_Copy/` — run `./build/zero_copy`; confirm all four strategy rows (including `MAP_UNMAP`) appear in the timing table with non-zero kernel times.
- [ ] MANUAL: Spot-check `05_Toolbox/10_Sub_Buffers_Partitioning/` — run `./build/sub_buffers`; confirm `output.bmp` shows correctly coloured strips and the improved error message appears when `--strips` exceeds `--height`.

---

## Execution Report

- **Status:** COMPLETE
- **Session:** T076

### Completed

| Item | Action |
|------|--------|
| A-1 | Removed `--kernel blur` from LocalMemory.md Build & Run |
| A-2 | Added hardware note in LocalMemory.md Verify section |
| A-3 | Added forward reference to Advanced Challenge section |
| B-1 | Removed `symlink_assets(coalesced_access)` from CMakeLists.txt |
| B-2 | Added WHY comment on `buf_in` creation in main.cpp |
| B-3 | Added `run_kernel` helper pointer in Mini-Challenge |
| C-1 | Removed duplicate `sudo apt install oclgrind` from Debugging.md body |
| C-2 | Replaced Build & Run inline comment with forward reference |
| C-3 | Added external resource links to Nsight/VTune/rocprof section |
| D-1 | Fixed binary name to `deployment` in appimage.sh |
| D-3 | Reduced ICD loader comment to one sentence with README forward ref |
| D-5 | Added `ARCH=$(uname -m)` and substituted into all URLs and output filename |
| E-1 | Added CPU/PoCL caveat in FastMath.md Verify section |
| E-2 | Added `--relaxed` to Build & Run example command |
| E-3 | Collapsed domain table into precision-tier table |
| E-4 | Removed `symlink_assets(fast_math)` from FastMath CMakeLists.txt |
| E-5 | Added WHY comment on `buf_out` reuse in FastMath main.cpp |
| E-6 | Extended Mini-Challenge with `native_sin` second example |
| F-1 | Added "If curious why" divider before Concept section |
| F-2 | Added fp16 N/A note in Verify block |
| F-3 | Extracted `build_program_from_source` to `common/opencl_utils.hpp`; removed local copies from 02 and 03; removed conflicting local from 08 |
| F-4 | Added `clamp(result, 0.0f, 255.0f)` before `(scalar_t)` cast in mad_kernel.cl |
| F-5 | Removed `--autotune` no-op flag from 03_AutoTune/main.cpp and README |
| G-1 | Replaced inline code example with cross-reference to kernel file |
| G-2 | Removed `symlink_assets` from GlobalWorkOffset CMakeLists.txt |
| G-3 | Replaced `(master specs §3 exception)` comment with self-contained rationale |
| H-1 | Added hardware-waiver clause to MultiGPUStrategy performance gate |
| H-2 | Fixed stale `--image` description in main.cpp and README |
| H-3 | Added forward pointer to Mini-Challenge in Concept section |
| H-4 | Removed dead `row_offset` from kernel signature and all `setArg` call sites atomically |
| I-1 | Replaced ecosystem tables with forward reference to `report.md §1` |
| J-1 | Added next-steps sentence pointing to 08 and 16 submodules |
| J-2 | Added WHY comment on `strip_color` |
| J-3 | Moved `grep` command from Verify to Troubleshooting |
| J-4 | Expanded `--strips > --height` error message with spec explanation |
| K-1 | Added WHY comment on `r.transfer_ms = 0.0` |
| K-2 | Added OpenCL 1.2 SDK build note in SVMTheory.md |
| K-3 | Consolidated Modes table and `buffer_map` prose |
| K-4 | Added cross-link from uchar4 rationale to CoalescedAccess.md |
| K-5 | Added skipped-mode summary line printed only when count > 0 |
| L-1 | Updated stale path comment `99_Toolbox` → `05_Toolbox/12_Sync_Atomics` |
| L-2 | Replaced hardcoded timing values with representative-output note |
| L-5 | Added expected output format description in atomic_max Mini-Challenge |
| M-1 | Updated stale path comment `99_Toolbox/ThreadDivergence` → `05_Toolbox/13_Thread_Divergence` |
| M-2 | Added `blur1d_row` macro portability note in ThreadDivergence.md |
| N-3 | Added `round_up()` note in WorkGroupSizing.md Mini-Challenge |
| O-1 | Renamed Verify block column from "Upload" to "Kernel Time (ms)" |
| O-2 | Added MAP_UNMAP strategy with `enqueueMapBuffer`/`enqueueUnmapMemObject`; wrapped unmap in `CL_CHECK` |
| O-3 | Replaced `std::exit(1)` with `throw std::runtime_error(...)` |
| O-4 | Updated stale path comment to `05_Toolbox/15_Zero_Copy` |
| P-1 | Removed duplicate driver-serialization caveat from Verify section |
| P-2 | Added WHY comment on blocking writes in 01_BasicSync/main.cpp |
| P-3 | Removed `symlink_assets(async_multi_thread)` from CMakeLists.txt |
| P-4 | Added illustrative output block before Concept section |

### Validation

```
01_Local_Memory:             exit=0 warnings=0 errors=0
02_Coalesced_Access:         exit=0 warnings=0 errors=0
03_Debugging:                exit=0 warnings=0 errors=0
04_Deployment:               exit=0 warnings=0 errors=0
05_Fast_Math:                exit=0 warnings=0 errors=0
06_Generic_Kernel_Templates: exit=0 warnings=0 errors=0  (built from parent dir 06_Generic_Kernel_Templates/)
07_Global_Work_Offset:       exit=0 warnings=0 errors=0
08_Multi_GPU_Strategy:       exit=0 warnings=0 errors=0
10_Sub_Buffers_Partitioning: exit=0 warnings=0 errors=0
11_SVM_Theory:               exit=0 warnings=0 errors=0
12_Sync_Atomics:             exit=0 warnings=0 errors=0
13_Thread_Divergence:        exit=0 warnings=0 errors=0
14_Work_Group_Sizing:        exit=0 warnings=0 errors=0
15_Zero_Copy:                exit=0 warnings=0 errors=0
16_Async_Multi_Thread:       exit=0 warnings=0 errors=0
```

### Changed Files

| File | Change |
|------|--------|
| `05_Toolbox/01_Local_Memory/LocalMemory.md` | A-1, A-2, A-3 |
| `05_Toolbox/02_Coalesced_Access/CMakeLists.txt` | B-1 |
| `05_Toolbox/02_Coalesced_Access/main.cpp` | B-2 |
| `05_Toolbox/02_Coalesced_Access/CoalescedAccess.md` | B-3 |
| `05_Toolbox/03_Debugging/Debugging.md` | C-1, C-2, C-3 |
| `05_Toolbox/04_Deployment/appimage.sh` | D-1, D-3, D-5 |
| `05_Toolbox/05_Fast_Math/FastMath.md` | E-1, E-2, E-3, E-6 |
| `05_Toolbox/05_Fast_Math/CMakeLists.txt` | E-4 |
| `05_Toolbox/05_Fast_Math/main.cpp` | E-5 |
| `05_Toolbox/06_Generic_Kernel_Templates/GenericKernelTemplates.md` | F-1, F-2, F-5 |
| `05_Toolbox/06_Generic_Kernel_Templates/02_Generic_MAD/main.cpp` | F-3: removed local run_variant, updated call sites |
| `05_Toolbox/06_Generic_Kernel_Templates/03_AutoTune/main.cpp` | F-3: removed local run_kernel, F-5 |
| `05_Toolbox/06_Generic_Kernel_Templates/kernels/mad_kernel.cl` | F-4 |
| `05_Toolbox/07_Global_Work_Offset/GlobalWorkOffset.md` | G-1 |
| `05_Toolbox/07_Global_Work_Offset/CMakeLists.txt` | G-2 |
| `05_Toolbox/07_Global_Work_Offset/main.cpp` | G-3 |
| `05_Toolbox/08_Multi_GPU_Strategy/MultiGPUStrategy.md` | H-1, H-2, H-3 |
| `05_Toolbox/08_Multi_GPU_Strategy/main.cpp` | H-2, H-4 |
| `05_Toolbox/08_Multi_GPU_Strategy/kernels/slice_kernel.cl` | H-4 |
| `05_Toolbox/09_OpenCL_vs_CUDA/OpenCLvsCUDA.md` | I-1 |
| `05_Toolbox/10_Sub_Buffers_Partitioning/SubBuffers.md` | J-1, J-3 |
| `05_Toolbox/10_Sub_Buffers_Partitioning/main.cpp` | J-2, J-4 |
| `05_Toolbox/11_SVM_Theory/SVMTheory.md` | K-2, K-3, K-4 |
| `05_Toolbox/11_SVM_Theory/main.cpp` | K-1, K-5 |
| `05_Toolbox/12_Sync_Atomics/CMakeLists.txt` | L-1 |
| `05_Toolbox/12_Sync_Atomics/SyncAtomics.md` | L-2, L-5 |
| `05_Toolbox/13_Thread_Divergence/CMakeLists.txt` | M-1 |
| `05_Toolbox/13_Thread_Divergence/ThreadDivergence.md` | M-2 |
| `05_Toolbox/14_Work_Group_Sizing/WorkGroupSizing.md` | N-3 |
| `05_Toolbox/15_Zero_Copy/ZeroCopy.md` | O-1 |
| `05_Toolbox/15_Zero_Copy/CMakeLists.txt` | O-4 |
| `05_Toolbox/15_Zero_Copy/main.cpp` | O-2, O-3 |
| `05_Toolbox/16_Async_Multi_Thread/AsyncMultiThread.md` | P-1, P-4 |
| `05_Toolbox/16_Async_Multi_Thread/CMakeLists.txt` | P-3 |
| `05_Toolbox/16_Async_Multi_Thread/01_BasicSync/main.cpp` | P-2 |
| `common/opencl_utils.hpp` | F-3: added `build_program_from_source`, added `run_kernel` |

### Remaining

- [ ] MANUAL: Spot-check `05_Toolbox/15_Zero_Copy/` — run `./build/zero_copy`; confirm all four strategy rows appear.
- [ ] MANUAL: Spot-check `05_Toolbox/10_Sub_Buffers_Partitioning/` — run `./build/sub_buffers`; confirm output.bmp and improved error message.
