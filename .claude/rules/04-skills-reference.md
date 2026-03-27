# Skills, Agents & Pipelines Reference

## Agents
| Agent | Owns |
|---|---|
| @architect | `workflow/design/`, `workflow/tasks/` — no source code |
| @coder | All `.cpp`, `.cl`, `CMakeLists.txt` changes |
| @reviewer | Read-only analysis; outputs issues or `APPROVED` |
| @educator | `<module>/README.md` — student-facing docs |
| @tester | Simulates junior student walkthrough; outputs UX gaps |
| @auditor | Validates technical claims via web search; edits docs directly |
| @evaluator | Grades modules 1–10; strict output template — reproduce verbatim |
| Explore | Read-only codebase search; use for broad multi-file exploration |
| Plan | Architecture planning; returns step-by-step plans and critical files |

---

## Design Pipeline (Exec Summary → Task)
```
/create-readme  →  /create-design  →  /plan-tasks
                      (revise loop)
```
- `/create-readme` (@educator): reads exec summary / user intent → generates `<module>/README.md`.
- `/create-design` (@architect): reads approved README → generates `workflow/design/<module>.md`.
- `/plan-tasks` (@architect): reads approved design doc → writes next atomic task file to `workflow/tasks/`.
- Revision loop: if design contradicts README intent, re-run `/create-design` with correction notes. Do NOT run `/plan-tasks` with an unapproved design.

## Implementation Pipeline (Task → Product)
```
/implement  →  /review  →  /implement  →  /validate  →  /implement  →  /sync
               (fix loop)                  (fix loop)
```
- `/implement` (@coder): executes the task file; all source code changes.
- `/review` (@reviewer): static analysis — code quality, spec compliance, safety. Output: issues or `APPROVED`.
- `/validate` (@coder): runtime — build, run binary, check DoD, fill `## Execution Report` + check DoD boxes.
- `/sync` (@architect): runs only after both pass — updates design doc, archives task.

## Content Quality
- `/test-ux` (@tester): simulates junior student walkthrough; outputs Pedagogical Gaps & UX Friction report.
- `/audit` (@auditor): validates technical claims via web search; removes redundancies across doc chain.
- `/grade-module` (@evaluator): grades a module 1–10. **Strict output template — reproduce verbatim, no reformatting.**
- `/code-review` (@reviewer): review protocol covering code quality, memory safety, and spec compliance.

## State Reconciliation
- `/update-design` (@architect): reverse-syncs task findings → parent design doc (`Key Decisions`, `Known Issues`).
- `/update-readme` (@educator): forward-syncs design doc changes → student-facing README.

## Additional Skills
- `/task-creation` (@architect): creates next atomic task file from a design doc; writes to `workflow/tasks/`.
- `/simplify` (@reviewer): post-implementation pass — checks changed code for reuse, quality, and efficiency.
- `/validate-dod` (@coder): builds, runs, checks DoD checkboxes, fills Execution Report. Equivalent to `/validate`.
- `/sync-design` (@architect): reverse-syncs task findings → design doc. Equivalent to `/update-design`.
- `/cpp-opencl-dev`: loads core C++17/OpenCL 1.2 engineering guidelines into context.
- `/run_workflow`: runs the full design → task → implement → validate pipeline end-to-end.

## Skill Aliases
| Alias | Canonical |
|---|---|
| `/ux-review` | `/test-ux` |
| `/grading` | `/grade-module` |
| `/sync-readme` | `/update-readme` |
| `/design-creation` | `/create-design` |
| `/readme-creation` | `/create-readme` |
| `/technical-audit` | `/audit` |
| `/validate-dod` | `/validate` |
| `/sync-design` | `/update-design` |
