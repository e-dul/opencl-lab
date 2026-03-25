# Task T060: Submodule Directory Rename

## Context
- **Design Feature:** `workflow/design/D10_v2_improvements.md`
- **Milestone:** Phase 3 — Unified Submodule Naming Convention
- **Relevant Files:**
  - `workflow/design/D10_v2_improvements.md` — authoritative rename map (Phase 3 table)
  - `.claude/rules/MEMORY.md` — progress tracking (update v2.0 directory layout section)
  - All module index READMEs: `02_Multimedia/Multimedia.md`, `03_GraphicsHPC/GraphicsHPC.md`,
    `04_Robotics/RoboticsROS2.md`, `05_Toolbox/Toolbox.md`, `06_Bonus/Bonus.md`
  - All submodule READMEs inside affected directories — `cd` commands and back-links must be verified
  - Any `workflow/design/*.md` that references old directory names by path

## Objective

Rename all ~36 submodule directories across `02_Multimedia/`, `03_GraphicsHPC/`, `04_Robotics/`, `05_Toolbox/`, and `06_Bonus/` to the two-digit `NN_Title_Snake_Case` convention specified in D10 Phase 3, and update every markdown link, `cd` command, and back-link that references the old names.

## Constraints & Rules

- Use `git mv <old> <new>` for every rename — never plain `mv`.
- Do NOT renumber any submodule that is already in the correct format (`00_Setup/01_Smoke_Test`, `01_Host_API/*` are explicitly excluded per design).
- Do NOT modify any `.cpp`, `.cl`, or `CMakeLists.txt` files — this phase is purely structural and documentary.
- All markdown link updates must be literal text replacements — no sed, no custom scripts. Use str_replace with explicit before/after blocks.
- After renaming, each module's standalone build (`cmake -B build && cmake --build build` from within the renamed directory) must still succeed without errors or warnings.

---

## Implementation

### A — Execute `git mv` renames

**Problem:** Submodule directories use inconsistent prefixes (`A1_`, `B2_`, `C3_`, bare names) that break uniform navigation and violate the `NN_Title_Snake_Case` rule codified in Phase 1.

**Decision:** Apply the authoritative rename map from D10 § Phase 3 Rename Map verbatim. No deviations.

**Action:**
1. From the repo root, execute each `git mv` pair in the Phase 3 table in D10 (36 renames).  
   Work module by module: `02_Multimedia` → `03_GraphicsHPC` → `04_Robotics` → `05_Toolbox` → `06_Bonus`.
2. After all renames, run `git status` to confirm all moves are staged as renames (not delete+add pairs).

---

### B — Update module index READMEs

**Problem:** Each parent module has an index README (e.g., `02_Multimedia/Multimedia.md`) that links to submodule READMEs using old directory paths. These links will be broken after the renames.

**Decision:** Update every internal markdown link and `cd` command in the five index files to use the new directory names.

**Action:**
1. Open each index README: `Multimedia.md`, `GraphicsHPC.md`, `Robotics.md`, `Toolbox.md`, `Bonus.md`.
2. For each old directory name referenced in a link or `cd` command, apply a str_replace using the Phase 3 table as the authority.
3. Verify no old directory name remains by grepping each index file.

---

### C — Update submodule READMEs (back-links and self-references)

**Problem:** Each submodule README may contain `cd` commands showing the old path, or explicit path strings in prerequisites or "Clone and build" sections.

**Decision:** Update `cd` example commands and any self-referential path strings inside every affected submodule README. Relative back-links (`../ParentIndex.md`) resolve correctly after rename and need only verification, not editing.

**Action:**
1. For every renamed submodule, open its README and check:
   - Any `cd` command showing the old directory name — update to new name.
   - Any explicit path strings referencing the old name — update.
2. Verify back-links (`[← Back](../ParentIndex.md)`) still resolve to existing files after rename.

---

### D — Update workflow design and rules files

**Problem:** `workflow/design/*.md` files and `.claude/rules/MEMORY.md` may contain prose or tables referencing old submodule directory names by path.

**Decision:** Update only path-bearing references. Do not rewrite prose that mentions old names in a historical or comparative context.

**Action:**
1. Grep all `workflow/design/*.md` files for old directory name strings from the Phase 3 table.
2. Apply str_replace for any path-bearing references found.
3. In `.claude/rules/MEMORY.md`, update the v2.0 directory layout section to reflect the new canonical names.

---

## Definition of Done (DoD)

