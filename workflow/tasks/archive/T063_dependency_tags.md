# Task T063: Dependency Tags for HW/OpenCL Version Requirements

## Context
- **Design Feature:** `workflow/design/D10_v2_improvements.md`
- **Milestone:** Phase 6 — Mark submodules with HW/OpenCL version dependencies
- **Relevant Files:**
  - `workflow/design/D10_v2_improvements.md` — (read-only: Phase 6 spec, §Phase 6 Dependency Tag Map)
  - `02_Multimedia/Multimedia.md` — (to modify: add inline tags to table rows)
  - `03_GraphicsHPC/GraphicsHPC.md` — (to modify: add inline tags to table rows)
  - `05_Toolbox/Toolbox.md` — (to modify: add inline tags to table rows)
  - `06_Bonus/Bonus.md` — (to modify: add inline tags to table rows)
  - `02_Multimedia/05_OpenVINO_GPU/OpenVINOGPU.md` — (to modify: add `> **Requires:**` callout block)
  - `02_Multimedia/06_Smart_Webcam/SmartWebcam.md` — (to modify: add `> **Requires:**` callout block)
  - `05_Toolbox/10_SVM/SVM.md` — (to modify: add `> **Requires:**` callout block if not already present)
  - `05_Toolbox/11_SVM_Theory/SVMTheory.md` — (to modify: add `> **Requires:**` callout block)
  - `05_Toolbox/09_OpenCL_vs_CUDA/OpenCLvsCUDA.md` — (to modify: add `> **Requires:**` callout block)
  - `06_Bonus/02_Device_Enqueue/DeviceEnqueue.md` — (to modify: add `> **Requires:**` callout block)
  - `03_GraphicsHPC/01_Ray_Tracer_Basic/RayTracerBasic.md` — (to modify: add `> **Requires:**` callout block)
  - `03_GraphicsHPC/02_Ray_Tracer_BVH/RayTracerBVH.md` — (to modify: add `> **Requires:**` callout block)
  - `03_GraphicsHPC/03_Ray_Tracer_BVH_Dynamic/RayTracerBVHDynamic.md` — (to modify: add `> **Requires:**` callout block)

## Objective

Add two-level dependency callouts to all submodules that require non-baseline hardware or OpenCL versions: a terse inline tag in the parent module index table, and an authoritative `> **Requires:**` callout block at the top of each affected submodule README.

## Constraints & Rules

- No `.cpp`, `.cl`, or `CMakeLists.txt` files may be modified — this task is documentation-only.
- Do not duplicate setup instructions between the index and submodule README. The submodule README is authoritative for setup detail; the index carries only the terse label.
- Inline tags must use the exact strings from the Phase 6 Dependency Tag Map in the design doc.
- The `> **Requires:**` callout block must appear immediately after the opening `**Goal**` paragraph (or the first heading paragraph) so it is visible before the student begins.
- If a `> **Requires:**` block already exists and accurately covers the requirement, do not duplicate it — only verify content and update wording if needed.
- Renumbering, removing, or restructuring any table row in an index README is forbidden.

---

## Implementation

### A — Index README Inline Tags

**Problem:** The module index tables (`Multimedia.md`, `GraphicsHPC.md`, `Toolbox.md`, `Bonus.md`) do not signal which entries require non-standard hardware or OpenCL versions. Students discover the requirement only after attempting to build.

**Decision:** Append a terse italic tag to the description cell of each affected table row. Use the exact tags from the design doc Phase 6 Dependency Tag Map.

**Action:**
1. Read each index README in full before editing.
2. Locate the table row for each affected submodule (use the Phase 6 Dependency Tag Map).
3. Append the tag to the end of the description cell, separated by a space:
   - `02_Multimedia/05_OpenVINO_GPU` → append `*(Intel OpenVINO SDK)*`
   - `02_Multimedia/06_Smart_Webcam` → append `*(Intel OpenVINO SDK)*`
   - `05_Toolbox/10_SVM` → append `*(OpenCL 2.0)*`
   - `05_Toolbox/11_SVM_Theory` → append `*(OpenCL 2.0)*`
   - `05_Toolbox/09_OpenCL_vs_CUDA` → append `*(NVIDIA GPU + CUDA)*`
   - `06_Bonus/02_Device_Enqueue` → append `*(OpenCL 2.0)*`
   - `03_GraphicsHPC/01_Ray_Tracer_Basic` → append `*(cl_khr_gl_sharing)*`
   - `03_GraphicsHPC/02_Ray_Tracer_BVH` → append `*(cl_khr_gl_sharing)*`
   - `03_GraphicsHPC/03_Ray_Tracer_BVH_Dynamic` → append `*(cl_khr_gl_sharing)*`
