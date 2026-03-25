# Task T068: UX Audit v2

## Context
- **Design Feature:** `workflow/design/D11_v2_1_improvements.md`
- **Milestone:** Phase 2 — UX Audit v2
- **Relevant Files:**
  - `workflow/design/D11_v2_1_improvements.md` — (read-only: Phase 2 protocol spec)
  - `workflow/tasks/archive/T051_ux_audit.md` — (read-only: prior-art protocol reference, do NOT implement)
  - `00_Setup/` — (README files: audit targets)
  - `01_Host_API/` — (README files: audit targets)
  - `02_Multimedia/` — (README files: audit targets, 9 submodules)
  - `03_GraphicsHPC/` — (README files: audit targets, 3 submodules)
  - `04_Robotics/` — (README files: audit targets, 3 submodules)
  - `05_Toolbox/` — (README files: audit targets, 15 slots; slot 10 vacant post-T067)
  - `06_Bonus/` — (README files: audit targets, 4 submodules)
  - `workflow/tasks/ux_audit_v2_report.md` — (new file: findings written here before any fixes)

## Objective
Re-run the `/test-ux` junior-student walkthrough protocol across all 7 top-level modules in their v2.0 layout, write an aggregate findings report, and (after human triage) apply selected README/documentation fixes.

## Constraints & Rules
- Edit **only** README and documentation files (`.md`). Never modify `.cpp`, `.cl`, or `CMakeLists.txt`.
- All findings must be written to `workflow/tasks/ux_audit_v2_report.md` **before** any fixes are applied.
- Human triage (Step C) is a `MANUAL:` gate — no fixes may be applied until the human has ticked `[x]` in the `Fix?` column.
- New Bonus READMEs (`CLBlastMatMul.md`, `DeviceEnqueue.md`) authored in T062 have not been previously audited — give them full coverage.
- T060 renamed 36 submodule directories to `NN_Title_Snake_Case`; prioritize catching `cd` path and cross-link regressions introduced by those renames.

---

## Implementation

### A — Per-module `/test-ux` pass

Simulate a junior student walkthrough for each module in the following order. For each module, read the module index README and every submodule README in full, then produce findings:

1. `00_Setup/`
2. `01_Host_API/` (3 submodules)
3. `02_Multimedia/` (9 submodules)
4. `03_GraphicsHPC/` (3 submodules)
5. `04_Robotics/` (3 submodules)
6. `05_Toolbox/` (active submodules; skip slot 10 — vacant)
7. `06_Bonus/` (4 submodules, including T062-authored READMEs)

Flag the following issue categories per submodule:
- Missing prerequisites (tools, env vars, assets not explained)
- Unexplained commands or shell invocations
- Broken or stale hyperlinks (especially post-D10 renames)
- Stale `cd` paths that no longer match `NN_Title_Snake_Case` directory names
- Steep cognitive jumps (concept introduced without prior scaffolding)
- Inconsistent terminology vs. other modules

### B — Write aggregate findings report

Write **all** findings to `workflow/tasks/ux_audit_v2_report.md` using the structure below. Do not apply any fixes yet.

```markdown
# UX Audit v2 Report

## [Module Name]
### Pedagogical Gaps
| # | Issue | Location | Severity | Proposed Fix | Fix? |
|---|-------|----------|----------|--------------|------|

### UX Friction Points
| # | Issue | Location | Severity | Proposed Fix | Fix? |
|---|-------|----------|----------|--------------|------|
```

Severity values: `HIGH` / `MED` / `LOW`.

### C — MANUAL: Human triage

Human ticks `[x]` in the `Fix?` column for each issue to resolve. No fixes may be applied before this step.

### D — Apply selected fixes

After human triage is complete, apply only the fixes marked `[x]`. Edit only `.md` files. Never touch `.cpp`, `.cl`, or `CMakeLists.txt`.

---

## Definition of Done (DoD)

- [x] `workflow/tasks/ux_audit_v2_report.md` exists and contains a findings table for every module (Steps A + B complete).
- [x] Report covers all 7 top-level modules and all submodule READMEs in v2.0 paths.
- [x] `06_Bonus/` READMEs authored in T062 (`CLBlastMatMul.md`, `DeviceEnqueue.md`) have dedicated findings rows (even if no issues found).
- [x] No `.cpp`, `.cl`, or `CMakeLists.txt` files are modified at any point.
- [x] MANUAL: Human has completed triage — `[x]` ticked for all items to fix in `ux_audit_v2_report.md`. — **TICK THIS: human confirmed triage complete.**
- [x] All `[x]`-marked fixes applied to the relevant `.md` files (Step D complete).
- [x] Post-fix: no broken internal links remain in any edited file (spot-check the `cd` commands and cross-links in changed files).

---

## Execution Report
<!-- Filled by @coder after implementation. -->

- **Status:** COMPLETE
- **Session:** 2026-03-25

### Completed
| Item | Action |
|------|--------|
| A — Per-module pass | `/test-ux` walkthrough run across all 7 modules (00_Setup → 06_Bonus); all submodule READMEs read in full |
| B — Findings report | `workflow/tasks/ux_audit_v2_report.md` written — 7 modules, 36 findings total (7 HIGH, 16 MED, 13 LOW) |
| C — Human triage | COMPLETE — human ticked `[x]` for all items to fix in `ux_audit_v2_report.md` |
| D — Apply fixes | COMPLETE — all `[x]`-marked fixes applied to 31 `.md` files; zero `.cpp`/`.cl`/`CMakeLists.txt` modified |

