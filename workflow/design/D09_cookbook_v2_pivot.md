# D — Cookbook v2.0 Pivot

## Goal

Reorganize the Applied OpenCL Lab repository from a linear, module-numbered layout into a Hub & Spoke Cookbook format. The result is a structure where foundations (Parts 0–1) feed three independent specialization tracks (Parts 2–4), a shared Toolbox (Part 5), and a standalone Bonus section (Part 6). Each sub-module is independently runnable without sibling context.

## Non-goals

- No new kernel implementations beyond the `global_work_offset` & Tiled Benchmark Toolbox entry.
- No changes to kernel (`.cl`) or host (`.cpp`) logic inside existing modules — content moves only.
- No Windows or macOS support: scope is Linux (Ubuntu 24.04) only.
- No dynamic asset discovery or runtime path resolution beyond the POST_BUILD symlink approach.
- No renaming of sub-module source files beyond what is required by folder renames.

---

## Roadmap / Status

- [x] Phase 1: Folder Restructuring — Rename and split top-level directories to match the v2.0 layout.
- [x] Phase 2: Content Migration — Move recipes between tracks as specified; update internal CMake target names and include paths.
- [x] Phase 3: README Unification — Split fat single-file READMEs (Multimedia, GraphicsHPC, Robotics, Host API) into the two-level index + sub-module doc structure.
- [x] Phase 4: CMake POST_BUILD Assets Symlink — Add `create_symlink` command to every module's `CMakeLists.txt`.
- [x] Phase 5: New Toolbox Entry — Add `global_work_offset` & Tiled Benchmark recipe under `05_Toolbox/`.
- [~] Phase 6: Protocol Rename — Rename existing task and design files to the `T<id>_*.md` / `D<id>_*.md` format; update MEMORY.md references. CANCELLED - v1 files doesn't need update.
- [x] Phase 7: Final Verification — Verify all modules build from their standalone directory; verify docs cross-links; update progress.sh script; update progress counters in MEMORY.md.

---

## Specifications

> **Inherits**: `.claude/rules/00_master_specs.md`

Additional constraints specific to this pivot:

- **Platform**: Linux / Ubuntu 24.04. No multi-platform compatibility work is in scope.
- **Fixed Structure**: The directory layout defined below is canonical. Users are not expected to move folders; relative path counting is eliminated by the POST_BUILD symlink.
- **Duplication Policy**: Independent runnable recipes may share boilerplate (CLI setup, context init). This is intentional and acceptable — it supports isolated experimentation.

---

## Architecture (high-level)

### Target Directory Layout

```
opencl-lab/
├── 00_Setup/
├── 01_Host_API/
│   └── <sub-modules>/          # existing content, READMEs unified (Phase 5)
├── 02_Multimedia/               # was: 02_Projects/A_Multimedia
│   ├── Multimedia.md            # Module index (~50 lines)
│   └── <SubName>/<SubName>.md  # per-recipe self-contained docs
├── 03_GraphicsHPC/              # was: 02_Projects/B_Graphics_HPC (minus CLBlast & DeviceEnqueue)
│   ├── GraphicsHPC.md
│   └── <SubName>/<SubName>.md
├── 04_Robotics/                 # was: 02_Projects/C_Robotics_ROS2
│   ├── Robotics.md
│   └── <SubName>/<SubName>.md
├── 05_Toolbox/                  # was: 99_Toolbox (plus OpenCL-vs-CUDA, SVM Theory, Deployment)
│   ├── Toolbox.md
│   ├── GlobalWorkOffset/        # NEW — Tiled Benchmark recipe
│   └── <existing entries>/
└── 06_Bonus/                    # was: 04_Addons (minus FFmpeg & SoftISP; plus CLBlast & DeviceEnqueue)
    ├── Bonus.md
    └── <SubName>/<SubName>.md
```

### Content Migration Map

