# Task T054: README Unification — Two-Level Doc Structure

## Context
- **Design Feature:** `workflow/design/D09_cookbook_v2_pivot.md`
- **Milestone:** Phase 3 — README Unification
- **Relevant Files:**
  - `workflow/design/D09_cookbook_v2_pivot.md` — (read-only: authority on two-level structure spec)
  - `02_Multimedia/Multimedia.md` — (to shrink to ≤90-line index)
  - `03_GraphicsHPC/GraphicsHPC.md` — (to shrink to ≤90-line index)
  - `04_Robotics/RoboticsROS2.md` — (to shrink to ≤90-line index)
  - `01_Host_API/HostAPI.md` — (to shrink to ≤90-line index)
  - `02_Multimedia/<SubName>/` — (new sub-module docs: A1–A5, FFmpeg_Pipeline, SoftISP)
  - `03_GraphicsHPC/<SubName>/` — (new sub-module docs: B2, B3, B3_Dynamic)
  - `04_Robotics/<SubName>/` — (new sub-module docs: C1, C2, C3)
  - `01_Host_API/<SubName>/` — (new sub-module docs: 01_Visual_Kernel, 02_Visual_Kernel_Events, 03_Buffer_Flags)

## Objective
Split each fat monolithic module index doc (Multimedia, GraphicsHPC, Robotics, Host API) into a lean ≤90-line module index and individual per-sub-module doc files, following the two-level structure defined in D09.

## Constraints & Rules
- **No source files modified.** `.cpp`, `.cl`, and `CMakeLists.txt` files are out of scope.
- **No `README.md` files created.** Named docs only: `<SubName>.md` inside `<SubName>/` (e.g., `A1_OpenCV_Interop/OpenCVInterop.md`). See D09 Key Decision 6.
- **No `../../../assets/` depth-relative paths** introduced in new docs. Use `assets/<file>` as the CLI argument shown in Build & Run, consistent with the POST_BUILD symlink approach planned in Phase 4. Do not use `../../..` style paths.
- **Cross-links** in sub-module docs must point to existing files using relative paths from the sub-module directory.
- **Bonus and Toolbox** modules already have their own doc structure — do not restructure them in this task.
- **`06_Bonus/` sub-modules** (CLBlast_MatMul, Device_Enqueue) do not yet have sub-module docs. They are NOT in scope for this task (they are not listed in Phase 3 targets).

---

## Implementation

**Decision:** use educator agent and readme creation skill instead of coder!

### Format B — Cleanup items per module

---

### A — Multimedia (`02_Multimedia/`)

**Problem:** `Multimedia.md` is ~407 lines containing full Build & Run, Verify, and Core Concept sections for 7 sub-modules inline. The index's role is navigation, not content delivery. Sub-modules A1–A5 have no individual doc files.

**Decision:** Extract each sub-module's content block into a dedicated `<SubName>.md` file inside its directory. `Multimedia.md` becomes a ≤90-line navigation index with: intro paragraph, Prerequisites, and a navigation table.

**Action:**
1. Read `Multimedia.md` in full to capture all per-sub-module content.
2. For each sub-module directory, create a doc file named as follows:
   - `A1_OpenCV_Interop/OpenCVInterop.md`
   - `A2_YUV_Pipeline/YUVPipeline.md`
   - `A2b_YUYV_Extension/YUYVExtension.md` — if this sub-module is referenced in the index; otherwise omit.
   - `A3_1_OpenCV_DNN/OpenCVDNN.md`
   - `A3_2_OpenVINO_GPU/OpenVINOGPU.md`
   - `A4_Smart_Webcam/SmartWebcam.md`
   - `A5_Privacy_Mode/PrivacyMode.md`
   - `FFmpeg_Pipeline/FFmpegPipeline.md` — already exists; verify it matches the Track/Bonus template sections. Update if sections are missing.
   - `SoftISP/SoftISP.md` — already exists; verify it matches the template. Update if sections are missing.
3. Each new doc must follow the **Track / Bonus template** sections in order (from D09 §Architecture):
   1. `# N.M — Name`
   2. Goal (1–2 sentences)
   3. Prerequisites (delta from module index only)
   4. Build & Run
   5. Verify (expected output / visual artifact description)
   6. Key Concepts
   7. Back-link to `[Path A: Multimedia & AI](../Multimedia.md)` as final line.
4. Mini-challenges, troubleshooting, and known issues that belong to a specific sub-module move into that sub-module doc (under a `## Mini-Challenge` or `## Troubleshooting` section appended after Key Concepts).
5. Track-wide troubleshooting and known issues that apply to multiple sub-modules remain in `Multimedia.md` or in a `## Known Issues` section at the bottom of the index.
6. Rewrite `Multimedia.md` as a ≤90-line index:
   - Opening paragraph (keep or trim existing intro)
   - `## Prerequisites` (common prerequisites only)
   - `## Contents` — replace the `code block` listing with a markdown table: columns = Sub-module, Goal summary, Doc link
   - `## Performance Gates` — a compact table (keep the existing gate table, trimmed)
   - `## What's Next` — navigation links to other tracks and Toolbox
   - Remove all per-sub-module Build & Run, Verify, and Core Concept sections from this file.

