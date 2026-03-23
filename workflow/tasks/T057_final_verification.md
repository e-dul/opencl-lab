# Task T057: Final Verification — D09 Cookbook v2.0 Pivot

## Context
- **Design Feature:** `workflow/design/D09_cookbook_v2_pivot.md`
- **Milestone:** Phase 7 — Final Verification
- **Relevant Files:**
  - `workflow/design/D09_cookbook_v2_pivot.md` — (read-only: spec / DoD source)
  - `.claude/rules/MEMORY.md` — (to modify: update progress counters)
  - `scripts/progress.sh` — (to modify: update module paths/counts to match v2.0 structure, then use output as reference for MEMORY.md counters)
  - All `*/CMakeLists.txt` under `01_Host_API/`, `02_Multimedia/`, `03_GraphicsHPC/`, `04_Robotics/`, `05_Toolbox/`, `06_Bonus/`, `00_Setup/` — (read-only: build verification)
  - All `*.md` sub-module docs — (to modify: add missing back-links)

## Objective
Verify that every structural guarantee of the D09 pivot is satisfied: all modules build standalone, no depth-relative asset paths survive in docs, every sub-module doc carries a back-link, and MEMORY.md progress counters reflect the v2.0 structure.

## Constraints & Rules
- No changes to `.cpp`, `.cl`, or kernel source files.
- Do not modify `workflow/design/D09_cookbook_v2_pivot.md` — it is read-only input for this task.
- Back-links must use the form `[Back to <ModuleName>.md](../<ModuleName>.md)` or equivalent relative path — do NOT use absolute paths.
- MEMORY.md counter update must be consistent with `scripts/progress.sh` output (run the script to get current numbers before editing).

---

## Implementation

### A — Standalone Build Sweep
**Problem:** It is unverified whether all standalone sub-modules build cleanly after the v2.0 restructure (Phase 1–5 changes).

**Decision:** Run `cmake -B build && cmake --build build` for every leaf module that has its own `CMakeLists.txt`, including ROS 2 modules.

**Action:**
1. For each module directory under `00_Setup/`, `01_Host_API/`, `02_Multimedia/`, `03_GraphicsHPC/`, `04_Robotics/`, `05_Toolbox/`, `06_Bonus/`: run `cmake -B build && cmake --build build` from that directory.
2. Record pass/fail in the Execution Report table.
3. If any module fails, note the error in the Execution Report — do NOT fix it in this task (file a Known Issue in the design doc instead).

---

### B — Depth-Relative Asset Path Audit
**Problem:** One known file (`06_Bonus/vkFFT_Audio/vkFFTAudio.md`) still contains a depth-relative asset path (`../../../assets/` or similar).

**Decision:** Replace all depth-relative asset paths with the `assets/` short form (the POST_BUILD symlink makes `assets/` available in the binary directory).

**Action:**
1. Run a repo-wide search for `../assets`, `../../assets`, `../../../assets` in all `*.md` files (excluding `workflow/` and `build/` directories).
2. Replace each occurrence with `assets/` (the path relative to the binary dir, consistent with all other module docs).
3. Verify no depth-relative asset paths remain.

---

### C — Sub-Module Back-Link Audit
**Problem:** Not all sub-module docs have a back-link to their parent module index.

**Decision:** Add a back-link as the last line of any sub-module doc that is missing one.

**Action:**
1. Identify sub-module docs without a back-link (any `*.md` under a sub-module directory that does not contain `[Back` or `←`).
2. Append `---\n\n[Back to <ParentIndex>.md](../<ParentIndex>.md)` at the end of each such file.
3. The module index filenames are: `HostAPI.md`, `Multimedia.md`, `GraphicsHPC.md`, `RoboticsROS2.md`, `Toolbox.md`, `Bonus.md`.
4. `00_Setup/01_Smoke_Test/SmokeTest.md` is the only currently identified missing file — audit all others for completeness.

---

### D — MEMORY.md Progress Counter Update
**Problem:** MEMORY.md still shows `Overall: 39/47 tasks → 82%` and module entries that predate the v2.0 restructure. `scripts/progress.sh` also contains hardcoded paths/module names from the old structure and must be updated before its output is meaningful.

**Decision:** First fix `scripts/progress.sh` to reflect the v2.0 directory layout, then use its output to update MEMORY.md counters.

**Action:**

1. Read `scripts/progress.sh` and update any hardcoded module paths, directory names, or task counts to match the v2.0 structure.
2. Run `bash scripts/progress.sh` from the repo root to get current numbers.
3. Update the `## Progress Tracking` section in `.claude/rules/MEMORY.md`:
   - Update the `Overall: N/M tasks → P%` line.
   - Update or remove module entries that no longer match the v2.0 structure.
4. Mark Phase 7 Verification Criteria checkboxes in `workflow/design/D09_cookbook_v2_pivot.md` as complete where applicable (using the findings from Items A–C).

---

## Definition of Done (DoD)

Standard items from `00_master_specs.md §8` apply where relevant (standalone build).

### A — Build Sweep

- [ ] Every standalone module under `00_Setup/`, `01_Host_API/`, `02_Multimedia/`, `03_GraphicsHPC/`, `04_Robotics/`, `05_Toolbox/`, `06_Bonus/` builds with zero errors and zero warnings.

### B — Asset Paths
- [ ] `grep -r "\.\./.*assets" --include="*.md"` (excluding `workflow/` and `build/`) returns zero results across the repo.

### C — Back-Links
- [ ] Every sub-module `*.md` doc (non-index, non-top-level) ends with a back-link to its parent module index.
- [ ] MANUAL: Spot-check three sub-module docs (one from `02_Multimedia/`, one from `05_Toolbox/`, one from `06_Bonus/`) to confirm back-links render correctly and point to the right file.

### D — MEMORY.md
- [ ] `MEMORY.md` `## Progress Tracking` section matches the output of `bash scripts/progress.sh`.
- [ ] Phase 7 checkboxes in `workflow/design/D09_cookbook_v2_pivot.md` are ticked for all criteria confirmed complete.

---

## Execution Report
<!-- Filled by @coder after implementation. -->

- **Status:** PENDING
  <!-- PENDING → IN PROGRESS → COMPLETED -->
- **Session:** [YYYY-MM-DD]

### Completed
| Item | Action |
|------|--------|
| A — Build Sweep | |
| B — Asset Paths | |
| C — Back-Links | |
| D — MEMORY.md Update | |

### Validation
```
[output here]
```

### Changed Files
| File | Change |
|------|--------|
| `.claude/rules/MEMORY.md` | Modified — updated progress counters |
| `workflow/design/D09_cookbook_v2_pivot.md` | Modified — Phase 7 checkboxes ticked |

### Remaining
- [ ] [Remaining item]
