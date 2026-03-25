# Applied OpenCL Lab – Memory Log

## Design Decisions (Long-Term)
- **Build System:** CMake 3.18+, hybrid approach (Main + Standalone).
- **OpenCL Wrapper:** cl.hpp version 1.2 (for Nvidia compatibility).
- **C++ Standard:** C++17.
- **GPU Selection:** `GPU` env var (vendor substring, case-insensitive). Implemented in `common/ocl_wrapper.hpp::create_context()`. Matches `CL_PLATFORM_VENDOR` or `CL_DEVICE_VENDOR`. Examples: `GPU=NVIDIA`, `GPU=AMD`, `GPU=INTEL`. Default: first GPU found, CPU fallback. Hard-coded device indices are FORBIDDEN.

## Progress Tracking
**Implementation: 39/40 sub-modules → 98%** (2 optional OpenVINO modules require Intel SDK)
**D09 Design phases: 6/15 → 40%** (run `scripts/progress.sh`)

v2.0 Directory Layout (all modules implemented and buildable):
- [x] Module 0: Setup (`00_Setup/`) — 1 sub-module
- [x] Module 1: Host API (`01_Host_API/`) — 3 sub-modules
- [x] Module 2: Multimedia & AI (`02_Multimedia/`) — 9 sub-modules (`01_OpenCV_Interop` … `09_SoftISP`)
- [x] Module 3: Graphics & HPC (`03_GraphicsHPC/`) — 3 sub-modules (`01_Ray_Tracer_Basic` … `03_Ray_Tracer_BVH_Dynamic`)
- [x] Module 4: Robotics & ROS 2 (`04_Robotics/`) — 3 sub-modules (`01_Node_Acceleration` … `03_Perception_Node`)
- [x] Module 5: Toolbox (`05_Toolbox/`) — 17 sub-modules (`01_Local_Memory` … `16_Async_Multi_Thread`)
- [x] Module 6: Bonus (`06_Bonus/`) — 4 sub-modules (`01_CLBlast_MatMul` … `04_Voxel_Mapping`)

D09 Cookbook v2.0 Pivot phases (tracked by `scripts/progress.sh`):
- [x] Phase 1: Folder Restructuring
- [x] Phase 2: Content Migration
- [x] Phase 3: README Unification
- [x] Phase 4: CMake POST_BUILD Assets Symlink
- [x] Phase 5: New Toolbox Entry (`GlobalWorkOffset`)
- [ ] Phase 7: Final Verification (in progress — T057)

## Style Guide (Coding Conventions)
- Naming: `snake_case` for variables, `PascalCase` for classes.
- Buffers: Always RAII (`cl::Buffer`).
- Comments: Explain "WHY", not "WHAT".

## Design Pipeline (Exec Summary → Task)
```
/create-readme  →  /create-design  →  /plan-tasks
                      (revise loop)
```
- `/create-readme` (@educator): reads exec summary / user intent → generates `<module>/README.md` (student-facing, educational goals). Output: README or revision request.
- `/create-design` (@architect): reads approved README → generates `workflow/design/<module>.md` (technical architecture). Output: design doc or revision request.
- `/plan-tasks` (@architect): reads approved design doc → writes next atomic task file to `workflow/tasks/`. Output: task file path.
- Revision loop: if design contradicts README intent, re-run `/create-design` with correction notes. Do NOT run `/plan-tasks` with an unapproved design.

## Implementation Pipeline (Task → Product)
```
/implement  →  /review  →  /implement  →  /validate  →  /implement  →  /sync
               (fix loop)                  (fix loop)
```
- `/review` (@reviewer): static analysis — code quality, spec compliance, safety. Output: issues or `APPROVED`.
- `/validate` (@coder): runtime — build, run binary, check DoD, fill `## Execution Report` + check DoD boxes.
- `/sync` (@architect): runs only after both pass — updates design doc, archives task.
- Fix loops always go back to `/implement` (@coder owns all source changes).