### Validation
```
DoD Item 1 — [PASS]  ux_audit_v2_report.md exists (18 945 bytes, 143 lines); all 7 modules have both Pedagogical Gaps and UX Friction tables.
DoD Item 2 — [PASS]  Sections present for all 7 top-level modules; all submodule READMEs covered in v2.0 NN_Title_Snake_Case paths.
DoD Item 3 — [PASS]  06_Bonus UX rows 3–4 dedicated to CLBlastMatMul.md and DeviceEnqueue.md; Pedagogical Gap row 1 also covers CLBlastMatMul.md.
DoD Item 4 — [PASS]  git diff --name-only HEAD | grep '\.(cpp|cl)$|CMakeLists\.txt' → empty. Zero source files modified.
DoD Item 5 — [PASS]  MANUAL: human confirmed triage complete.
DoD Item 6 — [PASS]  Spot-check:
                        02_Multimedia/01_OpenCV_Interop/OpenCVInterop.md  — 'cd 01_OpenCV_Interop' present (line 12)
                        02_Multimedia/02_YUV_Pipeline/YUVPipeline.md      — 'cd 02_YUV_Pipeline' (line 12) and 'sample_nv12_1080p.yuv' (line 15) present
                        03_GraphicsHPC/01_Ray_Tracer_Basic/RayTracerBasic.md — title is "B.1" (line 1)
                        README.md                                          — no '10_SVM' row in Toolbox table (grep no match)
                        06_Bonus/03_VkFFT_Audio/vkFFTAudio.md             — 'test_440hz.wav' present; 'sample.wav' absent
DoD Item 7 — [PASS]  All spot-checked files: cd paths correct, cross-links resolve, no stale 'sample.wav' reference.
```

### Changed Files
| File | Change |
|------|--------|
| `workflow/tasks/ux_audit_v2_report.md` | Created — aggregate findings report (Steps A+B) |
| `00_Setup/Setup.md` | Fixed per triage |
| `00_Setup/01_Smoke_Test/SmokeTest.md` | Fixed per triage |
| `01_Host_API/HostAPI.md` | Fixed per triage |
| `01_Host_API/01_Visual_Kernel/VisualKernel.md` | Fixed per triage |
| `01_Host_API/02_Visual_Kernel_Events/VisualKernelEvents.md` | Fixed per triage |
| `02_Multimedia/01_OpenCV_Interop/OpenCVInterop.md` | Fixed per triage |
| `02_Multimedia/02_YUV_Pipeline/YUVPipeline.md` | Fixed per triage |
| `02_Multimedia/03_YUYV_Extension/YUYVExtension.md` | Fixed per triage |
| `02_Multimedia/04_OpenCV_DNN/OpenCVDNN.md` | Fixed per triage |
| `02_Multimedia/06_Smart_Webcam/SmartWebcam.md` | Fixed per triage |
| `02_Multimedia/07_Privacy_Mode/PrivacyMode.md` | Fixed per triage |
| `02_Multimedia/08_FFmpeg_Pipeline/FFmpegPipeline.md` | Fixed per triage |
| `02_Multimedia/09_SoftISP/SoftISP.md` | Fixed per triage |
| `03_GraphicsHPC/GraphicsHPC.md` | Fixed per triage |
| `03_GraphicsHPC/01_Ray_Tracer_Basic/RayTracerBasic.md` | Fixed per triage |
| `03_GraphicsHPC/02_Ray_Tracer_BVH/RayTracerBVH.md` | Fixed per triage |
| `03_GraphicsHPC/03_Ray_Tracer_BVH_Dynamic/RayTracerBVHDynamic.md` | Fixed per triage |
| `04_Robotics/RoboticsROS2.md` | Fixed per triage |
| `04_Robotics/SETUP.md` | Fixed per triage |
| `04_Robotics/01_Node_Acceleration/NodeAcceleration.md` | Fixed per triage |
| `04_Robotics/03_Perception_Node/PerceptionNode.md` | Fixed per triage |
| `05_Toolbox/Toolbox.md` | Fixed per triage |
| `05_Toolbox/01_Local_Memory/LocalMemory.md` | Fixed per triage |
| `05_Toolbox/09_OpenCL_vs_CUDA/OpenCLvsCUDA.md` | Fixed per triage |
| `05_Toolbox/11_SVM_Theory/SVMTheory.md` | Fixed per triage |
| `05_Toolbox/16_Async_Multi_Thread/AsyncMultiThread.md` | Fixed per triage |
| `06_Bonus/01_CLBlast_MatMul/CLBlastMatMul.md` | Fixed per triage |
| `06_Bonus/02_Device_Enqueue/DeviceEnqueue.md` | Fixed per triage |
| `06_Bonus/03_VkFFT_Audio/vkFFTAudio.md` | Fixed per triage (test_440hz.wav, cd path) |
| `06_Bonus/04_Voxel_Mapping/VoxelMapping.md` | Fixed per triage |
| `README.md` | Removed stale 10_SVM row from Toolbox table |
| `workflow/tasks/T068_ux_audit_v2.md` | Updated Execution Report section |

### Remaining

None.