| Recipe | v1 Location | v2 Location |
|---|---|---|
| FFmpeg Pipeline | `04_Addons/` | `02_Multimedia/` |
| SoftISP (Debayering) | `04_Addons/` | `02_Multimedia/` |
| CLBlast | `02_Projects/B_Graphics_HPC/` | `06_Bonus/` |
| Device Enqueue | `02_Projects/B_Graphics_HPC/` | `06_Bonus/` |
| OpenCL vs CUDA | `04_Addons/` (Bonus) | `05_Toolbox/` |
| SVM Theory | `04_Addons/` (Bonus) | `05_Toolbox/` |
| Deployment | `04_Addons/` (Bonus) | `05_Toolbox/` |
| global_work_offset & Tiled Benchmark | — (new) | `05_Toolbox/GlobalWorkOffset/` |

### README Two-Level Structure

**Module index** (`<ModuleName>.md`):
- ~40–60 lines.
- Common prerequisites and hardware limitations for the whole track.
- Navigation table: columns = Sub-module name, goal summary, doc link.
- Does NOT duplicate sub-module build instructions.

**Sub-module doc** (`<SubName>/<SubName>.md`):

Track / Bonus template sections (in order):
1. `# N.M — Name`
2. Goal (1–2 sentences)
3. Prerequisites (delta from module index only)
4. Build & Run
5. Verify (expected output / visual artifact description)
6. Key Concepts

Toolbox entry template sections (in order):
1. `# Tool Name`
2. Symptom (when to reach for this tool)
3. Prerequisites (delta)
4. Build & Run
5. Verify
6. (Optional) How it works

Each sub-module doc ends with a back-link to the Module index.

Custom file names are preserved (e.g., `LocalMemory.md`, `OpenCVInterop.md`). Files are NOT renamed to `README.md`.

### CMake POST_BUILD Assets Symlink

Every module's `CMakeLists.txt` gains:

```cmake
add_custom_command(TARGET <target> POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E create_symlink
          ${CMAKE_SOURCE_DIR}/assets
          $<TARGET_FILE_DIR:<target>>/assets
  COMMENT "Symlinking assets into binary dir"
)
```

Doc-facing path in all READMEs becomes:
```
./build/<binary> --input assets/sample.bmp
```

No depth-relative `../../../assets/` paths remain after Phase 4.

### New Toolbox Entry: `global_work_offset` & Tiled Benchmark

**Problem**: Processing a full 4K frame when only a small ROI requires computation wastes GPU threads. In-kernel masking via `if-else` still launches dead threads; CPU-side cropping wastes memory bandwidth.

**Solution**: Use `global_work_offset` and `global_work_size` in the `enqueueNDRangeKernel` call to launch threads exclusively over the target tile. No buffer copies, no kernel rewrites.

**Concepts covered**:
- How `get_global_id()` shifts when a non-zero offset is applied.
- Stride/Pitch: computing the correct linear buffer index when the thread's logical `(0,0)` maps to `(offset_x, offset_y)` in image memory.
- Boundary alignment: handling ROI tiles that do not align to `local_work_size`.

**Tiled Benchmark**:
- Profile 256×256 tile vs. full 4K frame using `cl::Event`.
- Report times in milliseconds (3 decimal places) to a structured console table.
- No BMP output required (numeric benchmark — console table is the artifact per master specs §3).

---

## Key Decisions and Rationale

1. **Split `02_Projects/` into three top-level directories**
   - Rationale: Hub & Spoke requires tracks to be first-class siblings, not nested sub-paths. A student cloning only `02_Multimedia/` must get a complete, self-contained track.

2. **FFmpeg + SoftISP move to Multimedia; CLBlast + Device Enqueue move to Bonus**
   - Rationale: FFmpeg and SoftISP are canonically video/camera pipeline recipes — they belong alongside other Multimedia content. CLBlast and Device Enqueue are advanced standalone techniques with no track dependency, making Bonus the correct home.

3. **OpenCL vs CUDA, SVM Theory, Deployment move to Toolbox**
   - Rationale: These are reference/diagnostic recipes, not track-specific projects. Toolbox is the shared reference section by design.

4. **Assets symlink via POST_BUILD instead of relative paths**
   - Rationale: Depth-relative paths (`../../../assets/`) are fragile and silently wrong if the binary is run from any directory other than the build root. A symlink inside the binary dir makes every module self-contained at runtime.

