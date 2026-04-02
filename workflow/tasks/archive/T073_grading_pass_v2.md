# Task T073: Grading Pass v2

## Context
- **Design Feature:** `workflow/design/D12_v2_2_improvements.md`
- **Milestone:** Phase 4 — Grading Pass *(Carried forward from D11 Phase 4)*
- **Relevant Files:**
  - `workflow/design/D12_v2_2_improvements.md` — (read-only: grading scope, protocol)
  - `workflow/design/00-executive-summary.md` — (read-only: `@evaluator` reference)
  - `workflow/design/00_master_specs.md` — (read-only: normative reference)
  - `workflow/tasks/grade_report_v2.md` — (append-only output artifact)

## Objective

Run `/grade-module` on every submodule across all 7 top-level modules (≈ 39 submodules), collect the strict evaluator output verbatim, and write the aggregate `grade_report_v2.md` report with a human-triage action table.

The work is split into **8 sub-steps** (1a–1h), each scoped to one session to keep token usage manageable. All sub-steps append to the same `grade_report_v2.md`. Step 2 (assembly) runs after all sub-steps are complete.

## Constraints & Rules

- **Output passthrough:** `@evaluator` uses a strict output template. Reproduce it verbatim — do NOT reformat, summarize, or paraphrase any scores table or justification block.
- **No auto-fixes in this phase.** Grading is read-only. No `.cpp`, `.cl`, `CMakeLists.txt`, or README files are modified. Implementation of approved items is covered by D12 Phase 5.
- **Phase 1 prerequisite satisfied:** `10_Sub_Buffers_Partitioning` is present and must be graded in Step 1f.
- **Parallel execution:** Run up to 3 `@evaluator` agents at a time within each sub-step.
- **OpenVINO modules:** `05_OpenVINO_GPU` and `06_Smart_Webcam` are buildable — grade all artifacts (code, CMake, README) normally.
- **Append mode:** Each sub-step appends its Raw Evaluator Output and Actionable Items sections to `grade_report_v2.md`. The Merged Scores table is assembled in Step 2.

---

## Implementation

### Step 1a — Grade: 00_Setup + 01_Host_API (4 submodules)

Run `@evaluator` (`/grade-module`) on the 4 submodules below in batches of up to 3.
Feed each agent the submodule README and all source files (`.cpp`, `.cl`, `CMakeLists.txt`) present.
Also provide `workflow/design/00-executive-summary.md` and `workflow/design/00_master_specs.md` as context.

| # | Module | Submodule directory |
|---|--------|---------------------|
| 1 | 00_Setup | `00_Setup/01_Smoke_Test/` |
| 2 | 01_Host_API | `01_Host_API/01_Visual_Kernel/` |
| 3 | 01_Host_API | `01_Host_API/02_Visual_Kernel_Events/` |
| 4 | 01_Host_API | `01_Host_API/03_Buffer_Flags/` |

Append results to `workflow/tasks/grade_report_v2.md` under `## Raw Evaluator Output` and `## Actionable Items`.

**Session checkpoint:** When all 4 are done, mark `[x] Step 1a complete` in the progress tracker below.

---

### Step 1b — Grade: 02_Multimedia (9 submodules)

| # | Module | Submodule directory |
|---|--------|---------------------|
| 5 | 02_Multimedia | `02_Multimedia/01_OpenCV_Interop/` |
| 6 | 02_Multimedia | `02_Multimedia/02_YUV_Pipeline/` |
| 7 | 02_Multimedia | `02_Multimedia/03_YUYV_Extension/` |
| 8 | 02_Multimedia | `02_Multimedia/04_OpenCV_DNN/` |
| 9 | 02_Multimedia | `02_Multimedia/05_OpenVINO_GPU/` |
| 10 | 02_Multimedia | `02_Multimedia/06_Smart_Webcam/` |
| 11 | 02_Multimedia | `02_Multimedia/07_Privacy_Mode/` |
| 12 | 02_Multimedia | `02_Multimedia/08_FFmpeg_Pipeline/` |
| 13 | 02_Multimedia | `02_Multimedia/09_SoftISP/` |

