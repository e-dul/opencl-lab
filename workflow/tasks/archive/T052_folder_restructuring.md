# Task T052: Folder Restructuring (v2.0 Layout)

## Context
- **Design Feature:** `workflow/design/D09_cookbook_v2_pivot.md`
- **Milestone:** Phase 1 — Folder Restructuring
- **Relevant Files:**
  - `workflow/design/D09_cookbook_v2_pivot.md` — (read-only: canonical target layout and migration map)
  - `02_Projects/` — (source: split into three top-level dirs)
  - `04_Addons/` — (source: split between Multimedia, Toolbox, and Bonus)
  - `99_Toolbox/` — (source: rename to `05_Toolbox/`)

## Objective

Rename and split top-level directories so the repository matches the v2.0 Hub & Spoke layout exactly, using `git mv` for all moves to preserve history.

## Constraints & Rules
- Use `git mv` for every rename/move — no `cp` + `rm` workflows.
- Do NOT touch any `.cpp`, `.cl`, `CMakeLists.txt`, or doc file content. Directory structure only.
- Do NOT delete any source files.
- Do NOT create `05_Toolbox/GlobalWorkOffset/` — that is Phase 3 (new content, separate task).
- Do NOT update any CMake `include` paths or doc cross-links — those are Phase 2 and Phase 5.
- After all moves, `git status` must show only renames (no untracked files from stray copies).

---

## Implementation

### A — Rename `99_Toolbox/` to `05_Toolbox/`

**Action:**
```
git mv 99_Toolbox 05_Toolbox
```

---

### B — Create `02_Multimedia/` from `02_Projects/A_Multimedia/` + FFmpeg + SoftISP

**Action:**
```
git mv 02_Projects/A_Multimedia 02_Multimedia
git mv 04_Addons/4_6_FFmpeg_Pipeline 02_Multimedia/FFmpeg_Pipeline
git mv 04_Addons/4_7_SoftISP 02_Multimedia/SoftISP
```

---

### C — Create `03_GraphicsHPC/` from `02_Projects/B_Graphics_HPC/` (minus CLBlast and DeviceEnqueue)

**Action:**
```
git mv 02_Projects/B_Graphics_HPC 03_GraphicsHPC
```
The two recipes that must leave `03_GraphicsHPC/` (CLBlast, DeviceEnqueue) are handled in Item E below after the top-level dir exists.

---

### D — Create `04_Robotics/` from `02_Projects/C_Robotics_ROS2/`

**Action:**
```
git mv 02_Projects/C_Robotics_ROS2 04_Robotics
```

---

### E — Move CLBlast and DeviceEnqueue from `03_GraphicsHPC/` to `06_Bonus/`

**Action:**
```
mkdir -p 06_Bonus
git mv 03_GraphicsHPC/B1_CLBlast_MatMul 06_Bonus/CLBlast_MatMul
git mv 03_GraphicsHPC/B4_Device_Enqueue 06_Bonus/Device_Enqueue
```

---

### F — Move reference recipes from `04_Addons/` to `05_Toolbox/` and `06_Bonus/`

**Action (to Toolbox):**
```
git mv 04_Addons/4_2_OpenCL_vs_CUDA  05_Toolbox/OpenCL_vs_CUDA
git mv 04_Addons/4_4_SVM_Theory      05_Toolbox/SVM_Theory
git mv 04_Addons/4_3_Deployment      05_Toolbox/Deployment
```

**Action (to Bonus — remaining Addons content):**
```
git mv 04_Addons/4_1_vkFFT_Audio    06_Bonus/vkFFT_Audio
git mv 04_Addons/4_5_Voxel_Mapping  06_Bonus/Voxel_Mapping
```

---

### G — Remove now-empty scaffold directories

After all moves, `02_Projects/` and `04_Addons/` should contain only their top-level index `.md` files (e.g., `Projects.md`, `Addons.md`). Move those files to avoid orphaning them, then remove the empty directories.

**Action:**
```
git mv 02_Projects/Projects.md 02_Multimedia/Projects_legacy.md
git mv 04_Addons/Addons.md     06_Bonus/Addons_legacy.md
git rm -r 02_Projects
git rm -r 04_Addons
```

> Note: If `02_Projects/` or `04_Addons/` still contain files not accounted for above, stop and report them rather than silently deleting.

---

## Definition of Done (DoD)

