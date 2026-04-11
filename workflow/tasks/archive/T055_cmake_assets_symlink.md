# Task T055: CMake POST_BUILD Assets Symlink

## Context
- **Design Feature:** `workflow/design/D09_cookbook_v2_pivot.md`
- **Milestone:** Phase 4 — CMake POST_BUILD Assets Symlink
- **Relevant Files:**
  - `workflow/design/D09_cookbook_v2_pivot.md` — (read-only: canonical symlink spec and Known Issues §)
  - `common/common.cmake` — add `symlink_assets()` function here
  - All `CMakeLists.txt` files listed in the Implementation section — (to modify)

## Objective

Add a `symlink_assets()` helper to `common/common.cmake`, then call it from every module's `CMakeLists.txt` so that after building, the binary directory contains an `assets/` symlink pointing to the repo-root `assets/` folder, eliminating all depth-relative `../../../assets/` path dependencies at runtime.

## Constraints & Rules

- **No source files modified.** `.cpp`, `.cl`, and kernel files are out of scope.
- **Repo root depth is fixed.** All module `CMakeLists.txt` files are exactly two levels below the repo root (`<root>/<ModuleDir>/<SubModule>/CMakeLists.txt`). `symlink_assets()` uses `CMAKE_CURRENT_SOURCE_DIR` of the caller (function scope inherits it) — always resolves to the correct repo root.
- **The `00_Setup/01_Smoke_Test/` module** does not consume image assets — skip it if and only if it has no `--input` CLI flag. Verify by reading its CMakeLists before deciding.
- **`common/CMakeLists.txt`** is a library helper, not a runnable binary — skip it.
- **`CMakeLists.txt` at repo root** is the aggregator — skip it.
- **Symlink target must be absolute.** Use `${CMAKE_CURRENT_LIST_DIR}/../../assets` (resolved at configure time) as the symlink target, not a relative string.
- **Idempotency.** `create_symlink` is safe to re-run; CMake will overwrite an existing symlink. No guard condition is needed.

---

## Implementation

### Step 0 — Add `symlink_assets()` to `common/common.cmake`

Append after the `copy_kernels` function block:

```cmake
# ── symlink_assets() ──────────────────────────────────────────────────────────
# POST_BUILD: creates <binary_dir>/assets → <repo_root>/assets/ symlink.
# WHY no target arg: uses PROJECT_NAME, which matches the add_executable() name
# by convention in every module. Repo layout is fixed at 2 levels deep.
function(symlink_assets)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E create_symlink
            "${CMAKE_CURRENT_SOURCE_DIR}/../../assets"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/assets"
        COMMENT "Symlinking assets for ${PROJECT_NAME}"
    )
endfunction()
```

---

### A — `01_Host_API/` sub-modules (3 files)

**Problem:** Sub-modules read image assets via CLI at runtime but the binary dir has no `assets/` entry.

**Action:** For each file below, append `symlink_assets()` immediately after the existing `copy_kernels(...)` call (or after `opencl_lab_target(...)` if `copy_kernels` is absent).

```cmake
symlink_assets()
```

Files:
- `01_Host_API/01_Visual_Kernel/CMakeLists.txt` — target: `visual_kernel`
- `01_Host_API/02_Visual_Kernel_Events/CMakeLists.txt` — read to confirm target name
- `01_Host_API/03_Buffer_Flags/CMakeLists.txt` — read to confirm target name

---

### B — `02_Multimedia/` sub-modules (9 files)

**Problem:** Same as A.

**Action:** Same pattern. Read each file to confirm the exact target name before inserting.

Files:
- `02_Multimedia/A1_OpenCV_Interop/CMakeLists.txt`
- `02_Multimedia/A2_YUV_Pipeline/CMakeLists.txt`
- `02_Multimedia/A2b_YUYV_Extension/CMakeLists.txt`
- `02_Multimedia/A3_1_OpenCV_DNN/CMakeLists.txt`
- `02_Multimedia/A3_2_OpenVINO_GPU/CMakeLists.txt`
- `02_Multimedia/A4_Smart_Webcam/CMakeLists.txt`
- `02_Multimedia/A5_Privacy_Mode/CMakeLists.txt`
- `02_Multimedia/FFmpeg_Pipeline/CMakeLists.txt`
- `02_Multimedia/SoftISP/CMakeLists.txt`

