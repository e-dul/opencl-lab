# Task 050: Audit All Modules

## Context
- **Design Feature:** N/A (cross-cutting maintenance task)
- **Milestone:** Documentation Quality & Factual Accuracy Pass
- **Relevant Files:**
  - `.claude/skills/technical-audit/SKILL.md` — (read-only: audit procedure)
  - `.claude/commands/audit.md` — (read-only: audit command definition)
  - `01_Host_API/README.md` + `workflow/design/01-host-api.md`
  - `02_Projects/A_Multimedia/README.md` + `workflow/design/04-multimedia-projects.md`
  - `02_Projects/B_Graphics_HPC/README.md` + `workflow/design/05-graphics-hpc-projects.md`
  - `02_Projects/C_Robotics_ROS2/README.md` + `workflow/design/06-robotics-ros2-projects.md`
  - `99_Toolbox/README.md` + `workflow/design/07-toolbox.md`
  - `04_Addons/README.md` + `workflow/design/08-addons.md`
  - `workflow/tasks/050_audit_findings.md` — (new file: structured findings report)

## Objective
Run the `/audit` procedure across all six modules, collect findings into a single structured report, and auto-fix the safe category of issues (redundant comments / copy-paste duplication between README and design doc) while flagging factual-accuracy issues for human review.

## Constraints & Rules
- Follow the `technical-audit` skill exactly: read README + design doc, web-search claims, de-duplicate.
- **Auto-fix (safe):** Redundant comment blocks that appear verbatim (or near-verbatim) in both a module README and its design doc. Strip the duplicate from whichever file is less authoritative for that content (design doc for student-facing prose; README for implementation details).
- **Flag only (human review):** Any factual claim in either README or design doc that web search cannot confirm or actively contradicts. Do NOT silently delete; add a `> AUDIT FLAG:` blockquote inline in the affected file.
- Do NOT modify `.cpp`, `.cl`, `CMakeLists.txt`, or any source files.
- Do NOT modify `workflow/design/00-executive-summary.md` or `CLAUDE.md`.
- Show a clear diff before applying every auto-fix. Wait for no approval (batch mode), but record every edit in the findings report.

---

## Implementation

### A — Per-Module Audit Pass

**Problem:** No systematic cross-module documentation audit has been run. Claims may be stale; design docs and READMEs may duplicate each other.

**Decision:** Run audit on all modules in sequence; accumulate findings in a single report file before applying any fixes.

**Action:**

1. For each module listed below, invoke the `technical-audit` skill (read README + design doc, web-search top 3 technical claims):
   - `01_Host_API` → `workflow/design/01-host-api.md`
   - `02_Projects/A_Multimedia` → `workflow/design/04-multimedia-projects.md`
   - `02_Projects/B_Graphics_HPC` → `workflow/design/05-graphics-hpc-projects.md`
   - `02_Projects/C_Robotics_ROS2` → `workflow/design/06-robotics-ros2-projects.md`
   - `99_Toolbox` → `workflow/design/07-toolbox.md`
   - `04_Addons` → `workflow/design/08-addons.md`

2. For each module, record findings in the categories below (see Item B).

---

### B — Findings Report

**Problem:** Findings are lost between audit runs if not persisted.

**Decision:** Write a single `workflow/tasks/050_audit_findings.md` file before applying fixes.

**Action:**

Create `workflow/tasks/050_audit_findings.md` with the following structure per module:

```markdown
# Audit Findings — All Modules

## [Module Name]
### Factual Claims (FLAG — human review required)
| Claim | Source File | Web Search Result | Verdict |
|-------|-------------|-------------------|---------|
| ...   | ...         | ...               | UNVERIFIED / FALSE / OK |

### Redundancies (AUTO-FIX applied)
| Duplicated Content | Kept In | Removed From |
|--------------------|---------|--------------|
| ...                | ...     | ...          |

### Suggested Resources
- [Title](URL) — relevance note
```

---

### C — Apply Auto-Fixes

**Problem:** Redundant content increases maintenance burden and confuses students.

**Decision:** Remove verbatim duplicate paragraphs/lists from design docs where the same content already exists in the module README.

**Action:**

1. For each redundancy identified in Item B, show the diff and apply the removal from the design doc using `str_replace`.
2. Add inline `> AUDIT FLAG:` blockquotes in design docs for any unverified/false claims.
3. Both README and design doc are in scope for edits — apply redundancy removals and `AUDIT FLAG` markers to whichever file contains the issue.

---

## Definition of Done (DoD)

- [ ] `workflow/tasks/050_audit_findings.md` exists and contains entries for all 6 modules.
- [ ] Every factual claim marked UNVERIFIED or FALSE has an `> AUDIT FLAG:` blockquote inserted in the relevant design doc.
- [ ] All confirmed redundancies have been removed from the less-authoritative file (README or design doc, depending on content type).
- [ ] No `.cpp`, `.cl`, or `CMakeLists.txt` files were modified.
- [ ] MANUAL: Human reviews the `050_audit_findings.md` report and resolves `AUDIT FLAG` items — either correcting the claim text or confirming it with an authoritative source, then removing the flag.

---

## Execution Report
<!-- Filled by @auditor after execution. -->

- **Status:** PENDING
- **Session:** [YYYY-MM-DD]

### Completed
| Item | Action |
|------|--------|
| [A — Per-Module Audit Pass] | [What was done] |
| [B — Findings Report] | [What was done] |
| [C — Apply Auto-Fixes] | [What was done] |

### Validation
```
[output here]
```

### Changed Files
| File | Change |
|------|--------|
| `workflow/tasks/050_audit_findings.md` | Created — structured audit report |

### Remaining
- [ ] Human review of AUDIT FLAG items