- [x] `ls` at repo root shows exactly: `00_Setup/`, `01_Host_API/`, `02_Multimedia/`, `03_GraphicsHPC/`, `04_Robotics/`, `05_Toolbox/`, `06_Bonus/` as the numbered track directories (plus `assets/`, `common/`, `workflow/`, `scripts/`, top-level files).
- [x] `02_Projects/` does not exist.
- [x] `04_Addons/` does not exist.
- [x] `99_Toolbox/` does not exist.
- [x] `02_Multimedia/` contains `FFmpeg_Pipeline/` and `SoftISP/` (migrated from Addons).
- [x] `03_GraphicsHPC/` does NOT contain `B1_CLBlast_MatMul/` or `B4_Device_Enqueue/`.
- [x] `06_Bonus/` contains `CLBlast_MatMul/`, `Device_Enqueue/`, `vkFFT_Audio/`, `Voxel_Mapping/`.
- [x] `05_Toolbox/` contains `OpenCL_vs_CUDA/`, `SVM_Theory/`, `Deployment/` (migrated from Addons).
- [x] `git status` shows only renames — zero untracked files, zero deleted files outside the removed empty scaffold dirs.
- [x] No `.cpp`, `.cl`, or `CMakeLists.txt` file content was modified (verify with `git diff --stat`).

---

## Execution Report
<!-- Filled by @coder after implementation. -->

- **Status:** DONE
- **Session:** 2026-03-22

### Completed
| Item | Action |
|------|--------|
| [A — Rename Toolbox] | `git mv 99_Toolbox 05_Toolbox` |
| [B — Create Multimedia] | `git mv 02_Projects/A_Multimedia 02_Multimedia` + FFmpeg_Pipeline + SoftISP from Addons |
| [C — Create GraphicsHPC] | `git mv 02_Projects/B_Graphics_HPC 03_GraphicsHPC` |
| [D — Create Robotics] | `git mv 02_Projects/C_Robotics_ROS2 04_Robotics` |
| [E — Move CLBlast + DeviceEnqueue to Bonus] | `mkdir -p 06_Bonus` + git mv from 03_GraphicsHPC |
| [F — Move reference recipes] | OpenCL_vs_CUDA, SVM_Theory, Deployment → 05_Toolbox; vkFFT_Audio, Voxel_Mapping → 06_Bonus |
| [G — Remove empty scaffolds] | Projects.md → 02_Multimedia/Projects_legacy.md; Addons.md → 06_Bonus/Addons_legacy.md; empty dirs removed automatically by git |

### Validation
```
On branch dev
161 files changed, 0 insertions(+), 0 deletions(-)
All changes are pure renames — no content modified.
Untracked: workflow/tasks/T052_folder_restructuring.md (task file itself)
```

### Changed Files
| File | Change |
|------|--------|
| `99_Toolbox/` | Renamed → `05_Toolbox/` |
| `02_Projects/A_Multimedia/` | Moved → `02_Multimedia/` |
| `04_Addons/4_6_FFmpeg_Pipeline/` | Moved → `02_Multimedia/FFmpeg_Pipeline/` |
| `04_Addons/4_7_SoftISP/` | Moved → `02_Multimedia/SoftISP/` |
| `02_Projects/B_Graphics_HPC/` | Moved → `03_GraphicsHPC/` |
| `02_Projects/C_Robotics_ROS2/` | Moved → `04_Robotics/` |
| `03_GraphicsHPC/B1_CLBlast_MatMul/` | Moved → `06_Bonus/CLBlast_MatMul/` |
| `03_GraphicsHPC/B4_Device_Enqueue/` | Moved → `06_Bonus/Device_Enqueue/` |
| `04_Addons/4_2_OpenCL_vs_CUDA/` | Moved → `05_Toolbox/OpenCL_vs_CUDA/` |
| `04_Addons/4_4_SVM_Theory/` | Moved → `05_Toolbox/SVM_Theory/` |
| `04_Addons/4_3_Deployment/` | Moved → `05_Toolbox/Deployment/` |
| `04_Addons/4_1_vkFFT_Audio/` | Moved → `06_Bonus/vkFFT_Audio/` |
| `04_Addons/4_5_Voxel_Mapping/` | Moved → `06_Bonus/Voxel_Mapping/` |
| `02_Projects/Projects.md` | Moved → `02_Multimedia/Projects_legacy.md` |
| `04_Addons/Addons.md` | Moved → `06_Bonus/Addons_legacy.md` |

### Remaining
- None.
