# D10: V2 Improvements Backlog

> **Purpose:** Cross-cutting maintenance backlog for the Applied OpenCL Lab v2.0. Collects improvement ideas before committing to tasks. Ideas graduate to Approved only after explicit sign-off.

## Goal

Eliminate naming inconsistencies, structural debt, and documentation gaps introduced during the v1 → v2 migration, so that all modules present a uniform, professional surface to students and contributors.

## Non-goals

- Adding new lab content or new OpenCL techniques.
- Changes to any `.cpp`, `.cl`, or kernel logic — this backlog is purely structural and documentary.
- Renumbering existing modules within `00_Setup/` and `01_Host_API/` (their submodule names are already correct and are left unchanged).
- Modifying downloaded binary assets (`*.onnx`, `*.obj`) — only synthetic/generated assets are in scope for documentation.

## Roadmap / Status

- [x] Phase 1: Master Spec Updates — Consolidate all `00_master_specs.md` changes derived from this backlog before executing dependent phases.
- [x] Phase 2: Unified CMake Target & Executable Names — Standardize all `add_executable` target names and `project()` strings across ~30 modules.
- [ ] Phase 3: Unified Submodule Naming Convention — Rename ~36 submodule directories to two-digit numeric prefix + `Title_Snake_Case`.
- [ ] Phase 4: Kernel Symlink (replace copy) — Replace `cmake -E copy_directory` POST_BUILD with `cmake -E create_symlink` across ~40 CMakeLists.txt files.
- [ ] Phase 5: Add Missing Bonus READMEs — Author student-facing READMEs for `06_Bonus/CLBlast_MatMul/` and `06_Bonus/Device_Enqueue/`.
- [ ] Phase 6: Mark submodules with HW/OpenCL version dependencies — Add dependency callouts to submodule READMEs and inline tags to index READMEs.
- [ ] Phase 7: Unify ROS2 parameters handling — Remove hand-rolled `--help` loop from `C3_Perception_Node`; add `ros2 param` usage hints to all three ROS 2 module READMEs; formally exempt ROS 2 modules from CLI11 DoD gate in master specs.
- [ ] Phase 8: Extend assets README with creation commands — Add `## Regenerating Assets` section to `assets/assets.md`; replace scattered per-module creation commands with links.
- [ ] Phase 9: Add scripts README — Document all four helper scripts in `scripts/README.md`.

**Status key:** All phases Approved — awaiting task creation.

## Specifications

> **Inherits**: `workflow/design/00_master_specs.md`

Phases in this backlog must not break the Standard Definition of Done (§8) for any existing module. All CMake changes must preserve standalone buildability (§1).

## Architecture (high-level)

### Components

- **Master Spec (Phase 1):** Consolidates all changes to `00_master_specs.md` that are prerequisites for later phases. Must complete before any phase that references the updated spec. Changes:
  - §1 POST_BUILD template — replace `cmake -E copy_directory` with `cmake -E create_symlink` (prerequisite for Phase 4).
  - §1 CLI11 requirement — add ROS 2 exemption clause; `declare_parameter`/`get_parameter` is the idiomatic equivalent (prerequisite for Phase 7).
  - §2 Naming conventions — codify `snake_case` executable, `PascalCase` project(), `NN_Title_Snake_Case` submodule directory rules.

- **CMake Naming (Phase 2):** Governs `add_executable` target names and `project()` declarations inside each module's `CMakeLists.txt`. ~22 executable renames, ~20 `project()` fixes; cascades to `./build/<name>` references in submodule READMEs.

- **Directory Naming (Phase 3):** Governs submodule folder names on disk. ~36 `git mv` operations across `02_Multimedia/`, `03_GraphicsHPC/`, `04_Robotics/`, `05_Toolbox/`, `06_Bonus/`. Cascades to all markdown links (index READMEs, submodule READMEs, `cd` commands, back-links, `workflow/design/*.md`, `.claude/rules/MEMORY.md`).

- **Build System (Phase 4):** Governs the POST_BUILD kernel-copy step in every `CMakeLists.txt`. Replaces file copy with directory symlink per updated §1 spec. Find kernels copy_directory usage and replace with common.cmake method.

