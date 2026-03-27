# Task T067: Merge 10_SVM into 11_SVM_Theory

## Context
- **Design Feature:** `workflow/design/D11_v2_1_improvements.md`
- **Milestone:** Phase 1 — Merge 10_SVM into 11_SVM_Theory
- **Relevant Files:**
  - `05_Toolbox/10_SVM/main.cpp` — (read-only: source for `buffer_map` mode logic and split timing)
  - `05_Toolbox/10_SVM/CMakeLists.txt` — (read-only: reference)
  - `05_Toolbox/10_SVM/kernels/svm_kernel.cl` — (read-only: reference; mode will reuse `passthrough.cl`)
  - `05_Toolbox/10_SVM/SVM.md` — (to archive)
  - `05_Toolbox/11_SVM_Theory/main.cpp` — (to modify: add `buffer_map` mode + `--mode` flag)
  - `05_Toolbox/11_SVM_Theory/CMakeLists.txt` — (read-only: verify no changes needed)
  - `05_Toolbox/11_SVM_Theory/kernels/passthrough.cl` — (read-only: reused by `buffer_map` mode)
  - `05_Toolbox/11_SVM_Theory/SVMTheory.md` — (to modify: document new `buffer_map` mode and `--mode` flag)
  - `05_Toolbox/Toolbox.md` — (to modify: remove slot-10 row, leave slot vacant)

## Objective
Absorb `10_SVM`'s `CL_MEM_COPY_HOST_PTR` + map/unmap baseline mode and per-phase split timing into `11_SVM_Theory/main.cpp` under a `--mode buffer_map` flag, then archive the `10_SVM/` directory and update `Toolbox.md`.

## Constraints & Rules
- Standard constraints from `00_master_specs.md` apply (C++17, cl.hpp, CLI11, `opencl_lab_target()`, standalone CMake, kernel symlink rule).
- `buffer_map` mode MUST reuse `kernels/passthrough.cl` — do NOT copy or reference `svm_kernel.cl` (`scale_add` semantics are not required).
- `--mode` CLI11 flag: accepted values `buffer_map`, `use_host_ptr`, `copy_host_ptr`, `coarse_svm`, `fine_svm`, `all`. Default: `all`.
- Each mode MUST report split timing: `Transfer time` (ms, 3 d.p.) and `Kernel time` (ms, 3 d.p.) — GPU stages via `cl::Event`, CPU stages via `std::chrono::steady_clock`.
- BMP output via `save_bmp()` MUST be retained (§3 visual artifact requirement).
- OpenCL 2.0 graceful fallback MUST be preserved: if `coarse_svm` or `fine_svm` mode is requested on a 1.2 device, print a descriptive message and exit with code 0.
- `10_SVM/` directory is archived by moving it to `05_Toolbox/archive/10_SVM/` (create `archive/` if absent).
- Slot 10 in `Toolbox.md` is left vacant (no renumbering).
- Do NOT modify any other `.cpp`, `.cl`, or `CMakeLists.txt` files outside `11_SVM_Theory/`.

---

## Implementation

### A — Read and extract `buffer_map` mode from 10_SVM

**Problem:** `10_SVM/main.cpp` contains the only implementation of the `CL_MEM_COPY_HOST_PTR` + explicit `enqueueMap/Unmap` flow and per-phase split timing. That educational content is lost when the directory is archived.

**Decision:** Port the `buffer_map` logic into `11_SVM_Theory/main.cpp` as one case in the mode dispatch. Reuse `passthrough.cl` for the kernel; the semantic difference (identity vs. scale-add) is acceptable per the design doc.

**Action:**
1. Read `05_Toolbox/10_SVM/main.cpp` in full to extract: (a) the map/unmap sequence, (b) the `cl::Event`-based split timing pattern.
2. Read `05_Toolbox/11_SVM_Theory/main.cpp` in full to understand the existing mode structure.
3. Add `--mode` CLI11 option (allowed values listed above, default `all`).
4. Refactor existing modes (`use_host_ptr`, `copy_host_ptr`, `coarse_svm`, `fine_svm`) into named functions or a dispatch table.
5. Implement `run_buffer_map()` using `CL_MEM_COPY_HOST_PTR` + `enqueueMapBuffer` / `enqueueUnmapMemObject`; wrap `enqueueUnmapMemObject` in `CL_CHECK`.
6. Add split timing to every mode that lacks it (Transfer time + Kernel time).
7. When `--mode all`, run all 5 modes in sequence and print a summary table.

---

### B — Update `SVMTheory.md`

**Problem:** The README only documents the original 4 modes. The new `buffer_map` mode and `--mode` flag are undocumented.

**Decision:** Add a section describing `buffer_map` (what it is, why it matters as a 1.x baseline) and update the usage example to show `--mode`.

**Action:**
1. Add `buffer_map` row to the mode comparison table (or create one if absent).
2. Update `## Usage` / `## Running` section to show: `./build/svm_theory --mode buffer_map`, `./build/svm_theory --mode all`.
3. Preserve all existing content — this is an additive edit.

---

### C — Archive `10_SVM/` and update `Toolbox.md`

**Problem:** `10_SVM/` will be a dead directory after the merge. Its `Toolbox.md` row must be removed to avoid broken navigation.

**Decision:** Move `10_SVM/` to `05_Toolbox/archive/10_SVM/`. Remove the slot-10 row from `Toolbox.md`. Leave the slot number gap (no renumbering, per §2 naming conventions).

