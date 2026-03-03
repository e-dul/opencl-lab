# Applied OpenCL Lab – Memory Log

## Design Decisions (Long-Term)
- **Build System:** CMake 3.18+, hybrid approach (Main + Standalone).
- **OpenCL Wrapper:** cl.hpp version 1.2 (for Nvidia compatibility).
- **C++ Standard:** C++17.
- **GPU Selection:** `GPU` env var (vendor substring, case-insensitive). Implemented in `common/ocl_wrapper.hpp::create_context()`. Matches `CL_PLATFORM_VENDOR` or `CL_DEVICE_VENDOR`. Examples: `GPU=NVIDIA`, `GPU=AMD`, `GPU=INTEL`. Default: first GPU found, CPU fallback. Hard-coded device indices are FORBIDDEN.

## Progress Tracking
- [ ] Module 0: Setup
- [ ] Module 1: Host API
- [ ] Module 2: Projects
- [ ] Toolbox
- [ ] Addons

## Style Guide (Coding Conventions)
- Naming: `snake_case` for variables, `PascalCase` for classes.
- Buffers: Always RAII (`cl::Buffer`).
- Comments: Explain "WHY", not "WHAT".

## Workflow Pipeline
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