- **Documentation — Bonus READMEs (Phase 5):** Two student-facing markdown files authored via `/create-readme` (@educator). Reference material lives on `main` branch in archived tasks and the old `GraphicsHPC.md`.

- **Documentation — Dependency Tags (Phase 6):** Inline tags added to top-level `README.md` and module index files; authoritative `> **Requires:**` callout blocks in individual submodule READMEs. No setup details duplicated between levels.

- **ROS 2 Spec Compliance (Phase 7):** Code change in `C3_Perception_Node/main.cpp` (delete lines 699–716). README updates in all three ROS 2 module READMEs (`NodeAcceleration.md`, `CostmapInflation.md`, `PerceptionNode.md`) — add a `## Inspecting Parameters` (or `Troubleshooting`) section with:
  - `ros2 param list /<node_name>` — list all declared parameters
  - `ros2 param describe /<node_name> <param>` — show type + description
  - `ros2 param get /<node_name> <param>` / `ros2 param set /<node_name> <param> <value>` — read/write at runtime
  Spec update (CLI11 exemption) handled in Phase 1.

- **Assets Documentation (Phase 8):** New `## Regenerating Assets` section in `assets/assets.md`. Per-module READMEs that embed creation commands are updated to link to the canonical section instead.

- **Scripts Documentation (Phase 9):** New `scripts/README.md` covering `build_all.sh`, `test_all.sh`, `progress.sh`, `gen_pgm.py`.

## Key Decisions (and Rationale)

1. **Executable names: `snake_case`, no module prefix, no `_demo` suffix**
   - **Why:** Module prefixes (`A1_`, `b2_`) are filesystem navigation aids, not binary identifiers. Stripping them makes `./build/<name>` commands consistent and copy-pasteable across modules.

2. **`project()` names: `PascalCase` matching the directory name**
   - **Why:** CMake convention; avoids stale project strings that diverge from directory names after renames.

3. **Multi-target modules: keep descriptive per-target names, same `snake_case` rule**
   - **Why:** `GenericKernelTemplates`, `C3_Perception_Node`, and `Voxel_Mapping` expose multiple binaries; a single generic name would be ambiguous.

4. **Submodule prefix: two-digit numeric, scoped per parent module**
   - **Why:** Consistent with the existing `00_`/`01_` pattern at module level. Gaps on add/remove are acceptable — renumbering is forbidden to avoid churn.

5. **Acronyms: ALLCAPS for well-known technical terms; preserve proper names**
   - **Why:** `SVM`, `BVH`, `YUV`, `ISP`, `DNN` are standard initialisms; `OpenCV`, `OpenVINO`, `CLBlast`, `VkFFT` are product/library names that must match their upstream spelling.

6. **Kernel symlink over copy**
   - **Why:** Single source of truth — editing a `.cl` file in source is immediately reflected in the build directory. The copy approach risks confusion when the wrong copy is edited and then silently overwritten on the next build.

7. **Dependency tags: two-level approach (authoritative callout in submodule README, lightweight inline tag in index)**
   - **Why:** Avoids duplicating setup instructions. The submodule README is the canonical place; index files carry only a terse label for discoverability.

8. **CLI11 exemption for ROS 2 modules (C1, C2, C3)**
   - **Why:** `ros2 param` / `declare_parameter` / `get_parameter` is the idiomatic ROS 2 equivalent of CLI11. The hand-rolled `--help` loop in C3 is a spec violation that also drifts out of sync when parameters change.

## Known Issues / Risks

- **Phase 1 must precede Phases 4 and 7:** Spec changes to `00_master_specs.md` (symlink template, ROS 2 CLI11 exemption) must land before the sweep phases that reference them.
- **Phase 2 → Phase 3 ordering dependency:** Executable renames (Phase 2) reference old directory names; directory renames (Phase 3) must happen after or atomically. Tasks should be sequenced Phase 2 first so README references can be updated in one sweep.
- **Phase 4 symlink portability:** `cmake -E create_symlink` requires the build directory to be on the same filesystem as the source. Cross-filesystem or container builds may fail. Task must document this constraint.
- **Phase 5 reference material on `main` branch:** Archived tasks `028_graphics_b1_clblast_matmul.md` and `033_graphics_b4_device_enqueue.md` exist only on `main`; the coder must check out or `git show` those files.
- **Phase 7 C1/C2 no action needed:** C1 and C2 have no hand-rolled parser — only C3 requires a code change.

