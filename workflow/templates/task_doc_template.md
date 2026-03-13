# Task [ID]: [Short Title]

## Context
- **Design Feature:** `workflow/design/[feature_name].md`
- **Milestone:** [Phase Name from Design Roadmap]
- **Relevant Files:**
  - `[path/to/file1]` — (read-only: reference)
  - `[path/to/file2]` — (to modify)
  - `[path/to/file3]` — (new file)

## Objective
<!-- MANDATORY. One sentence minimum. What is the specific, atomic goal? -->
[1 sentence: What is the specific, atomic goal of this task?]

## Constraints & Rules
<!-- All standard constraints inherited from .claude/rules/00_master_specs.md (C++17, cl.hpp,
     CLI11, create_context(), cl::Event profiling, CL_CHECK, standalone CMake, kernel copy rule).
     List ONLY task-specific overrides or additions below. -->
- [Task-specific override or addition — delete if none]

---

## Implementation
<!-- Choose one format based on task type: -->

<!-- FORMAT A — Feature task (adding new functionality, scaffolding new modules) -->
1. [Step 1]
2. [Step 2]
3. [Step 3]

<!-- FORMAT B — Cleanup/refactor task (use named Items instead of steps) -->
<!--
### [Letter] — [Item Name]
**Problem:** [What is wrong / what duplication / what debt exists?]

**Decision:** [Chosen approach and rationale.]

**Action:** [Concrete steps for @coder to execute.]

---
-->

## Definition of Done (DoD)
<!-- Standard items defined in .claude/rules/00_master_specs.md §8 apply to all tasks. -->
<!-- Add ONLY task-specific outcomes below. -->
<!--
  Prefix convention:
    (no prefix)  — agent-verifiable: build output, file existence, console output, exit code.
    MANUAL:      — requires human eyes/hardware: live display windows, webcam, audio, physical device.
                   The /validate agent MUST stop and leave these unchecked. The human checks and
                   ticks them before /sync can run.
  Examples:
    - [ ] Binary runs without error; output BMP saved with blurred ROI.
    - [ ] MANUAL: Run `./build/<bin> --device 0`; confirm live window opens, face blurred, 'q' exits.
    - [ ] MANUAL: Run with `--loop --input assets/face.png`; confirm preview loops until 'q'.
-->
- [ ] [Task-specific outcome]
- [ ] MANUAL: [Human-only verification step — describe exact command and what to look for]

---

## Execution Report
<!-- Filled by @coder after implementation. -->

- **Status:** PENDING
  <!-- PENDING → IN PROGRESS → COMPLETED -->
- **Session:** [YYYY-MM-DD]

### Completed
<!-- For multi-item tasks (Format B). Delete section if not applicable. -->
| Item | Action |
|------|--------|
| [A — Item Name] | [What was done] |

### Validation
<!-- Paste terminal output, test results, or observable evidence. -->
```
[output here]
```

### Changed Files
| File | Change |
|------|--------|
| `[path/to/file]` | [Created / Modified — brief description] |

### Remaining
<!-- For IN PROGRESS state: list blockers or incomplete items. Delete when COMPLETED. -->
- [ ] [Remaining item]