---

### B — Graphics & HPC (`03_GraphicsHPC/`)

**Problem:** `GraphicsHPC.md` contains full content for B2, B3, B3_Dynamic inline. No per-sub-module docs exist.

**Decision:** Same split as item A. Note: B1 (CLBlast) and B4 (Device Enqueue) have moved to `06_Bonus/` in Phase 2 — the index must not reference them as `B1`/`B4` in build paths. If they still appear in the index, update the paths to point to `../06_Bonus/CLBlast_MatMul/` or remove them from this index (they belong in the Bonus index).

**Action:**
1. Read `GraphicsHPC.md` in full.
2. Create sub-module docs:
   - `B2_Ray_Tracer_Basic/RayTracerBasic.md`
   - `B3_Ray_Tracer_BVH/RayTracerBVH.md`
   - `B3_Ray_Tracer_BVH_Dynamic/RayTracerBVHDynamic.md`
3. Each doc: Track/Bonus template sections in order, back-link to `[Path B: Graphics & HPC](../GraphicsHPC.md)`.
4. Rewrite `GraphicsHPC.md` as a ≤90-line index (same structure as item A step 6).
5. Remove any build instructions for CLBlast or Device Enqueue that reference the old `B1_CLBlast_MatMul/` or `B4_Device_Enqueue/` paths inside `03_GraphicsHPC/`. Point readers to `06_Bonus/` instead.

---

### C — Robotics (`04_Robotics/`)

**Problem:** `RoboticsROS2.md` contains full content for C1, C2, C3 inline. No per-sub-module docs exist.

**Decision:** Same split as item A.

**Action:**
1. Read `RoboticsROS2.md` in full.
2. Create sub-module docs:
   - `C1_Node_Acceleration/NodeAcceleration.md`
   - `C2_Costmap_Inflation/CostmapInflation.md`
   - `C3_Perception_Node/PerceptionNode.md`
3. Each doc: Track/Bonus template sections in order, back-link to `[Path C: Robotics & ROS 2](../RoboticsROS2.md)`.
4. Rewrite `RoboticsROS2.md` as a ≤90-line index (same structure as item A step 6). `SETUP.md` remains as-is; link to it from the index's Prerequisites section.

---

### D — Host API (`01_Host_API/`)

**Problem:** `HostAPI.md` contains the full pedagogical walkthrough (architecture diagrams, code snippets, full Build & Run sections) for 01, 02, 03 sub-modules inline.

**Decision:** Same split as item A, using the **Track / Bonus template** (these are sequential building-block modules, not tools).

**Action:**
1. Read `HostAPI.md` in full.
2. Create sub-module docs (use `src/` source files for reference when verifying Build & Run commands):
   - `01_Visual_Kernel/VisualKernel.md`
   - `02_Visual_Kernel_Events/VisualKernelEvents.md`
   - `03_Buffer_Flags/BufferFlags.md`
3. Each doc: Track/Bonus template sections in order, back-link to `[Module 1: Host API](../HostAPI.md)`.
4. Rewrite `HostAPI.md` as a ≤90-line index. The "Heterogeneous Architecture in a Nutshell" diagram is module-wide conceptual content — keep it in the index (it is not sub-module-specific). It counts toward the 60-line budget; trim surrounding prose to stay within limit.

---

### E — Root `README.md`

**Problem:** The root `README.md` navigation table links directly to module directories or bare `README.md` files. After this task, the authoritative entry points are the named index docs (e.g., `01_Host_API/HostAPI.md`).

**Action:**

1. Read root `README.md`.
2. Update every module link in the navigation table that currently points to a bare directory or old `README.md` to target the named index doc instead:
   - `01_Host_API/` → `01_Host_API/HostAPI.md`
   - `02_Multimedia/` → `02_Multimedia/Multimedia.md`
   - `03_GraphicsHPC/` → `03_GraphicsHPC/GraphicsHPC.md`
   - `04_Robotics/` → `04_Robotics/RoboticsROS2.md`
3. Do not rewrite any other section of `README.md`.

---

## Definition of Done (DoD)

Standard items from `.claude/rules/00_master_specs.md §8` are **not applicable** — this task creates only documentation files, no build artifacts.