## Performance Gate

N/A — this is a cross-cutting maintenance backlog, not a module with performance targets.

## Specifications & Standards

*Naming rules that all tasks in this backlog must enforce:*

- **Executable names:** `snake_case`, no module prefix (`A1_`, `b2_`, `c3_`), no `_demo` suffix.
- **`project()` names:** `PascalCase` matching the directory name exactly.
- **Submodule directory names:** `NN_Title_Snake_Case` where `NN` is a two-digit number scoped per parent module.
- **Acronyms in directory names:** ALLCAPS (`SVM`, `BVH`, `YUV`, `ISP`, `DNN`); proper names preserved (`OpenCV`, `OpenVINO`, `CLBlast`, `VkFFT`).
- **Rename maps:** Authoritative tables are in Phases 2 and 3 below. Do not deviate.

### Phase 2 Rename Map (CMake targets and project() names — changes only)

| Module | Old executable | New executable | Old project() | New project() |
|---|---|---|---|---|
| `01_Host_API/03_Buffer_Flags` | `buffers_layout_demo` | `buffer_flags` | `BuffersLayout` | `BufferFlags` |
| `02_Multimedia/A1_OpenCV_Interop` | `A1_OpenCV_Interop` | `opencv_interop` | `A1_OpenCV_Interop` | `OpenCVInterop` |
| `02_Multimedia/A2_YUV_Pipeline` | `A2_YUV_Pipeline` | `yuv_pipeline` | `A2_YUV_Pipeline` | `YUVPipeline` |
| `02_Multimedia/A2b_YUYV_Extension` | `A2b_YUYV_Extension` | `yuyv_extension` | `A2b_YUYV_Extension` | `YUYVExtension` |
| `02_Multimedia/A3_1_OpenCV_DNN` | `a3_1_opencv_dnn` | `opencv_dnn` | `a3_1_opencv_dnn` | `OpenCVDNN` |
| `02_Multimedia/A3_2_OpenVINO_GPU` | `a3_2_openvino_gpu` | `openvino_gpu` | `a3_2_openvino_gpu` | `OpenVINOGPU` |
| `02_Multimedia/A4_Smart_Webcam` | `smart_webcam` ✓ | — | `smart_webcam` | `SmartWebcam` |
| `02_Multimedia/A5_Privacy_Mode` | `privacy_mode` ✓ | — | `privacy_mode` | `PrivacyMode` |
| `02_Multimedia/FFmpeg_Pipeline` | `ffmpeg_opencl_transcoder` | `ffmpeg_pipeline` | `ffmpeg_opencl_transcoder` | `FFmpegPipeline` |
| `02_Multimedia/SoftISP` | `softisp` | `soft_isp` | `softisp` | `SoftISP` |
| `03_GraphicsHPC/B2_Ray_Tracer_Basic` | `b2_ray_tracer` | `ray_tracer` | `B2_Ray_Tracer_Basic` | `RayTracerBasic` |
| `03_GraphicsHPC/B3_Ray_Tracer_BVH` | `b3_ray_tracer_bvh` | `ray_tracer_bvh` | `B3_Ray_Tracer_BVH` | `RayTracerBVH` |
| `03_GraphicsHPC/B3_Ray_Tracer_BVH_Dynamic` | `b3_ray_tracer_dynamic` | `ray_tracer_bvh_dynamic` | `B3_Ray_Tracer_BVH_Dynamic` | `RayTracerBVHDynamic` |
| `04_Robotics/C1_Node_Acceleration` | `accel_node` | `node_acceleration` | `c1_node_acceleration` | `NodeAcceleration` |
| `04_Robotics/C2_Costmap_Inflation` | `costmap_node` | `costmap_inflation` | `c2_costmap_inflation` | `CostmapInflation` |
| `04_Robotics/C3_Perception_Node` | `perception_node` ✓ | — | `c3_perception_node` | `PerceptionNode` |
| `05_Toolbox/AsyncMultiThread` | `async_demo` | `async_multi_thread` | `AsyncMultiThread` ✓ | — |
| `05_Toolbox/Debugging` | `debug_demo` | `debugging` | `debug_demo` | `Debugging` |
| `05_Toolbox/Deployment` | `deployment_demo` | `deployment` | `deployment_demo` | `Deployment` |
| `05_Toolbox/FastMath` | `fast_math` ✓ | — | `fast_math` | `FastMath` |
| `05_Toolbox/GenericKernelTemplates` | `01_basic_mad` etc. ✓ | — | `generic_kernel_templates` | `GenericKernelTemplates` |
| `05_Toolbox/MultiGPU_Strategy` | `multigpu_strategy` | `multi_gpu_strategy` | `MultiGPU_Strategy` | `MultiGPUStrategy` |
| `05_Toolbox/SVM` | `svm_demo` | `svm` | `SVM` ✓ | — |
| `05_Toolbox/SVM_Theory` | `svm_deep_dive` | `svm_theory` | `svm_deep_dive` | `SVMTheory` |
| `05_Toolbox/SyncAtomics` | `sync_atomics` ✓ | — | `sync_atomics` | `SyncAtomics` |
| `05_Toolbox/WorkGroupSizing` | `workgroup_sizing` | `work_group_sizing` | `WorkGroupSizing` ✓ | — |
| `06_Bonus/CLBlast_MatMul` | `b1_clblast_matmul` | `clblast_matmul` | `B1_CLBlast_MatMul` | `CLBlastMatMul` |
| `06_Bonus/Device_Enqueue` | `b4_device_enqueue` | `device_enqueue` | `b4_device_enqueue` | `DeviceEnqueue` |
| `06_Bonus/vkFFT_Audio` | `vkfft_audio` ✓ | — | `vkfft_audio` | `VkFFTAudio` |
| `06_Bonus/Voxel_Mapping` | `voxel_mapping` ✓ | — | `voxel_mapping_addon` | `VoxelMapping` |

