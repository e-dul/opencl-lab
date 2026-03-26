# D11: v2.1 Improvements Backlog

> **Purpose:** Cross-cutting improvement backlog for the Applied OpenCL Lab v2.1. Collects ideas before committing to tasks. Ideas graduate to Approved only after explicit sign-off.

## Goal

Eliminate remaining structural debt, module consolidation opportunities, and quality gaps not addressed in the v2.0 backlog (D10).

## Non-goals

- Renumbering existing module slots (gaps are acceptable per `00_master_specs.md` §2).
- Changes already completed under D10.

## Roadmap / Status

- [x] Phase 1: Merge 10_SVM into 11_SVM_Theory — Absorb `10_SVM`'s Buffer+Map/Unmap baseline mode and per-phase split timing into `11_SVM_Theory/main.cpp`; add `--mode` CLI flag; archive `10_SVM/`; update `Toolbox.md`. Slot 10 left vacant.

- [x] Phase 2: UX Audit v2 — Re-run `/test-ux` across all 7 modules in their v2.0 structure (`00_Setup`, `01_Host_API`, `02_Multimedia`, `03_GraphicsHPC`, `04_Robotics`, `05_Toolbox`, `06_Bonus`). Aggregate findings, human triage, apply selected fixes. Same protocol as Task 051 but scoped to the renamed v2.0 layout.
- [x] Phase 3: Tech Audit v2 — Re-run `/audit` across all 7 modules against their current v2.0 READMEs. Web-search top factual claims per submodule, flag stale/false claims, remove redundancies between READMEs and design docs. Report uses the same human-triage table format as Phase 2.
- [ ] Phase 4: Grading Pass — Run `/grade-module` on all 7 top-level modules (and key submodules). Produce a merged scores table plus per-module actionable items in the Phase 2/3 human-triage format.


---

## Phase 1 Detail: Merge 10_SVM → 11_SVM_Theory

### Architecture

**`11_SVM_Theory/main.cpp` (post-merge):** Single binary with 5 modes:

1. `buffer_map` — `CL_MEM_COPY_HOST_PTR` + explicit `enqueueMap/Unmap` (OpenCL 1.x baseline, absorbed from 10_SVM)
2. `use_host_ptr` — `CL_MEM_USE_HOST_PTR` (existing 11 mode)
3. `copy_host_ptr` — `CL_MEM_COPY_HOST_PTR` (existing 11 mode)
4. `coarse_svm` — Coarse-grained SVM (OpenCL 2.0+)
5. `fine_svm` — Fine-grained system SVM (OpenCL 2.0+)

- `--mode <name>` selects one mode; default runs all.
- Each mode reports split timing: `Transfer time` + `Kernel time` (absorbed from 10_SVM).
- BMP output via `save_bmp()` retained (satisfies §3 visual artifact requirement).

**`11_SVM_Theory/kernels/`:** Retain `passthrough.cl`; `scale_add.cl` from 10_SVM may be absorbed or dropped — `buffer_map` mode can reuse `passthrough.cl` with a minor semantic change accepted.

**`05_Toolbox/10_SVM/`:** Archived after merge. `Toolbox.md` entry removed; slot 10 left vacant.

### Key Decisions

1. **11_SVM_Theory is the base** — 10_SVM duplicates `CL_CHECK` and device-selection from `common/` (spec non-compliant). 11_SVM_Theory uses `common.cmake` + `opencl_lab_target()` and already has visual output.
2. **`--mode all` as default** — consistent with 10_SVM's UX; lets learners compare all paths in one run.
3. **Split timing: Transfer + Kernel** — absorbed from 10_SVM; educational payoff is showing *where* time is spent.

### Known Issues

- **`scale_add.cl` vs `passthrough.cl`:** Different kernels (`y[i] = a*x[i] + b` vs identity). `buffer_map` mode will reuse `passthrough.cl` for consistency.
- **CL 2.0 target version:** 10_SVM sets `CL_HPP_TARGET_OPENCL_VERSION 200` inline; merged binary sets it at CMake level (already done in 11_SVM_Theory).
- **Toolbox.md:** `SVM.md` back-link must be removed; `SVMTheory.md` back-link remains.
- **`fine_svm` graceful skip on OpenCL 1.2 hardware:** Expected behavior. When `CL_DEVICE_SVM_FINE_GRAIN_SYSTEM` is not supported, `fine_svm` mode prints a descriptive skip message and exits with code 0. Observed on Intel Iris Xe with 1.2-capable driver. Not a bug.