- [x] `02_Multimedia/Multimedia.md` is ≤90 lines and contains a navigation table linking to all sub-module docs.
- [x] `03_GraphicsHPC/GraphicsHPC.md` is ≤90 lines and contains a navigation table linking to all sub-module docs.
- [x] `04_Robotics/RoboticsROS2.md` is ≤90 lines and contains a navigation table linking to all sub-module docs.
- [x] `01_Host_API/HostAPI.md` is ≤90 lines and contains a navigation table linking to all sub-module docs.
- [x] All sub-module docs listed in items A–D exist on disk.
- [x] Every sub-module doc contains all required Track/Bonus template sections (Goal, Prerequisites delta, Build & Run, Verify, Key Concepts) and a back-link to its module index as the final section.
- [x] No sub-module doc is named `README.md`.
- [x] No depth-relative `../../../assets/` paths appear in any new or modified doc file.
- [x] No `.cpp`, `.cl`, or `CMakeLists.txt` files are modified.
- [x] `FFmpeg_Pipeline/FFmpegPipeline.md` and `SoftISP/SoftISP.md` conform to the Track/Bonus template (updated if pre-existing sections were missing).
- [x] `03_GraphicsHPC/GraphicsHPC.md` does not contain build instructions pointing to paths inside `03_GraphicsHPC/B1_*` or `03_GraphicsHPC/B4_*`.
- [x] Root `README.md` navigation links (track/module table) updated to point to the new named index docs (e.g., `01_Host_API/HostAPI.md`) wherever it currently links to `README.md` or bare directory paths.
- [x] Review cycle with @educator agent and readme creation skill
- [x] Check if some key content was lost in reorganization see `*_legacy.md` files and previous module readme versions.

---

## Execution Report

- **Status:** COMPLETE
- **Session:** 2026-03-22

### Completed
| Item | Action |
|------|--------|
| A — Multimedia | `Multimedia.md` rewritten to 51 lines; created A5 PrivacyMode.md; verified/fixed FFmpegPipeline.md and SoftISP.md |
| B — GraphicsHPC | `GraphicsHPC.md` rewritten to 51 lines; created RayTracerBasic.md, RayTracerBVH.md, RayTracerBVHDynamic.md; B1/B4 removed, pointer to `06_Bonus/` added |
| C — Robotics | `RoboticsROS2.md` rewritten to 52 lines; created NodeAcceleration.md, CostmapInflation.md, PerceptionNode.md |
| D — Host API | `HostAPI.md` rewritten to 54 lines; created VisualKernel.md, VisualKernelEvents.md, BufferFlags.md |
| E — Root README | 4 navigation links updated to named index docs |

### Validation
```
Line counts (all ≤90):
  51  02_Multimedia/Multimedia.md
  51  03_GraphicsHPC/GraphicsHPC.md
  52  04_Robotics/RoboticsROS2.md
  54  01_Host_API/HostAPI.md

Depth-relative asset paths (../../../assets/): NONE found
README.md files created: NONE
Source files modified (.cpp/.cl/CMakeLists.txt): NONE
```

### Changed Files
| File | Change |
|------|--------|
| `02_Multimedia/Multimedia.md` | Modified — rewritten to ≤90-line navigation index |
| `03_GraphicsHPC/GraphicsHPC.md` | Modified — rewritten to ≤90-line navigation index |
| `04_Robotics/RoboticsROS2.md` | Modified — rewritten to ≤90-line navigation index |
| `01_Host_API/HostAPI.md` | Modified — rewritten to ≤90-line navigation index |
| `README.md` | Modified — 4 navigation links updated to named index docs |
| `02_Multimedia/FFmpeg_Pipeline/FFmpegPipeline.md` | Modified — heading, cd path, asset path, back-link fixed |
| `02_Multimedia/SoftISP/SoftISP.md` | Modified — heading, cd path, asset path, back-link fixed |
| `02_Multimedia/A5_Privacy_Mode/PrivacyMode.md` | Created — Track template doc |
| `03_GraphicsHPC/B2_Ray_Tracer_Basic/RayTracerBasic.md` | Created — Track template doc |
| `03_GraphicsHPC/B3_Ray_Tracer_BVH/RayTracerBVH.md` | Created — Track template doc |
| `03_GraphicsHPC/B3_Ray_Tracer_BVH_Dynamic/RayTracerBVHDynamic.md` | Created — Track template doc |
| `04_Robotics/C1_Node_Acceleration/NodeAcceleration.md` | Created — Track template doc |
| `04_Robotics/C2_Costmap_Inflation/CostmapInflation.md` | Created — Track template doc |
| `04_Robotics/C3_Perception_Node/PerceptionNode.md` | Created — Track template doc |
| `01_Host_API/01_Visual_Kernel/VisualKernel.md` | Created — Track template doc |
| `01_Host_API/02_Visual_Kernel_Events/VisualKernelEvents.md` | Created — Track template doc |
| `01_Host_API/03_Buffer_Flags/BufferFlags.md` | Created — Track template doc |
