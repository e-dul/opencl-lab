# OpenSpec Migration

## Context

- **Design Feature:** OpenSpec integration (`open-spec-test` branch)
- **Milestone:** Infrastructure setup — spec layer established, old S&T workflow removed
- **Relevant Files:**
  - `openspec/config.yaml` — OpenSpec schema and project metadata
  - `openspec/specs/` — 8 bootstrapped capability specs
  - `openspec/changes/` — active change proposals (empty, ready for use)
  - `.claude/commands/opsx/` — 4 slash commands (propose, apply, archive, explore)
  - `.claude/skills/openspec-*/` — 4 skill files
  - `.claude/rules/04-skills-reference.md` — updated agents table + OpenSpec commands
  - `.claude/rules/MEMORY.md` — removed stale S&T discipline bullet
  - `CLAUDE.md` — added OpenSpec Workflow section
  - `workflow/design/archive/` — D09–D12 archived
  - `workflow/templates/archive/` — design_doc_template.md, task_doc_template.md archived

## Objective

Replace the S&T (Strategy & Tactics) workflow with OpenSpec as the proposal and planning layer, while preserving implementation history and student-facing content.

## What Was Done

### Phase 1 — Archive completed design docs

Moved to `workflow/design/archive/`:
- D09_cookbook_v2_pivot.md
- D10_v2_improvements.md
- D11_v2_1_improvements.md
- D12_v2_2_improvements.md

Moved to `workflow/templates/archive/`:
- design_doc_template.md
- task_doc_template.md

Kept:
- `workflow/design/00-executive-summary.md` — living scope reference
- `workflow/tasks/archive/` — historical task record
- `workflow/templates/module_doc_template.md` — still used for student READMEs

### Phase 2 — Remove S&T from `.claude/`

Deleted:
- All 16 files in `.claude/commands/`
- All 8 skill directories in `.claude/skills/`
- `.claude/rules/01-protocol-strategy-tactics.md`

Updated:
- `.claude/rules/04-skills-reference.md` — trimmed to agents table + OpenSpec commands
- `.claude/rules/MEMORY.md` — removed `@architect`-only planning discipline bullet
- `CLAUDE.md` — replaced S&T task-storage rule with OpenSpec Workflow section

### Phase 3 — Install & init OpenSpec

```
npm install -g @fission-ai/openspec@latest   (pre-installed)
openspec init --tools claude
```

Created:
- `.claude/commands/opsx/` — propose.md, apply.md, archive.md, explore.md
- `.claude/skills/openspec-propose/`, `openspec-apply-change/`, `openspec-archive-change/`, `openspec-explore/`
- `openspec/config.yaml` (created manually — init skipped in non-interactive mode)
- `openspec/specs/` and `openspec/changes/archive/`

### Phase 4 — Bootstrap spec library

Created 8 capability specs in `openspec/specs/`:

| Spec | Source |
|------|--------|
| `build-system/spec.md` | Master specs §1 |
| `opencl-api/spec.md` | Master specs §1, §4, §7.2, §7.8 + safety §3 |
| `gpu-selection/spec.md` | Master specs §5 |
| `performance/spec.md` | Master specs §6 |
| `cli-args/spec.md` | Master specs §1 |
| `io-standards/spec.md` | Master specs §3 |
| `module-structure/spec.md` | Master specs §2 |
| `safety/spec.md` | `03-safety.md` §1–§4 |

## Definition of Done

- [x] All D* design docs archived to `workflow/design/archive/`
- [x] All old `.claude/commands/` and `.claude/skills/` removed
- [x] `.claude/rules/01-protocol-strategy-tactics.md` deleted
- [x] `CLAUDE.md` updated with OpenSpec Workflow section
- [x] `openspec init --tools claude` complete; 4 commands + 4 skills in `.claude/`
- [x] `openspec/config.yaml`, `specs/`, `changes/`, `changes/archive/` exist
- [x] 8 capability specs written in `openspec/specs/`
- [ ] MANUAL: Restart IDE — verify `/opsx:propose` autocompletes in Claude Code
- [ ] MANUAL: Run `/opsx:propose` with a real feature idea; confirm proposal.md is generated under `openspec/changes/`

## Execution Report

- **Status:** Complete (pending manual verification)
- **Session:** 2026-04-16
- **Branch:** `open-spec-test`