5. **Two-level README structure (index + sub-module doc)**
   - Rationale: Eliminates duplicated prerequisite blocks across recipes. The module index is the single source for common setup; sub-module docs contain only the delta. Toolbox and Bonus already follow this pattern — Multimedia, GraphicsHPC, Robotics, and Host API are brought into compliance.

6. **Custom doc filenames preserved, not renamed to README.md**
   - Rationale: Named files (`LocalMemory.md`) are more discoverable in editors and search results. `README.md` as a convention only works when you know the directory; a named file is self-labeling at any level.

7. **`T<id>_*.md` / `D<id>_*.md` naming protocol**
   - Rationale: Enables unambiguous shell filtering (`ls T*.md`, `ls D*.md`) and prevents accidental mixing of task and design files in the same directory listing.

---

## Known Issues / Risks

- **CMake `CMAKE_SOURCE_DIR` in standalone builds** (RESOLVED — Phase 4): `CMAKE_CURRENT_SOURCE_DIR`/../../assets` is used as the absolute symlink target in `symlink_assets()`. No `-DASSETS_DIR` flag is required.
- **`04_Robotics/01_Node_Acceleration` requires `ROS_DISTRO`**: This module skips configure/build when `ROS_DISTRO` is not set in the environment. Pre-existing condition; unrelated to the assets symlink work. No mitigation in scope.
- **Duplication as a side effect**: Independent runnable recipes may duplicate context-init boilerplate. This is accepted per the Non-goals and Duplication Policy above.
- **Linux-only scope**: Any recipe relying on ROS 2, V4L2, or `/dev/video*` has no cross-platform mitigation in scope.
- **`get_global_id()` absolute semantics with `global_work_offset`** (T056 finding): `get_global_id()` always returns the work-item index within the NDRange starting at 0, regardless of the offset set in `enqueueNDRangeKernel`. The offset only controls *which work-items are launched* (dispatch window). The kernel must add the offset manually (via `get_global_offset()` or host-passed `offset_x`/`offset_y` args) to compute the correct linear buffer address. This is a common misconception and is now documented in `GlobalWorkOffset.md`.
- **ROI tile `local_work_size` alignment requirement** (T056 finding): When using `global_work_offset`, the `global_work_size` must still be a multiple of `local_work_size`. If the ROI tile dimensions are not multiples of the chosen local size, the host must round up `global_work_size` to the next multiple. The kernel guard (`if (gid < count)`) handles the surplus threads. Failure to round up results in `CL_INVALID_WORK_GROUP_SIZE` at runtime.
- **`02_Multimedia/05_OpenVINO_GPU` and `02_Multimedia/06_Smart_Webcam` require OpenVINO SDK** (T057 finding): These two modules fail to build when the OpenVINO SDK is not installed. This is an expected optional dependency. No mitigation is in scope; confirmed via build sweep in T057.
- **T053 spec count discrepancy**: Task DoD stated "16 files" but items A–D sum to 7+3+3+2=15. The changed-files table confirms 15 files were fixed. The "16" in the DoD checkbox text was a typo in the task spec; no file was missed.

---

## Verification Criteria (replaces Performance Gate for structural work)

- [x] Top-level directory names match the target layout exactly.
- [x] All content moves completed with no orphaned files in old locations.
- [x] `cmake -B build && cmake --build build` passes with zero errors and zero warnings for every module from its standalone directory. (Exception: 05_OpenVINO_GPU and 06_Smart_Webcam require OpenVINO SDK — see Known Issues.)
- [x] No depth-relative `../../../assets/` paths remain in any README or doc file.
- [x] Every module's binary dir contains an `assets/` symlink after build.
- [x] All Module index docs are ≤60 lines; all sub-module docs include a back-link to the Module index.
- [x] `05_Toolbox/GlobalWorkOffset/` recipe builds and outputs a structured timing table.
- [~] `workflow/tasks/` contains no files using the old naming convention (without `T<id>_` prefix).
- [x] MEMORY.md progress counters updated to reflect v2.0 structure.

---

## Prerequisites

See [main README](../../README.md) for base environment requirements (OpenCL ICD, CMake 3.18+, C++17 toolchain).