4. Do not alter any other row or any text outside the modified cell.

---

### B — Submodule README Authoritative Callout Blocks

**Problem:** Individual submodule READMEs may lack a prominent, standardized callout stating the hard dependency. Students must read through prose to find it.

**Decision:** Place a `> **Requires:**` blockquote immediately after the opening goal/description paragraph in each affected submodule README. If equivalent content already exists as a blockquote at that position, update it to the standard format rather than adding a duplicate.

**Action — for each submodule README listed below:**

1. Read the full file before editing.
2. Locate the correct insertion point: immediately after the first paragraph (the `**Goal**` paragraph or equivalent opening description), before any `## Prerequisites` or `## Build` heading.
3. Insert the following callout block (one blank line before and after):

   - `02_Multimedia/05_OpenVINO_GPU/OpenVINOGPU.md`:
     ```
     > **Requires:** Intel iGPU + Intel OpenVINO SDK (`libopenvino-dev`). Run `source /opt/intel/openvino/setupvars.sh` before building. See [SETUP.md](SETUP.md) for full installation steps.
     ```

   - `02_Multimedia/06_Smart_Webcam/SmartWebcam.md`:
     ```
     > **Requires:** Intel iGPU + Intel OpenVINO SDK (`libopenvino-dev`). Run `source /opt/intel/openvino/setupvars.sh` before building. See [SETUP.md](../05_OpenVINO_GPU/SETUP.md) for full installation steps.
     ```

   - `05_Toolbox/10_SVM/SVM.md`:
     ```
     > **Requires:** OpenCL 2.0+ device (AMD APU, Intel iGPU, ARM Mali). Falls back gracefully with an informational message on OpenCL 1.2 devices. Not supported on NVIDIA OpenCL.
     ```

   - `05_Toolbox/11_SVM_Theory/SVMTheory.md`:
     ```
     > **Requires:** OpenCL 2.0+ device (AMD APU, Intel iGPU, ARM Mali). Falls back gracefully with an informational message on OpenCL 1.2 devices. Not supported on NVIDIA OpenCL.
     ```

   - `05_Toolbox/09_OpenCL_vs_CUDA/OpenCLvsCUDA.md`:
     ```
     > **Requires:** NVIDIA GPU with CUDA toolkit installed (for the CUDA side of the comparison). The OpenCL path runs on any supported GPU; only the CUDA build requires NVIDIA hardware.
     ```

   - `06_Bonus/02_Device_Enqueue/DeviceEnqueue.md`:
     ```
     > **Requires:** OpenCL 2.0+ device with `cl_device_device_enqueue_support`. Falls back gracefully with an informational message on devices that do not support device-side enqueue.
     ```

   - `03_GraphicsHPC/01_Ray_Tracer_Basic/RayTracerBasic.md`:
     ```
     > **Requires:** `cl_khr_gl_sharing` extension for the live OpenGL window. Headless BMP output (`--output render.bmp`) works without the extension. Check availability: `clinfo | grep gl_sharing`.
     ```

   - `03_GraphicsHPC/02_Ray_Tracer_BVH/RayTracerBVH.md`:
     ```
     > **Requires:** `cl_khr_gl_sharing` extension for the live OpenGL window. Headless BMP output works without the extension. Check availability: `clinfo | grep gl_sharing`.
     ```

   - `03_GraphicsHPC/03_Ray_Tracer_BVH_Dynamic/RayTracerBVHDynamic.md`:
     ```
     > **Requires:** `cl_khr_gl_sharing` extension for the live OpenGL window. Headless BMP output works without the extension. Check availability: `clinfo | grep gl_sharing`.
     ```

4. If the file already has a blockquote at that position covering the same requirement, replace/update it to match the standardized text above rather than inserting a second block.

---

## Definition of Done (DoD)

- [x] All 4 index READMEs (`Multimedia.md`, `GraphicsHPC.md`, `Toolbox.md`, `Bonus.md`) contain inline tags matching the Phase 6 Dependency Tag Map exactly.
- [x] All 9 affected submodule READMEs contain a `> **Requires:**` callout block immediately after the opening paragraph.
- [x] No tag or callout is duplicated within any single file.
- [x] No `.cpp`, `.cl`, or `CMakeLists.txt` files are modified (verify via `git diff --name-only`).
- [x] `git diff --name-only` lists only `.md` files.
- [x] All internal markdown links in modified files resolve correctly (spot-check: `SETUP.md` link in OpenVINOGPU.md and SmartWebcam.md).
- [ ] MANUAL: Visually scan the rendered table in each index README; confirm the inline tags appear in the correct column and do not break table alignment.