### Prerequisites

- `05_Toolbox/10_SVM/main.cpp`, `CMakeLists.txt`, `kernels/svm_kernel.cl`, `SVM.md`
- `05_Toolbox/11_SVM_Theory/main.cpp`, `CMakeLists.txt`, `kernels/passthrough.cl`, `SVMTheory.md`
- `05_Toolbox/Toolbox.md`

---

## Phase 2 Detail: UX Audit v2

### Scope

All 7 top-level modules in their v2.0 layout (post-D10 renames):

1. `00_Setup/`
2. `01_Host_API/`
3. `02_Multimedia/`
4. `03_GraphicsHPC/`
5. `04_Robotics/`
6. `05_Toolbox/`
7. `06_Bonus/`

### Protocol (mirrors Task 051)

**A — Per-module `/test-ux` pass:** Simulate a junior student walkthrough for each module index README and its submodule READMEs. Flag missing prerequisites, unexplained commands, broken links, stale paths, and steep cognitive jumps.

**B — Aggregate findings report:** Write all findings to a new `workflow/tasks/ux_audit_v2_report.md` before any fixes are applied. Structure:

```markdown
## [Module Name]
### Pedagogical Gaps
| # | Issue | Location | Severity | Proposed Fix | Fix? |
|---|-------|----------|----------|--------------|------|

### UX Friction Points
| # | Issue | Location | Severity | Proposed Fix | Fix? |
|---|-------|----------|----------|--------------|------|
```

**C — MANUAL: Human triage:** Human ticks `[x]` in the `Fix?` column for issues to resolve.

**D — Apply selected fixes:** Edit only README and documentation files. Never modify `.cpp`, `.cl`, or `CMakeLists.txt`.

### Difference from Task 051

| | Task 051 | Phase 2 |
| --- | --- | --- |
| Module layout | v1 paths (`A_Multimedia`, `B_Graphics_HPC`, `99_Toolbox`) | v2.0 paths (`02_Multimedia`, `03_GraphicsHPC`, `05_Toolbox`) |
| Prior fixes | None applied | T051 fixes already in place — look for regressions and new gaps introduced by D10 renames |
| Submodule READMEs | Partial coverage | Full coverage of all renamed submodule READMEs (`NN_Title_Snake_Case` dirs) |

### Known Risks

- **Link rot from D10 renames:** Phase 3 of D10 renamed 36 submodule directories; any hardcoded `cd` paths or cross-links in READMEs not caught by T060 may be stale.
- **New READMEs (Bonus):** `CLBlastMatMul.md` and `DeviceEnqueue.md` authored in T062 have not been audited by `/test-ux`.
- **ROS 2 modules:** `/test-ux` cannot execute `ros2` commands; flag these as MANUAL items for human verification.

### Known Issues (post-T068)

- **Stale `cd` paths (post-T060):** 36 submodule directory renames introduced by D10/T060 left stale `cd` commands and cross-links in several READMEs; the majority were resolved in T068. Any residual cases should be caught by Phase 3.
- **Title-numbering drift:** Several submodule README H1 titles used old prefixes (e.g., "B.1", "C.3") inconsistent with the `NN_Title_Snake_Case` naming; corrected in T068.
- **Vacant slot 10 broken link:** `README.md` root Toolbox table contained a stale row for the archived `10_SVM` entry; removed in T068.
- **MANUAL ROS 2 steps:** Four `ros2` walkthrough steps across `04_Robotics/` are flagged MANUAL and were not auto-verified; human spot-check required.
- **Scope of T068 fixes:** 31 `.md` files edited; 7 HIGH, 16 MED, 13 LOW findings resolved (36 total). Zero `.cpp`/`.cl`/`CMakeLists.txt` files modified.

---

## Phase 3 Detail: Tech Audit v2

### Audit Scope