### Phase 3 Rename Map (submodule directories)

| Old | New |
|---|---|
| `02_Multimedia/A1_OpenCV_Interop` | `01_OpenCV_Interop` |
| `02_Multimedia/A2_YUV_Pipeline` | `02_YUV_Pipeline` |
| `02_Multimedia/A2b_YUYV_Extension` | `03_YUYV_Extension` |
| `02_Multimedia/A3_1_OpenCV_DNN` | `04_OpenCV_DNN` |
| `02_Multimedia/A3_2_OpenVINO_GPU` | `05_OpenVINO_GPU` |
| `02_Multimedia/A4_Smart_Webcam` | `06_Smart_Webcam` |
| `02_Multimedia/A5_Privacy_Mode` | `07_Privacy_Mode` |
| `02_Multimedia/FFmpeg_Pipeline` | `08_FFmpeg_Pipeline` |
| `02_Multimedia/SoftISP` | `09_SoftISP` |
| `03_GraphicsHPC/B2_Ray_Tracer_Basic` | `01_Ray_Tracer_Basic` |
| `03_GraphicsHPC/B3_Ray_Tracer_BVH` | `02_Ray_Tracer_BVH` |
| `03_GraphicsHPC/B3_Ray_Tracer_BVH_Dynamic` | `03_Ray_Tracer_BVH_Dynamic` |
| `04_Robotics/C1_Node_Acceleration` | `01_Node_Acceleration` |
| `04_Robotics/C2_Costmap_Inflation` | `02_Costmap_Inflation` |
| `04_Robotics/C3_Perception_Node` | `03_Perception_Node` |
| `05_Toolbox/AsyncMultiThread` | `16_Async_Multi_Thread` |
| `05_Toolbox/CoalescedAccess` | `02_Coalesced_Access` |
| `05_Toolbox/Debugging` | `03_Debugging` |
| `05_Toolbox/Deployment` | `04_Deployment` |
| `05_Toolbox/FastMath` | `05_Fast_Math` |
| `05_Toolbox/GenericKernelTemplates` | `06_Generic_Kernel_Templates` |
| `05_Toolbox/GlobalWorkOffset` | `07_Global_Work_Offset` |
| `05_Toolbox/LocalMemory` | `01_Local_Memory` |
| `05_Toolbox/MultiGPU_Strategy` | `08_Multi_GPU_Strategy` |
| `05_Toolbox/OpenCL_vs_CUDA` | `09_OpenCL_vs_CUDA` |
| `05_Toolbox/SVM` | `10_SVM` |
| `05_Toolbox/SVM_Theory` | `11_SVM_Theory` |
| `05_Toolbox/SyncAtomics` | `12_Sync_Atomics` |
| `05_Toolbox/ThreadDivergence` | `13_Thread_Divergence` |
| `05_Toolbox/WorkGroupSizing` | `14_Work_Group_Sizing` |
| `05_Toolbox/ZeroCopy` | `15_Zero_Copy` |
| `06_Bonus/CLBlast_MatMul` | `01_CLBlast_MatMul` |
| `06_Bonus/Device_Enqueue` | `02_Device_Enqueue` |
| `06_Bonus/vkFFT_Audio` | `03_VkFFT_Audio` |
| `06_Bonus/Voxel_Mapping` | `04_Voxel_Mapping` |
| `00_Setup/01_Smoke_Test` | unchanged |
| `01_Host_API/01_Visual_Kernel` | unchanged |
| `01_Host_API/02_Visual_Kernel_Events` | unchanged |
| `01_Host_API/03_Buffer_Flags` | unchanged |