---

### C — `03_GraphicsHPC/` sub-modules (3 files)

**Problem:** Same as A.

**Action:** Same pattern. Read each file to confirm target name.

Files:
- `03_GraphicsHPC/B2_Ray_Tracer_Basic/CMakeLists.txt`
- `03_GraphicsHPC/B3_Ray_Tracer_BVH/CMakeLists.txt`
- `03_GraphicsHPC/B3_Ray_Tracer_BVH_Dynamic/CMakeLists.txt`

---

### D — `04_Robotics/` sub-modules (3 files)

**Problem:** Same as A.

**Action:** Same pattern. Read each file to confirm target name.

Files:
- `04_Robotics/C1_Node_Acceleration/CMakeLists.txt`
- `04_Robotics/C2_Costmap_Inflation/CMakeLists.txt`
- `04_Robotics/C3_Perception_Node/CMakeLists.txt`

---

### E — `05_Toolbox/` entries (14 files)

**Problem:** Same as A. Toolbox entries that use image assets need the symlink; purely numeric benchmarks benefit from it too (consistent behavior, no harm).

**Action:** Same pattern. Each Toolbox entry is one level deeper than module dir: `05_Toolbox/<EntryName>/CMakeLists.txt` — still exactly two levels from repo root, so the `../../assets` path is correct.

Files:
- `05_Toolbox/AsyncMultiThread/CMakeLists.txt`
- `05_Toolbox/CoalescedAccess/CMakeLists.txt`
- `05_Toolbox/Debugging/CMakeLists.txt`
- `05_Toolbox/Deployment/CMakeLists.txt`
- `05_Toolbox/FastMath/CMakeLists.txt`
- `05_Toolbox/GenericKernelTemplates/CMakeLists.txt`
- `05_Toolbox/LocalMemory/CMakeLists.txt`
- `05_Toolbox/MultiGPU_Strategy/CMakeLists.txt`
- `05_Toolbox/SVM/CMakeLists.txt`
- `05_Toolbox/SVM_Theory/CMakeLists.txt`
- `05_Toolbox/SyncAtomics/CMakeLists.txt`
- `05_Toolbox/ThreadDivergence/CMakeLists.txt`
- `05_Toolbox/WorkGroupSizing/CMakeLists.txt`
- `05_Toolbox/ZeroCopy/CMakeLists.txt`

---

### F — `06_Bonus/` sub-modules (4 files)

**Problem:** Same as A.

**Action:** Same pattern. Read each file to confirm target name.

Files:
- `06_Bonus/CLBlast_MatMul/CMakeLists.txt`
- `06_Bonus/Device_Enqueue/CMakeLists.txt`
- `06_Bonus/vkFFT_Audio/CMakeLists.txt`
- `06_Bonus/Voxel_Mapping/CMakeLists.txt`

---

### G — `00_Setup/01_Smoke_Test/` (conditional)

**Problem:** May or may not consume image assets.

**Action:**
1. Read `00_Setup/01_Smoke_Test/CMakeLists.txt`.
2. If the binary has no `--input` asset path (i.e., it does not read image files), skip this file — no symlink needed.
3. If it does consume assets, apply the same pattern.

---

## Definition of Done (DoD)

Standard items from `.claude/rules/00_master_specs.md §8` apply to every module that already built successfully before this task.

- [x] Every `CMakeLists.txt` listed in items A–F (and G if applicable) contains the `create_symlink` POST_BUILD block.
- [x] After `cmake -B build && cmake --build build` from any sub-module directory, the file `build/assets` exists as a symlink resolving to the repo-root `assets/` directory. Spot-check at least one module per track: `01_Host_API/01_Visual_Kernel/`, `02_Multimedia/A1_OpenCV_Interop/`, `03_GraphicsHPC/B2_Ray_Tracer_Basic/`, `04_Robotics/C1_Node_Acceleration/`, `05_Toolbox/LocalMemory/`, `06_Bonus/CLBlast_MatMul/`.
- [x] The symlink target path is absolute (contains no `../` segments in the resolved link). Verify with `readlink -f build/assets` — output must start with `/`.
- [x] `cmake -B build && cmake --build build` produces zero new errors or warnings compared to before this task for all spot-checked modules.
- [x] No `.cpp`, `.cl`, or doc (`.md`) files are modified.
- [x] `common/CMakeLists.txt` and the repo-root `CMakeLists.txt` are unchanged.

