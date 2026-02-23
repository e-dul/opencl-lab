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
- **No Design Changes:** Do not modify `workflow/design/*.md`. If architecture is wrong, stop and ask.
- **Language/Standard:** C++17 (or strictly per project settings).
- **Dependencies:** [List allowed libraries]
- **Error Handling:** Throw `std::runtime_error` on CL errors; use `CL_CHECK()` macro.

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
<!-- Write task-specific entries. Do NOT use generic Build/Test/Output/Verification labels. -->
<!-- Each checkbox should be a concrete, verifiable outcome for THIS task. -->
- [ ] [Specific observable outcome, e.g., "cmake --build succeeds without warnings"]
- [ ] [Specific runtime outcome, e.g., "output.bmp produced and visually correct"]
- [ ] [Specific verification step, e.g., "--help prints CLI11-generated usage"]

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