**Action:**
1. Create `05_Toolbox/archive/` if it does not exist.
2. Move `05_Toolbox/10_SVM/` → `05_Toolbox/archive/10_SVM/`.
3. In `Toolbox.md`: remove the `| [SVM](10_SVM/SVM.md) | ... | 10_SVM/ |` row. Do not renumber any other rows.

---

## Definition of Done (DoD)

Standard items from `00_master_specs.md` §8 apply to `11_SVM_Theory`:

- [x] `cmake -B build && cmake --build build` succeeds inside `05_Toolbox/11_SVM_Theory/` with zero errors and zero warnings.
- [x] `./build/svm_theory` (no args) runs all 5 modes, prints split timing for each, and exits with code 0 (CL 2.0 modes may print a graceful-fallback message on 1.2 hardware).
- [x] `./build/svm_theory --help` lists `--mode` with all 6 accepted values.
- [x] `GPU=<vendor> ./build/svm_theory` selects the correct device without crashing.
- [x] `./build/svm_theory --mode buffer_map` runs the map/unmap mode, prints `Transfer time` and `Kernel time`, and writes `output.bmp`.
- [x] `./build/svm_theory --mode use_host_ptr` and `--mode copy_host_ptr` each print split timing and write `output.bmp`.
- [x] Console output includes a timing row for each executed mode in the format: `[mode_name]  Transfer: X.XXX ms  Kernel: Y.YYY ms`.
- [x] `05_Toolbox/10_SVM/` no longer exists at its original path; it is present under `05_Toolbox/archive/10_SVM/`.
- [x] `05_Toolbox/Toolbox.md` no longer contains a link to `10_SVM/SVM.md`; slot 11 row is intact.
- [x] `05_Toolbox/11_SVM_Theory/SVMTheory.md` documents the `buffer_map` mode and `--mode` flag.
- [ ] MANUAL: Run `./build/svm_theory --mode all`; confirm all 5 mode results print, BMP is written, and timing table is readable.

---

## Execution Report

- **Status:** VALIDATED (agent-verifiable items)
- **Session:** 2026-03-25

### Completed
| Item | Action |
|------|--------|
| A — buffer_map port | Implemented — `run_buffer_map()` with `CL_MEM_COPY_HOST_PTR` + map/unmap, split timing |
| B — SVMTheory.md update | `buffer_map` row in table, `--mode` usage examples, dedicated section added |
| C — Archive + Toolbox.md | `10_SVM/` moved to `archive/10_SVM/`; slot-10 row removed from `Toolbox.md` |

### Validation
```
$ cmake -B build && cmake --build build
-- Configuring done (0.9s)
-- Generating done (0.0s)
-- Build files have been written to: .../11_SVM_Theory/build
[  0%] Built target CLI11
[100%] Built target svm_theory

$ ./build/svm_theory
Device: Intel(R) Iris(R) Xe Graphics

[buffer_map    ]  Transfer: 0.011 ms  Kernel: 0.345 ms
[copy_host_ptr ]  Transfer: 0.261 ms  Kernel: 0.406 ms
[use_host_ptr  ]  Transfer: 0.336 ms  Kernel: 0.404 ms
[coarse_svm    ]  Transfer: 0.002 ms  Kernel: 0.365 ms
[fine_svm      ]  SKIPPED — CL_DEVICE_SVM_FINE_GRAIN_SYSTEM not supported
EXIT: 0

$ ./build/svm_theory --help
SVM Theory — OpenCL memory-transfer path benchmark (5 modes)
Usage: ./build/svm_theory [OPTIONS]

Options:
  -h,--help                   Print this help message and exit
  --width INT [1920]          Image width in pixels
  --height INT [1080]         Image height in pixels
  --mode TEXT:{buffer_map,use_host_ptr,copy_host_ptr,coarse_svm,fine_svm,all} [all]
                              Transfer mode to benchmark

$ GPU=INTEL ./build/svm_theory --mode buffer_map
Device: Intel(R) Iris(R) Xe Graphics
[buffer_map    ]  Transfer: 0.012 ms  Kernel: 0.362 ms
EXIT: 0

$ ./build/svm_theory --mode buffer_map
[buffer_map    ]  Transfer: 0.013 ms  Kernel: 0.370 ms
output.bmp: 8294522 bytes

$ ./build/svm_theory --mode use_host_ptr
[use_host_ptr  ]  Transfer: 0.268 ms  Kernel: 0.432 ms
output.bmp written

$ ./build/svm_theory --mode copy_host_ptr
[copy_host_ptr ]  Transfer: 0.308 ms  Kernel: 0.367 ms
output.bmp written

$ ls 05_Toolbox/archive/10_SVM/
(present — archived successfully)

$ grep "10_SVM/SVM.md" 05_Toolbox/Toolbox.md
(no matches — row removed)

$ grep "11_SVM_Theory" 05_Toolbox/Toolbox.md
| [SVM Theory](11_SVM_Theory/SVMTheory.md) | ... | 11_SVM_Theory/ |

$ grep "buffer_map\|--mode" 05_Toolbox/11_SVM_Theory/SVMTheory.md
(multiple matches — documented)
```

### Changed Files
| File | Change |
|------|--------|
| `05_Toolbox/11_SVM_Theory/main.cpp` | Modified — added `--mode` flag and `buffer_map` mode |
| `05_Toolbox/11_SVM_Theory/SVMTheory.md` | Modified — documented new mode and flag |
| `05_Toolbox/Toolbox.md` | Modified — removed slot-10 row |
| `05_Toolbox/archive/10_SVM/` | Created — archived from `05_Toolbox/10_SVM/` |

### Remaining
- [ ] MANUAL verification pending