**Session checkpoint:** When all 9 are done, mark `[x] Step 1b complete`.

---

### Step 1c — Grade: 03_GraphicsHPC (3 submodules)

| # | Module | Submodule directory |
|---|--------|---------------------|
| 14 | 03_GraphicsHPC | `03_GraphicsHPC/01_Ray_Tracer_Basic/` |
| 15 | 03_GraphicsHPC | `03_GraphicsHPC/02_Ray_Tracer_BVH/` |
| 16 | 03_GraphicsHPC | `03_GraphicsHPC/03_Ray_Tracer_BVH_Dynamic/` |

**Session checkpoint:** When all 3 are done, mark `[x] Step 1c complete`.

---

### Step 1d — Grade: 04_Robotics (3 submodules)

| # | Module | Submodule directory |
|---|--------|---------------------|
| 17 | 04_Robotics | `04_Robotics/01_Node_Acceleration/` |
| 18 | 04_Robotics | `04_Robotics/02_Costmap_Inflation/` |
| 19 | 04_Robotics | `04_Robotics/03_Perception_Node/` |

**Session checkpoint:** When all 3 are done, mark `[x] Step 1d complete`.

---

### Step 1e — Grade: 05_Toolbox tools 01–05 (5 submodules)

| # | Module | Submodule directory |
|---|--------|---------------------|
| 20 | 05_Toolbox | `05_Toolbox/01_Local_Memory/` |
| 21 | 05_Toolbox | `05_Toolbox/02_Coalesced_Access/` |
| 22 | 05_Toolbox | `05_Toolbox/03_Debugging/` |
| 23 | 05_Toolbox | `05_Toolbox/04_Deployment/` |
| 24 | 05_Toolbox | `05_Toolbox/05_Fast_Math/` |

**Session checkpoint:** When all 5 are done, mark `[x] Step 1e complete`.

---

### Step 1f — Grade: 05_Toolbox tools 06–10 (5 submodules)

| # | Module | Submodule directory |
|---|--------|---------------------|
| 25 | 05_Toolbox | `05_Toolbox/06_Generic_Kernel_Templates/` |
| 26 | 05_Toolbox | `05_Toolbox/07_Global_Work_Offset/` |
| 27 | 05_Toolbox | `05_Toolbox/08_Multi_GPU_Strategy/` |
| 28 | 05_Toolbox | `05_Toolbox/09_OpenCL_vs_CUDA/` |
| 29 | 05_Toolbox | `05_Toolbox/10_Sub_Buffers_Partitioning/` |

**Session checkpoint:** When all 5 are done, mark `[x] Step 1f complete`.

---

### Step 1g — Grade: 05_Toolbox tools 11–16 (6 submodules)

| # | Module | Submodule directory |
|---|--------|---------------------|
| 30 | 05_Toolbox | `05_Toolbox/11_SVM_Theory/` |
| 31 | 05_Toolbox | `05_Toolbox/12_Sync_Atomics/` |
| 32 | 05_Toolbox | `05_Toolbox/13_Thread_Divergence/` |
| 33 | 05_Toolbox | `05_Toolbox/14_Work_Group_Sizing/` |
| 34 | 05_Toolbox | `05_Toolbox/15_Zero_Copy/` |
| 35 | 05_Toolbox | `05_Toolbox/16_Async_Multi_Thread/` |

**Session checkpoint:** When all 6 are done, mark `[x] Step 1g complete`.

---

### Step 1h — Grade: 06_Bonus (4 submodules)

| # | Module | Submodule directory |
|---|--------|---------------------|
| 36 | 06_Bonus | `06_Bonus/01_CLBlast_MatMul/` |
| 37 | 06_Bonus | `06_Bonus/02_Device_Enqueue/` |
| 38 | 06_Bonus | `06_Bonus/03_VkFFT_Audio/` |
| 39 | 06_Bonus | `06_Bonus/04_Voxel_Mapping/` |