## Content Quality & Consistency
- `/test-ux` (@tester): reads `<module>/README.md` and module source code. Simulates a junior student walkthrough by mentally executing bash commands and verifying the pedagogical flow to identify missing prerequisites or steep cognitive leaps. Output: Pedagogical Gaps & UX Friction report.
- `/audit` (@auditor): reads both `<module>/README.md` and `workflow/design/<module>.md` (inspecting tasks if necessary). Uses web search to validate technical claims against online sources and identifies/removes redundant comments across the document chain. Output: Audit report with citations and direct file edits.

## State Reconciliation
- `/update-design` (@architect): reads a specific `workflow/tasks/<task_file>.md` (e.g., when execution reveals blockers or requires a technical pivot). Reverse-syncs these findings by updating the parent `workflow/design/<module>.md` (Key Decisions, Known Issues). Output: Updated design doc.
- `/update-readme` (@educator): reads the updated `workflow/design/<module>.md`. Forward-syncs any architectural or state changes down into the student-facing `<module>/README.md`, ensuring the technical reality matches the documentation without breaking the educational tone. Output: Updated README.


## Known Issues
- **02_Multimedia/05_OpenVINO_GPU** (): CMake configure fails — OpenVINO SDK not installed on this machine. Expected optional dependency.
- **02_Multimedia/06_Smart_Webcam** (): CMake configure fails — OpenVINO SDK not installed. Expected optional dependency.
- T057:  hyperlink in  retained — it is a correct relative markdown link to the assets README, not a runtime path.

## Discipline
- When asked to **plan a task**, act as **@architect only**: read design doc → identify next step → write task file in `workflow/tasks/`. Do NOT design implementation details (code structure, CMake, buffer strategies) — that is @coder work.
- The task file is the handoff artifact. @coder reads it to implement.
- Never use sed or custom python scripts for file modifications
- ALWAYS show a clear diff before applying any change
- Use str_replace with explicit before/after blocks
- Wait for approval before writing to disk
- **Agent output passthrough:** When a subagent (e.g., @evaluator) uses a Strict Output Template, reproduce its output verbatim — do not reformat or summarize. Applies to `/grade-module` and any skill with a defined template.

## Session Notes

### 2026-03-22 — UX Audit Session (Task 051)
- Ran `/test-ux` across all 7 modules: 00_Setup, 01_Host_API, A_Multimedia, B_Graphics_HPC, C_Robotics_ROS2, 99_Toolbox, 04_Addons.
- Found ~100 issues (19 HIGH, ~50 MED, ~30 LOW). Human triaged; all [x] items applied across 30 files.
- Task 051 archived. `workflow/tasks/ux_audit_report.md` also archived.
- No source files (.cpp, .cl, CMakeLists.txt) were modified.

### 2026-03-23 — Final Verification Session (Task T057)
- Ran standalone build sweep across all 40+ modules; confirmed all pass except 05_OpenVINO_GPU and 06_Smart_Webcam (OpenVINO SDK not installed — expected).
- Fixed depth-relative asset path in `06_Bonus/03_VkFFT_Audio/vkFFTAudio.md` (was three levels deep, now `assets/sample.wav`).
- Fixed stale `cd` path and back-link target in `vkFFTAudio.md` (now points to `../Bonus.md`).
- Added back-link to `00_Setup/01_Smoke_Test/SmokeTest.md`.
- Updated MEMORY.md progress counters to match v2.0 structure and `scripts/progress.sh` output.

### 2026-03-24 — Submodule Directory Rename Session (Task T060)
- Renamed all 36 submodule directories across 02_Multimedia through 06_Bonus to `NN_Title_Snake_Case` convention.
- Updated all 5 module index READMEs and all affected submodule READMEs (cd commands, cross-links).
- Updated MEMORY.md Known Issues to use new canonical paths.

### 2026-03-25 — Bonus READMEs Session (Task T062)
- Authored `06_Bonus/01_CLBlast_MatMul/CLBlastMatMul.md` and `06_Bonus/02_Device_Enqueue/DeviceEnqueue.md`; updated `06_Bonus/Bonus.md` table links.
- **Lesson:** Always fetch prior-art reference files in **full** (`git show <branch>:<path>` with no `head`/line limit) before README authoring. Truncation at 200 lines caused the B4 section to be missed entirely, requiring a second `@educator` pass to recover the GEMM Key Terms callout, CUDA comparison note, and clinfo compatibility note.