---

## Execution Report

- **Status:** DONE
- **Session:** 2026-03-25

### Completed
| Item | Action |
|------|--------|
| A — Index README Inline Tags | Verified: all 4 index READMEs contain exact tags per Phase 6 map. `Multimedia.md` (2× `*(Intel OpenVINO SDK)*`), `GraphicsHPC.md` (3× `*(cl_khr_gl_sharing)*`), `Toolbox.md` (`*(OpenCL 2.0)*` ×2, `*(NVIDIA GPU + CUDA)*` ×1), `Bonus.md` (`*(OpenCL 2.0)*` ×1). |
| B — Submodule README Callout Blocks | Verified: all 9 submodule READMEs contain the standardized `> **Requires:**` block immediately after the opening paragraph. No duplicates found within any file. |

### Validation
```
git diff --name-only output:
02_Multimedia/05_OpenVINO_GPU/OpenVINOGPU.md
02_Multimedia/06_Smart_Webcam/SmartWebcam.md
02_Multimedia/Multimedia.md
03_GraphicsHPC/01_Ray_Tracer_Basic/RayTracerBasic.md
03_GraphicsHPC/02_Ray_Tracer_BVH/RayTracerBVH.md
03_GraphicsHPC/03_Ray_Tracer_BVH_Dynamic/RayTracerBVHDynamic.md
03_GraphicsHPC/GraphicsHPC.md
05_Toolbox/09_OpenCL_vs_CUDA/OpenCLvsCUDA.md
05_Toolbox/10_SVM/SVM.md
05_Toolbox/11_SVM_Theory/SVMTheory.md
05_Toolbox/Toolbox.md
06_Bonus/02_Device_Enqueue/DeviceEnqueue.md
06_Bonus/Bonus.md

All 13 entries are .md files. No .cpp, .cl, or CMakeLists.txt present.

SETUP.md spot-check:
- OpenVINOGPU.md links to [SETUP.md](SETUP.md) → 02_Multimedia/05_OpenVINO_GPU/SETUP.md ✓ exists
- SmartWebcam.md links to [SETUP.md](../05_OpenVINO_GPU/SETUP.md) → resolves to 02_Multimedia/05_OpenVINO_GPU/SETUP.md ✓ exists
```

### Changed Files
| File | Change |
|------|--------|
| `02_Multimedia/Multimedia.md` | Added `*(Intel OpenVINO SDK)*` tags to rows 05 and 06 |
| `03_GraphicsHPC/GraphicsHPC.md` | Added `*(cl_khr_gl_sharing)*` tags to rows 01, 02, 03 |
| `05_Toolbox/Toolbox.md` | Added `*(OpenCL 2.0)*` to SVM and SVM Theory rows; `*(NVIDIA GPU + CUDA)*` to OpenCL vs CUDA row |
| `06_Bonus/Bonus.md` | Added `*(OpenCL 2.0)*` to Device Enqueue row |
| `02_Multimedia/05_OpenVINO_GPU/OpenVINOGPU.md` | Inserted `> **Requires:**` callout after opening paragraph |
| `02_Multimedia/06_Smart_Webcam/SmartWebcam.md` | Inserted `> **Requires:**` callout after opening paragraph |
| `05_Toolbox/10_SVM/SVM.md` | Inserted `> **Requires:**` callout after opening paragraph |
| `05_Toolbox/11_SVM_Theory/SVMTheory.md` | Inserted `> **Requires:**` callout after opening paragraph |
| `05_Toolbox/09_OpenCL_vs_CUDA/OpenCLvsCUDA.md` | Inserted `> **Requires:**` callout after opening paragraph |
| `06_Bonus/02_Device_Enqueue/DeviceEnqueue.md` | Inserted `> **Requires:**` callout after opening paragraph |
| `03_GraphicsHPC/01_Ray_Tracer_Basic/RayTracerBasic.md` | Inserted `> **Requires:**` callout after opening paragraph |
| `03_GraphicsHPC/02_Ray_Tracer_BVH/RayTracerBVH.md` | Inserted `> **Requires:**` callout after opening paragraph |
| `03_GraphicsHPC/03_Ray_Tracer_BVH_Dynamic/RayTracerBVHDynamic.md` | Inserted `> **Requires:**` callout after opening paragraph |

### Remaining
- None — all automated DoD items pass. MANUAL item left for human review.