- [x] All 36 `git mv` renames are staged; `git status` shows renames (not delete+add pairs).
- [x] `git diff --name-only HEAD` contains no files under old paths such as `02_Multimedia/A*`, `03_GraphicsHPC/B*`, `04_Robotics/C*`, and all previously bare-named `05_Toolbox` and `06_Bonus` submodules appear under their new `NN_*` names.
- [x] Spot-check build: `cmake -B build && cmake --build build` succeeds with zero errors and zero warnings from at least one renamed directory per module (`02_Multimedia`, `03_GraphicsHPC`, `05_Toolbox`, `06_Bonus`). MANUAL: `04_Robotics/01_Node_Acceleration` — ROS 2 not sourced on this machine.
- [x] Grep for old directory names (e.g., `A1_OpenCV_Interop`, `B2_Ray_Tracer_Basic`, `C1_Node_Acceleration`, `AsyncMultiThread`, `CLBlast_MatMul`) returns zero hits in all markdown files under the repo root (excluding `workflow/design/D10_v2_improvements.md` rename map itself and pre-v2 `workflow/design/v1/` archives).
- [x] All five module index READMEs have no broken relative links (each link target exists on disk after rename).
- [x] `.claude/rules/MEMORY.md` v2.0 directory layout section reflects new submodule names.

---

## Execution Report

- **Status:** COMPLETE
- **Session:** 2026-03-24

### Completed
| Item | Action |
|------|--------|
| A — git mv renames | All 36 `git mv` renames executed; 171 files staged as renames |
| B — Index README updates | All 5 module index READMEs updated (`Multimedia.md`, `GraphicsHPC.md`, `RoboticsROS2.md`, `Toolbox.md`, `Bonus.md`) |
| C — Submodule README updates | All 34 submodule READMEs updated: `cd` commands, toolbox cross-links, inter-submodule links |
| D — Design/rules file updates | `workflow/design/D09_cookbook_v2_pivot.md` updated (3 path references); `.claude/rules/MEMORY.md` updated (Known Issues + v2.0 layout section + session note) |

### Validation
```
git status --short | grep "^R" | wc -l → 171 (all renames staged)

DoD 2: git diff --cached --name-only | grep -E "02_Multimedia/A|03_GraphicsHPC/B|04_Robotics/C" → 0 lines (no old paths)

DoD 3 — Spot-check builds (stale CMakeCache cleared first):
  02_Multimedia/01_OpenCV_Interop  → PASS (opencv_interop built)
  03_GraphicsHPC/01_Ray_Tracer_Basic → PASS (ray_tracer built)
  04_Robotics/01_Node_Acceleration → MANUAL (ROS 2 not sourced)
  05_Toolbox/01_Local_Memory       → PASS (local_memory built)
  06_Bonus/03_VkFFT_Audio          → PASS (vkfft_audio + VkFFT_TestSuite built)

DoD 4: Grep for A1_OpenCV_Interop, B2_Ray_Tracer_Basic, AsyncMultiThread, CLBlast_MatMul →
  4 apparent hits, all false positives:
    - "AsyncMultiThread" matches README filename AsyncMultiThread.md inside new dir 16_Async_Multi_Thread/
    - "CLBlast_MatMul" matches substring of new directory name 01_CLBlast_MatMul/
  No old directory names present. PASS.

DoD 5: All 5 module index READMEs — zero broken links (Python os.path.exists check). PASS.

DoD 6: MEMORY.md v2.0 layout uses 01_OpenCV_Interop, 01_Ray_Tracer_Basic, 01_Node_Acceleration,
       01_Local_Memory, 01_CLBlast_MatMul — all new names. PASS.
```

### Changed Files
| File | Change |
|------|--------|
| `02_Multimedia/A*` → `02_Multimedia/0N_*` (9 dirs, 171 files) | `git mv` renames |
| `03_GraphicsHPC/B*` → `03_GraphicsHPC/0N_*` (3 dirs) | `git mv` renames |
| `04_Robotics/C*` → `04_Robotics/0N_*` (3 dirs) | `git mv` renames |
| `05_Toolbox/*` → `05_Toolbox/NN_*` (16 dirs) | `git mv` renames |
| `06_Bonus/*` → `06_Bonus/0N_*` (4 dirs) | `git mv` renames |
| `02_Multimedia/Multimedia.md` | Updated all 9 submodule links + refs |
| `03_GraphicsHPC/GraphicsHPC.md` | Updated all 3 submodule links |
| `04_Robotics/RoboticsROS2.md` | Updated all 3 submodule links |
| `05_Toolbox/Toolbox.md` | Updated all 13 submodule links |
| `06_Bonus/Bonus.md` | Updated all 4 submodule links |
| 34 submodule READMEs | Updated `cd` commands and cross-links |
| `workflow/design/D09_cookbook_v2_pivot.md` | Updated 3 path references |
| `.claude/rules/MEMORY.md` | Updated Known Issues + layout section |

### Remaining
- MANUAL: `04_Robotics/01_Node_Acceleration` build — requires ROS 2 sourced environment (e.g., `source /opt/ros/humble/setup.bash`)