All 7 top-level modules in their v2.0 layout, covering both module index READMEs and all submodule READMEs:

1. `00_Setup/`
2. `01_Host_API/`
3. `02_Multimedia/` (9 submodules)
4. `03_GraphicsHPC/` (3 submodules)
5. `04_Robotics/` (3 submodules)
6. `05_Toolbox/` (15 submodules)
7. `06_Bonus/` (4 submodules)

### Protocol (mirrors Task 050)

**A — Per-module `/audit` pass:** For each module, read the README + relevant design doc section, web-search the top 3 factual technical claims per submodule. Flag stale, false, or unverifiable claims. Identify verbatim/near-verbatim redundancies between READMEs and design docs.

**B — Aggregate findings report:** Write all findings to `workflow/tasks/tech_audit_v2_report.md` before applying any fixes. Use the same human-triage table format as Phase 2:

```markdown
## [Module Name] / [Submodule Name]
### Factual Claims
| # | Claim | Location | Verdict | Proposed Fix | Fix? |
|---|-------|----------|---------|--------------|------|

### Redundancies
| # | Duplicated Content | Kept In | Remove From | Fix? |
|---|--------------------|---------|-------------|------|
```

Verdict values: `OK` / `UNVERIFIED` / `FALSE` / `STALE`.

**C — MANUAL: Human triage:** Human ticks `[x]` in the `Fix?` column. `FALSE`/`STALE` items default to `[x]`; `UNVERIFIED` items are human judgement.

**D — Apply selected fixes:** Insert `> **AUDIT FLAG:**` blockquotes for unresolved claims. Remove redundant blocks. Edit only README and documentation files — never `.cpp`, `.cl`, or `CMakeLists.txt`.

### Difference from Task 050

| | Task 050 | Phase 3 |
| --- | --- | --- |
| Module layout | v1 paths (`A_Multimedia`, `99_Toolbox`, `04_Addons`) | v2.0 paths (`02_Multimedia`, `05_Toolbox`, `06_Bonus`) |
| Design docs audited | Per-module design docs (`01-host-api.md` etc.) | `D11_v2_1_improvements.md` + `00_master_specs.md` as normative reference |
| Submodule coverage | Index READMEs only | All submodule READMEs in scope |
| Prior AUDIT FLAGs | None | T050 flags may still be open — check and resolve stale ones first |

### Audit Risks

- **T050 unresolved flags:** Four `AUDIT FLAG` markers were inserted by T050 (OpenCLvsCUDA FPGA, Deployment rocm image, VoxelMapping 3 MB, Robotics rmw_cyclonedds). Grep for `AUDIT FLAG` before running — resolve or carry forward.
- **ROS 2 runtime claims:** Cannot be web-searched definitively; flag as MANUAL.
- **Vendor-specific performance numbers** (e.g., PCIe bandwidth, SVM speedups): mark UNVERIFIED unless sourced.

### Audit Prerequisites

- All 7 module index READMEs and their submodule READMEs (v2.0 paths).
- `workflow/design/00_master_specs.md` — normative reference for technical claims.
- Grep output of existing `AUDIT FLAG` markers across the repo before starting.

### Known Issues (post-T069)

- **`half_sqrt` gate incorrect (FALSE → fixed):** `05_Toolbox/05_Fast_Math/FastMath.md` incorrectly gated `half_sqrt` behind `cl_khr_fp16`; removed the gate. Precision note corrected from "≥11-bit" to "≥10-bit (≤8192 ULP, §6.12.2)".
- **Dead SVM cross-link (STALE → fixed):** `02_Multimedia/01_OpenCV_Interop/OpenCVInterop.md` referenced the archived `10_SVM/SVM.md`; updated to `11_SVM_Theory/SVMTheory.md` (residual from Phase 1 merge not caught by T068).
- **`04_Deployment` row missing from Toolbox (STALE → fixed):** `05_Toolbox/Toolbox.md` contents table omitted the `04_Deployment` submodule entry entirely; row added.
- **Setup.md CPU generation error (FALSE → fixed):** "6th Gen Skylake" was incorrect; corrected to "5th Gen Broadwell" per Intel ARK.
- **PCIe Gen 3 bandwidth understated (STALE → updated):** `03_GraphicsHPC/01_Ray_Tracer_Basic/RayTracerBasic.md` stated "~12 GB/s"; widened to "~16 GB/s theoretical, ~12–14 GB/s measured".
- **`tech_audit_v2_report.md` archived alongside task:** Report retained in `workflow/tasks/archive/` for traceability.

