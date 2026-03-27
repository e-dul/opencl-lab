# Task T069: Tech Audit v2

## Context
- **Design Feature:** `workflow/design/D11_v2_1_improvements.md`
- **Milestone:** Phase 3 — Tech Audit v2
- **Relevant Files:**
  - `workflow/design/D11_v2_1_improvements.md` — (read-only: protocol spec)
  - `workflow/design/00_master_specs.md` — (read-only: normative technical reference)
  - `workflow/design/00-executive-summary.md` — (read-only: project vision)
  - `workflow/tasks/tech_audit_v2_report.md` — (new file: findings report, written before any fixes)
  - All 7 module index READMEs and their submodule READMEs (v2.0 paths) — (to modify, step D only)

## Objective

Run `/audit` across all 7 top-level modules (and all submodule READMEs) in their v2.0 layout, produce an aggregate findings report for human triage, and apply only human-approved fixes to `.md` files.

## Constraints & Rules

- **Documentation only:** Never modify `.cpp`, `.cl`, or `CMakeLists.txt`.
- **Report before fixes:** `tech_audit_v2_report.md` must be written and fully populated before any edit to any README.
- **T050 flags first:** Before auditing new claims, grep for existing `AUDIT FLAG` markers across the repo and list them in the report's pre-existing flags section. Resolve or carry them forward explicitly.
- **Verdict vocabulary:** Use only `OK` / `UNVERIFIED` / `FALSE` / `STALE` in the Verdict column.
- **Unresolved claims:** Insert `> **AUDIT FLAG:** <reason>` blockquotes directly in the affected README for claims that cannot be verified or corrected.
- **ROS 2 runtime claims:** Cannot be web-searched definitively; flag as `MANUAL` in the triage table.
- **Vendor-specific performance numbers** (PCIe bandwidth, SVM speedups, etc.): mark `UNVERIFIED` unless a source URL is provided.
- **Strict output template passthrough:** Reproduce all `@auditor` output verbatim in the report — do not reformat or summarize.

---

## Implementation

### A — Pre-audit: grep existing AUDIT FLAGs

Search the entire repo for `AUDIT FLAG` markers, list each occurrence (file + line) in the report under a dedicated `## Pre-existing AUDIT FLAGs` section. For each: decide `RESOLVED`, `CARRY FORWARD`, or `NEEDS FIX`.

### B — Per-module `/audit` pass

Run `@auditor` on each module in order. For each module, cover:
1. Module index README.
2. Every submodule README inside that module directory.

For each submodule, web-search the top 3 factual technical claims. Flag stale, false, or unverifiable claims. Identify verbatim or near-verbatim redundancies between submodule READMEs and design docs.

Modules to cover (in order):
1. `00_Setup/`
2. `01_Host_API/` — 3 submodules
3. `02_Multimedia/` — 9 submodules
4. `03_GraphicsHPC/` — 3 submodules
5. `04_Robotics/` — 3 submodules (ROS 2 runtime claims → MANUAL)
6. `05_Toolbox/` — 14 active submodules (slot 10 vacant post-T067)
7. `06_Bonus/` — 4 submodules

### C — Write aggregate report

Write all findings to `workflow/tasks/tech_audit_v2_report.md` using the schema from D11 Phase 3 Detail:

```markdown
## [Module Name] / [Submodule Name]
### Factual Claims
| # | Claim | Location | Verdict | Proposed Fix | Fix? |
|---|-------|----------|---------|--------------|------|

### Redundancies
| # | Duplicated Content | Kept In | Remove From | Fix? |
|---|--------------------|---------|-------------|------|
```

All `Fix?` cells start as `[ ]`. Do not begin Step D until the human has triaged.

### D — MANUAL: Human triage then apply fixes

Wait for human to tick `[x]` in the `Fix?` column. Then:
- For each approved fix: edit the relevant `.md` file.
- For `FALSE`/`STALE` claims with no clean replacement: insert `> **AUDIT FLAG:** <reason>` blockquote.
- Remove approved redundant blocks.
- Do not touch `.cpp`, `.cl`, or `CMakeLists.txt` under any circumstances.

---

## Definition of Done (DoD)

- [x] `workflow/tasks/tech_audit_v2_report.md` exists and contains a `## Pre-existing AUDIT FLAGs` section listing all current markers.
- [x] Report covers all 7 modules and all submodule READMEs (no module skipped).
- [x] Every factual claim row has a non-empty Verdict (`OK` / `UNVERIFIED` / `FALSE` / `STALE`).
- [x] All `FALSE` and `STALE` items default to `[x]` in the `Fix?` column before human triage.
- [x] MANUAL: Human has triaged the report — ticked or unticked each `Fix?` cell — before Step D begins.
- [x] All approved fixes applied to `.md` files; zero `.cpp`/`.cl`/`CMakeLists.txt` files modified.
- [ ] Remaining unresolved claims have `> **AUDIT FLAG:**` blockquotes inserted in place.
- [ ] D11 Phase 3 checkbox updated to `[x]` in `workflow/design/D11_v2_1_improvements.md`.
- [ ] This task file moved to `workflow/tasks/archive/T069_tech_audit_v2.md`.

---

## Execution Report

- **Status:** STEP D COMPLETE — awaiting AUDIT FLAG insertion + sync
- **Session:** 2026-03-26

### Validation
```
All 13 approved fixes applied to .md files.
Zero .cpp / .cl / CMakeLists.txt files modified.
```

### Changed Files
| File | Change |
| ---- | ------ |
| `workflow/tasks/tech_audit_v2_report.md` | Created — aggregate findings report |
| `00_Setup/Setup.md` | Fixed "6th Gen Skylake" → "5th Gen Broadwell"; shortened ICD callout → link to Deployment.md |
| `01_Host_API/01_Visual_Kernel/VisualKernel.md` | Softened "95% of real workloads" → "most common GPU compute workloads" |
| `02_Multimedia/01_OpenCV_Interop/OpenCVInterop.md` | Fixed dead link `10_SVM/SVM.md` → `11_SVM_Theory/SVMTheory.md` |
| `03_GraphicsHPC/01_Ray_Tracer_Basic/RayTracerBasic.md` | Updated PCIe Gen 3 bandwidth: "~12 GB/s" → "~16 GB/s theoretical, ~12–14 GB/s measured" |
| `05_Toolbox/Toolbox.md` | Added missing `04_Deployment` row to contents table |
| `05_Toolbox/01_Local_Memory/LocalMemory.md` | Expanded local memory range note; added `clinfo` / `CL_DEVICE_LOCAL_MEM_SIZE` tip |
| `05_Toolbox/05_Fast_Math/FastMath.md` | Fixed `half_sqrt`: removed `cl_khr_fp16` gate; corrected precision "≥11-bit" → "≥10-bit (≤8192 ULP, §6.12.2)" |
| `05_Toolbox/11_SVM_Theory/SVMTheory.md` | Widened APU bandwidth: "~100 GB/s" → "85–140 GB/s depending on DDR5/LPDDR5x" |
| `06_Bonus/04_Voxel_Mapping/VoxelMapping.md` | Clarified "3 MB" Note: removed meta "Update this line" instruction; restated type-dependent sizes |

### Remaining

- [ ] Insert `> **AUDIT FLAG:**` blockquotes for unresolved UNVERIFIED items (if any approved by human)
- [ ] Update D11 Phase 3 checkbox
- [ ] Archive this task to `workflow/tasks/archive/`