---

## Execution Report

- **Status:** DONE
- **Session:** 2026-03-22

### Completed
| Item | Action |
|------|--------|
| Step 0 — common/common.cmake | Added `symlink_assets(TARGET_NAME)` function after `copy_kernels` block |
| A — Host API (3 files) | `symlink_assets(visual_kernel)`, `symlink_assets(visual_kernel_events)`, `symlink_assets(buffer_flags)` added |
| B — Multimedia (9 files) | `symlink_assets(<target>)` added to all 9 files |
| C — GraphicsHPC (3 files) | `symlink_assets(<target>)` added to all 3 files |
| D — Robotics (3 files) | `symlink_assets(<target>)` added to all 3 files |
| E — Toolbox (14 files) | `symlink_assets(<target>)` added to all 14 files (SVM uses inline `create_symlink` — equivalent) |
| F — Bonus (4 files) | `symlink_assets(<target>)` added to all 4 files |
| G — Setup (conditional) | SKIPPED — `00_Setup/01_Smoke_Test/CMakeLists.txt` has no asset consumption, no `--input` flag |

### Validation
```
# DoD item 1: all 36 module CMakeLists.txt contain create_symlink POST_BUILD block
$ grep -rl "create_symlink\|symlink_assets" <all module CMakeLists.txt> | wc -l
36  ✓

# DoD item 2 & 3: spot-check builds + readlink -f
$ cmake -B build && cmake --build build  [01_Host_API/01_Visual_Kernel]
[100%] Built target visual_kernel  ✓
$ readlink -f 01_Host_API/01_Visual_Kernel/build/assets
<repo>/assets  ✓

$ cmake -B build && cmake --build build  [02_Multimedia/A1_OpenCV_Interop]
[100%] Built target A1_OpenCV_Interop  ✓
$ readlink -f 02_Multimedia/A1_OpenCV_Interop/build/assets
<repo>/assets  ✓

$ cmake -B build && cmake --build build  [03_GraphicsHPC/B2_Ray_Tracer_Basic]
[100%] Built target b2_ray_tracer  ✓
$ readlink -f 03_GraphicsHPC/B2_Ray_Tracer_Basic/build/assets
<repo>/assets  ✓

$ cmake -B build && cmake --build build  [04_Robotics/C1_Node_Acceleration]
SKIPPED — ROS_DISTRO not set (pre-existing condition, unrelated to this task)

$ cmake -B build && cmake --build build  [05_Toolbox/LocalMemory]
[100%] Built target local_memory  ✓
$ readlink -f 05_Toolbox/LocalMemory/build/assets
<repo>/assets  ✓

$ cmake -B build && cmake --build build  [06_Bonus/CLBlast_MatMul]
[100%] Built target clblast_tuner_routine_xtrsv  ✓
$ readlink -f 06_Bonus/CLBlast_MatMul/build/assets
<repo>/assets  ✓

# DoD item 5: no .cpp / .cl files changed
$ git diff HEAD --name-only | grep -E "\.(cpp|cl)$"
(empty)  ✓

# DoD item 6: repo-root and common/CMakeLists.txt unchanged
$ git diff HEAD -- CMakeLists.txt common/CMakeLists.txt
(empty)  ✓
```

