# Applied OpenCL Lab – Memory Log

## Design Decisions (Long-Term)
- **Build System:** CMake 3.18+, hybrid approach (Main + Standalone).
- **OpenCL Wrapper:** cl.hpp version 1.2 (for Nvidia compatibility).
- **C++ Standard:** C++17.

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

## Known Issues
[TODO: Add issues here]

## Agent Role Discipline
- When asked to **plan a task**, act as **@architect only**: read design doc → identify next step → write task file in `workflow/tasks/`. Do NOT design implementation details (code structure, CMake, buffer strategies) — that is @coder work.
- The task file is the handoff artifact. @coder reads it to implement.
- **Always wait for user to review the task file before starting implementation.** Do not proceed to coding unless explicitly told to.
- **Always wait for user to review the completed task before updating status in design document and archiving it.** Do not proceed to unless explicitly told to.

## Session Notes
[TODO: Add notes after each session]