---

## Phase 4 Detail: Grading Pass

### Grading Scope

Run `/grade-module` on all 7 top-level modules. For modules with multiple distinct submodules, grade each submodule independently (the `@evaluator` reads the submodule README + source files):

1. `00_Setup/`
2. `01_Host_API/` — grade each of the 3 submodules
3. `02_Multimedia/` — grade each of the 9 submodules
4. `03_GraphicsHPC/` — grade each of the 3 submodules
5. `04_Robotics/` — grade each of the 3 submodules
6. `05_Toolbox/` — grade each submodule (15 slots, slot 10 vacant post-Phase 1)
7. `06_Bonus/` — grade each of the 4 submodules

### Protocol

**A — Per-submodule `/grade-module` pass:** Run `@evaluator` on each submodule directory using the `grading` skill. Collect the strict output template (scores table + justification + actionable items) verbatim for each submodule.

**B — Aggregate report:** Write all findings to `workflow/tasks/grade_report_v2.md`. Structure:

```markdown
# Grading Report v2

## Merged Scores

| Module | Submodule | Theory/App | Uniqueness | HW Dep. | Repetitiveness | Clarity | Reproducibility | **FINAL SCORE** |
|--------|-----------|------------|------------|---------|----------------|---------|-----------------|-----------------|
| 00_Setup | — | x/10 | x/10 | x/10 | x/10 | x/10 | x/10 | **x/10** |
| 01_Host_API | 01_Visual_Kernel | ... | | | | | | |
| ... | | | | | | | | |

## Actionable Items

### [Module / Submodule]
| # | Issue | Criterion | Severity | Proposed Fix | Fix? |
|---|-------|-----------|----------|--------------|------|
```

Severity derived from score gap: criterion score ≤ 5 → HIGH, 6–7 → MED, 8+ → LOW/skip.

**C — MANUAL: Human triage:** Human ticks `[x]` in the `Fix?` column. Items typically feed back into Phase 2 (README fixes) or future design backlog entries.

**D — No auto-fixes in this phase.** Grading is read-only. Selected action items are graduated to tasks or appended to Phase 2/3 fix lists.

### Grading Criteria Reference

Per `grading` skill (`SKILL.md`):

| Criterion | What it measures |
| --- | --- |
| Theory/App | Just-in-Time theory placement vs. dry theory dump |
| Uniqueness | Value vs. generic online OpenCL tutorials |
| HW Dep. | Portability; graceful OpenCL 1.2 fallback |
| Repetitiveness | README conciseness; no code-comment parroting |
| Clarity | WHY-focused comments; architectural reasoning |
| Reproducibility | Self-contained CMake build; no environment hell |

### Grading Risks

- **Strict output template must be reproduced verbatim** — do not reformat or summarize evaluator output. Append raw table + justification for each submodule, then extract actionable items into the triage table.
- **Phase 1 dependency:** If Phase 1 is complete before this phase runs, grade `11_SVM_Theory` only (slot 10 vacant). If Phase 1 is not yet complete, grade both `10_SVM` and `11_SVM_Theory` separately.
- **Large scope:** ~40 submodules. Run `@evaluator` agents in parallel (up to 3 at a time) to keep wall-clock time reasonable.

### Grading Prerequisites

- All submodule READMEs and source files in v2.0 paths.
- `workflow/design/00-executive-summary.md` — `@evaluator` reads this for project vision.
- `workflow/design/00_master_specs.md` — normative reference for design decisions and known limitations.

---

## Specifications

> **Inherits**: `workflow/design/00_master_specs.md`

Phases in this backlog must not break the Standard Definition of Done (§8) for any affected module. All CMake changes must preserve standalone buildability (§1).

## Performance Gate

N/A — this is a maintenance backlog, not a module with performance targets.