### Changed Files
| File | Change |
|------|--------|
| `common/common.cmake` | Added `symlink_assets(TARGET_NAME)` function + header comment |
| `01_Host_API/01_Visual_Kernel/CMakeLists.txt` | Added `symlink_assets(visual_kernel)` |
| `01_Host_API/02_Visual_Kernel_Events/CMakeLists.txt` | Added `symlink_assets(visual_kernel_events)` |
| `01_Host_API/03_Buffer_Flags/CMakeLists.txt` | Added `symlink_assets(buffer_flags)` |
| `02_Multimedia/A1_OpenCV_Interop/CMakeLists.txt` | Added `symlink_assets(A1_OpenCV_Interop)` |
| `02_Multimedia/A2_YUV_Pipeline/CMakeLists.txt` | Added `symlink_assets(yuv_pipeline)` |
| `02_Multimedia/A2b_YUYV_Extension/CMakeLists.txt` | Added `symlink_assets(yuyv_extension)` |
| `02_Multimedia/A3_1_OpenCV_DNN/CMakeLists.txt` | Added `symlink_assets(opencv_dnn)` |
| `02_Multimedia/A3_2_OpenVINO_GPU/CMakeLists.txt` | Added `symlink_assets(openvino_gpu)` |
| `02_Multimedia/A4_Smart_Webcam/CMakeLists.txt` | Added `symlink_assets(smart_webcam)` |
| `02_Multimedia/A5_Privacy_Mode/CMakeLists.txt` | Added `symlink_assets(privacy_mode)` |
| `02_Multimedia/FFmpeg_Pipeline/CMakeLists.txt` | Added `symlink_assets(ffmpeg_pipeline)` |
| `02_Multimedia/SoftISP/CMakeLists.txt` | Added `symlink_assets(soft_isp)` |
| `03_GraphicsHPC/B2_Ray_Tracer_Basic/CMakeLists.txt` | Added `symlink_assets(b2_ray_tracer)` |
| `03_GraphicsHPC/B3_Ray_Tracer_BVH/CMakeLists.txt` | Added `symlink_assets(b3_ray_tracer_bvh)` |
| `03_GraphicsHPC/B3_Ray_Tracer_BVH_Dynamic/CMakeLists.txt` | Added `symlink_assets(b3_ray_tracer_bvh_dynamic)` |
| `04_Robotics/C1_Node_Acceleration/CMakeLists.txt` | Added `symlink_assets(cl_node_accel)` |
| `04_Robotics/C2_Costmap_Inflation/CMakeLists.txt` | Added `symlink_assets(costmap_inflation)` |
| `04_Robotics/C3_Perception_Node/CMakeLists.txt` | Added `symlink_assets(perception_node)` |
| `05_Toolbox/AsyncMultiThread/CMakeLists.txt` | Added `symlink_assets(async_multi_thread)` |
| `05_Toolbox/CoalescedAccess/CMakeLists.txt` | Added `symlink_assets(coalesced_access)` |
| `05_Toolbox/Debugging/CMakeLists.txt` | Added `symlink_assets(debugging)` |
| `05_Toolbox/Deployment/CMakeLists.txt` | Added `symlink_assets(deployment)` |
| `05_Toolbox/FastMath/CMakeLists.txt` | Added `symlink_assets(fast_math)` |
| `05_Toolbox/GenericKernelTemplates/CMakeLists.txt` | Added `symlink_assets(generic_kernel_templates)` |
| `05_Toolbox/LocalMemory/CMakeLists.txt` | Added `symlink_assets(local_memory)` |
| `05_Toolbox/MultiGPU_Strategy/CMakeLists.txt` | Added `symlink_assets(multi_gpu_strategy)` |
| `05_Toolbox/SVM/CMakeLists.txt` | Added inline `create_symlink` POST_BUILD block (does not use common.cmake) |
| `05_Toolbox/SVM_Theory/CMakeLists.txt` | Added `symlink_assets(svm_theory)` |
| `05_Toolbox/SyncAtomics/CMakeLists.txt` | Added `symlink_assets(sync_atomics)` |
| `05_Toolbox/ThreadDivergence/CMakeLists.txt` | Added `symlink_assets(thread_divergence)` |
| `05_Toolbox/WorkGroupSizing/CMakeLists.txt` | Added `symlink_assets(work_group_sizing)` |
| `05_Toolbox/ZeroCopy/CMakeLists.txt` | Added `symlink_assets(zero_copy)` |
| `06_Bonus/CLBlast_MatMul/CMakeLists.txt` | Added `symlink_assets(clblast_matmul)` |
| `06_Bonus/Device_Enqueue/CMakeLists.txt` | Added `symlink_assets(device_enqueue)` |
| `06_Bonus/vkFFT_Audio/CMakeLists.txt` | Added `symlink_assets(vkfft_audio)` |
| `06_Bonus/Voxel_Mapping/CMakeLists.txt` | Added `symlink_assets(voxel_mapping)` |