**Session checkpoint:** When all 4 are done, mark `[x] Step 1h complete`.

---

### Step 2 — Assemble the aggregate report

*Run only after all 8 sub-steps (1a–1h) are complete.*

Write the final `workflow/tasks/grade_report_v2.md` using the exact structure below.
The `## Raw Evaluator Output` and `## Actionable Items` sections are already populated by Steps 1a–1e — only the `## Merged Scores` table needs to be assembled from the raw output.

```markdown
# Grading Report v2

## Merged Scores

| Module | Submodule | Theory/App | Uniqueness | HW Dep. | Repetitiveness | Clarity | Reproducibility | **FINAL SCORE** |
|--------|-----------|------------|------------|---------|----------------|---------|-----------------|-----------------|
| 00_Setup | 01_Smoke_Test | x/10 | x/10 | x/10 | x/10 | x/10 | x/10 | **x/10** |
| ... | | | | | | | | |

## Raw Evaluator Output

### [Module / Submodule]
<!-- Verbatim @evaluator output (scores table + justification). -->

## Actionable Items

### [Module / Submodule]
| # | Issue | Criterion | Severity | Proposed Fix | Fix? |
|---|-------|-----------|----------|--------------|------|
```

Severity derivation rule (from D12):
- Criterion score ≤ 5 → **HIGH**
- Criterion score 6–7 → **MED**
- Criterion score 8+ → LOW / skip

### Step 3 — Leave triage column blank for human

The `Fix?` column in the Actionable Items table must be left blank (`[ ]`) for every row. Do NOT pre-tick any items. Human triage happens after this task is complete.

---

## Progress Tracker

- [x] Step 1a complete — 00_Setup + 01_Host_API (4 submodules)
- [x] Step 1b complete — 02_Multimedia (9 submodules)
- [x] Step 1c complete — 03_GraphicsHPC (3 submodules)
- [x] Step 1d complete — 04_Robotics (3 submodules)
- [x] Step 1e complete — 05_Toolbox tools 01–05 (5 submodules)
- [x] Step 1f complete — 05_Toolbox tools 06–10 (5 submodules)
- [x] Step 1g complete — 05_Toolbox tools 11–16 (6 submodules)
- [x] Step 1h complete — 06_Bonus (4 submodules)
- [x] Step 2 complete — Merged Scores table assembled

---

## Definition of Done (DoD)

- [x] `workflow/tasks/grade_report_v2.md` exists and contains a Merged Scores row for every one of the 39 submodules listed in Steps 1a–1h.
- [x] Raw verbatim `@evaluator` output (scores table + justification) is present for each submodule under the `## Raw Evaluator Output` section — no reformatting or paraphrasing.
- [x] Actionable Items table is populated for every submodule with at least one criterion score ≤ 7.
- [x] `Fix?` column for every actionable item was blank (`[ ]`) prior to human triage — no pre-ticked items by the evaluator.
- [x] `05_OpenVINO_GPU` and `06_Smart_Webcam` are graded on all criteria (code, CMake, README) — no build-issue exemptions.
- [x] No `.cpp`, `.cl`, `CMakeLists.txt`, or `*.md` files outside `workflow/tasks/grade_report_v2.md` are modified.
- [x] MANUAL: Human reviews the Actionable Items table; ticks `[x]` in `Fix?` for items approved for implementation in D12 Phase 5.

---

## Execution Report

- **Status:** COMPLETE
- **Session:** 2026-03-30

### Validation
```
Merged Scores rows:   39 / 39 ✓
Raw evaluator ### sections: 117 (39 × 3) ✓
05_OpenVINO_GPU graded: ✓  (8.83/10)
06_Smart_Webcam graded: ✓  (8.50/10)
Files modified outside workflow/: none (during grading pass) ✓
Human triage: complete ✓
```

### Changed Files
| File | Change |
|------|--------|
| `workflow/tasks/grade_report_v2.md` | Created — aggregate grading output, human triage complete |