### Phase 6 Dependency Tag Map

| Submodule | Tag |
|---|---|
| `A3_2_OpenVINO_GPU`, `A4_Smart_Webcam` | `*(Intel OpenVINO SDK)*` |
| `SVM`, `SVM_Theory` | `*(OpenCL 2.0)*` |
| `OpenCL_vs_CUDA` | `*(NVIDIA GPU + CUDA)*` |
| `Device_Enqueue` | `*(OpenCL 2.0)*` |
| `B2_Ray_Tracer_Basic`, `B3_Ray_Tracer_BVH`, `B3_Ray_Tracer_BVH_Dynamic` | `*(cl_khr_gl_sharing)*` |

### Phase 8 Assets Scope

Synthetic assets to document in `assets/assets.md#regenerating-assets`:
- `sample.bmp`, `sample_1080p.bmp`, `rgb_4k.bmp` — ffmpeg `testsrc`/`testsrc2` commands
- `sample_nv12*.yuv`, `sample_yuyv*.yuv` — ffmpeg `-pix_fmt` pipeline
- `sample.mp4` — ffmpeg lavfi command (currently only in `FFmpegPipeline.md`)
- `raw_bayer_4k.raw` — python3/ffmpeg channel-selection script
- `warehouse.pgm` — `python3 scripts/gen_pgm.py` invocation
- `test_440hz.wav`, `test_1024frames.wav` — generation commands

Downloaded assets (`*.onnx`, `*.obj`) are out of scope — link to sources already in the existing table.

### Phase 9 Scripts Scope

`scripts/README.md` must cover:
- `build_all.sh` — purpose, usage, expected output
- `test_all.sh` — purpose, usage, expected output
- `progress.sh` — what it tracks, how to read the output
- `gen_pgm.py` — what it generates, when to use it

## Prerequisites

- Existing lab structure as defined in `.claude/rules/MEMORY.md` (v2.0 directory layout).
- `workflow/design/00_master_specs.md` — normative reference for CMake, naming, and DoD rules.
- `workflow/templates/task_doc_template.md` — used when graduating any phase to a task.
- For Phase 4: access to `main` branch archived tasks `028_graphics_b1_clblast_matmul.md` and `033_graphics_b4_device_enqueue.md`, and `02_Projects/B_Graphics_HPC/GraphicsHPC.md`.
