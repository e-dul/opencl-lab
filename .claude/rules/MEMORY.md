# Applied OpenCL Lab – Memory Log

## Design Decisions (Long-Term)
- **Build System:** CMake 3.18+, hybrid approach (Main + Standalone).
- **OpenCL Wrapper:** cl.hpp version 1.2 (for Nvidia compatibility).
- **C++ Standard:** C++17.
- **GPU Selection:** `GPU` env var (vendor substring, case-insensitive). Implemented in `common/ocl_wrapper.hpp::create_context()`. Matches `CL_PLATFORM_VENDOR` or `CL_DEVICE_VENDOR`. Examples: `GPU=NVIDIA`, `GPU=AMD`, `GPU=INTEL`. Default: first GPU found, CPU fallback. Hard-coded device indices are FORBIDDEN.

## Style Guide (Coding Conventions)
- Naming: `snake_case` for variables, `PascalCase` for classes.
- Buffers: Always RAII (`cl::Buffer`).
- Comments: Explain "WHY", not "WHAT".

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
