# Applied OpenCL Lab – Memory Log

## Design Decisions (Long-Term)
- **Build System:** CMake 3.18+, hybrid approach (Main + Standalone).
- **OpenCL Wrapper:** cl.hpp version 1.2 (for Nvidia compatibility).
- **C++ Standard:** C++17.
- **GPU Selection:** `GPU` env var (vendor substring, case-insensitive). Implemented in `common/ocl_wrapper.hpp::create_context()`. Matches `CL_PLATFORM_VENDOR` or `CL_DEVICE_VENDOR`. Examples: `GPU=NVIDIA`, `GPU=AMD`, `GPU=INTEL`. Default: first GPU found, CPU fallback. Hard-coded device indices are FORBIDDEN.

## Progress Tracking
**Overall: 39/47 tasks → 82%** (run `scripts/progress.sh`)
- [x] Module 0: Setup (100%)
- [x] Module 1: Host API (100% — 6/6)
- [x] Module 4: Path A — Multimedia & AI (100% — 8/8)
- [x] Module 5: Path B — Graphics & HPC (100% — 6/6)
- [x] Module 6: Path C — Robotics & ROS2 (100% — 6/6)
- [x] Module 7: Optimization Toolbox (100% — 13/13)
- [ ] Module 8: Add-ons (0/8 — 0%)

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

## Known Issues
[TODO: Add issues here]

## Discipline
- When asked to **plan a task**, act as **@architect only**: read design doc → identify next step → write task file in `workflow/tasks/`. Do NOT design implementation details (code structure, CMake, buffer strategies) — that is @coder work.
- The task file is the handoff artifact. @coder reads it to implement.
- Never use sed or custom python scripts for file modifications
- ALWAYS show a clear diff before applying any change
- Use str_replace with explicit before/after blocks
- Wait for approval before writing to disk


## Session Notes
[TODO: Add notes after each session]
