# D12: v2.2 Improvements Backlog

> **Purpose:** Cross-cutting improvement backlog for the Applied OpenCL Lab v2.2. Collects and tracks all changes before committing to tasks. Ideas graduate to Approved only after explicit sign-off.

## Goal

Consolidate the Optimization Toolbox by filling the vacant slot 10 with Sub-Buffers, nest advanced hardware edge cases into existing tools as challenges, integrate "Silicon Realities" lessons into project modules (Path B / Path C), and run the deferred Grading Pass from v2.1.

## Non-goals

- Renumbering existing module slots (gaps are acceptable per `00_master_specs.md` §2).
- Changes already completed under D10 or D11.
- Source-code modifications to `.cpp`, `.cl`, or `CMakeLists.txt` files during documentation-only phases.

## Roadmap / Status

- [x] Phase 1: Sub-Buffers Tool — Create `05_Toolbox/10_Sub_Buffers_Partitioning/` with README and implementation; update `05_Toolbox/Toolbox.md` table and root `README.md` Toolbox section.
- [x] Phase 2: Toolbox Advanced Challenges — Nest AoS/SoA into `02_Coalesced_Access`, Bank Conflicts into `01_Local_Memory`, Register Pressure into `14_Work_Group_Sizing`; update each submodule README.
- [x] Phase 3: Silicon Realities Lessons — Add `float3` alignment trap and hardware-safe struct lessons to Path B (Ray Tracer) and Path C (Robotics) project module READMEs.
- [ ] Phase 4: Grading Pass — Run `/grade-module` on all 7 top-level modules and key submodules; produce `grade_report_v2.md`; human triage actionable items. *(Carried forward from D11 Phase 4.)*

---

## Phase 1 Detail: Sub-Buffers Tool

### Architecture

New submodule `05_Toolbox/10_Sub_Buffers_Partitioning/`:

- **`SubBuffers.md`**: Self-contained submodule README following the Toolbox entry template (`# Tool Name` → Symptom, Prerequisites (delta), Build & Run, Verify). Back-links to `Toolbox.md`.
- **`main.cpp`**: Demonstrates `cl::Buffer::createSubBuffer` — single large parent buffer partitioned into N sub-buffers; each sub-buffer dispatched to a separate kernel invocation (or queue) with zero host-side `memcpy`.
- **`kernels/sub_buffer_demo.cl`**: Operates on a sub-buffer region; demonstrates that `get_global_id(0)` is relative to the sub-buffer origin, not the parent.
- **`CMakeLists.txt`**: Standalone buildable; uses `common.cmake` + `opencl_lab_target()`.

**`05_Toolbox/Toolbox.md` update:** Add row:

```
| [Sub-Buffers](10_Sub_Buffers_Partitioning/SubBuffers.md) | VRAM limits exceeded / High CPU overhead from manual `memcpy` chunking | `10_Sub_Buffers_Partitioning/` |
```

**Root `README.md` update:** Add `10_Sub_Buffers_Partitioning` entry to the Toolbox section table (mirrors the `Toolbox.md` row).

### Key Decisions

1. **Slot 10 assignment** — Slot 10 was vacated by the D11 Phase 1 `10_SVM` archive. Sub-Buffers occupies it as the mechanical prerequisite for out-of-core streaming, `08_Multi_GPU_Strategy`, and `16_Async_Multi_Thread`. No renumbering of other tools.
2. **Zero data movement constraint** — The demo must prove the zero-copy guarantee: parent buffer allocated once; sub-buffer handles aliased from it; no `enqueueWriteBuffer` of sub-regions.
3. **Visual artifact** — BMP output required per §3 of `00_master_specs.md`. Each sub-buffer processes a horizontal image strip; final BMP assembles strips to confirm correct partitioning.

### Definition of Done

- [ ] `cmake -B build && cmake --build build` succeeds with zero errors and zero warnings.
- [ ] Binary runs without arguments and completes without error; outputs `output.bmp` showing partitioned sub-buffer strips.
- [ ] `--help` prints CLI11-generated usage.
- [ ] `GPU=<vendor> ./build/<bin>` selects the correct device without crashing.
- [ ] `CL_DEVICE_MEM_BASE_ADDR_ALIGN` is queried at runtime; no hardcoded alignment values present.
- [ ] `05_Toolbox/Toolbox.md` table updated with the Sub-Buffers row.
- [ ] Root `README.md` Toolbox section updated with the Sub-Buffers entry.
- [ ] `SubBuffers.md` README reviewed and approved by `@educator` via `/test-ux` pass — no HIGH UX friction items unresolved.

### Known Issues / Risks

- **`CL_MISALIGNED_SUB_BUFFER_OFFSET`**: Sub-buffer origin must be a multiple of `CL_DEVICE_MEM_BASE_ADDR_ALIGN`. The demo must query this at runtime and align partitions accordingly — hardcoded 64-byte alignment is FORBIDDEN.
- **OpenCL 1.2 compatibility**: `createSubBuffer` is a 1.2 feature; no version guard needed. Coarse-grained SVM is not required for this tool.

---

## Phase 2 Detail: Toolbox Advanced Challenges

Three existing Toolbox submodules receive an **Advanced Challenge** section in their READMEs. No source files are modified; documentation only.

### 2a — Bank Conflicts → `01_Local_Memory/LocalMemory.md`

Add section: *Advanced Challenge: Bank Conflicts in LDS*

- Explain 32-bank LDS architecture; show stride-`N` access pattern that hits the same bank for all threads.
- Show `+1` column padding fix (`__local float tile[ROWS][COLS + 1]`).
- Tie back to the tile-based convolution already in the module.
- Updated Toolbox.md symptom text: *"Kernel re-reads same global data repeatedly (Incl. Bank Conflicts)"*.

### 2b — AoS vs. SoA → `02_Coalesced_Access/CoalescedAccess.md`

Add section: *Advanced Challenge: AoS vs. SoA Data Layout*

- Show C++ `struct Particle { float x, y, z, w; }` Array-of-Structures layout; trace how thread `N` reading `particles[N].x` creates stride-4 access, wasting 75% of the 128-byte cache line.
- Show Structure-of-Arrays refactor (`float* xs, *ys, *zs, *ws`) achieving contiguous per-attribute reads.
- Benchmark kernel: same computation, AoS vs. SoA, `cl::Event` timing.
- Updated Toolbox.md symptom text: *"Kernel slow despite simple logic (Incl. AoS vs. SoA)"*.

### 2c — Register Pressure → `14_Work_Group_Sizing/WorkGroupSizing.md`

Add section: *Advanced Challenge: Register Pressure & Spilling*

- Explain register file budget; show a kernel deliberately overloaded with `private` variables.
- Show how to use `-cl-nv-verbose` (Nvidia) and equivalent AMD/Intel build flags to observe `ptxas` register/spill output.
- Connect to occupancy: high register count → fewer concurrent warps → lower GPU utilization.
- Updated Toolbox.md symptom text: *"GPU underutilized, low occupancy (Incl. Register Pressure)"*.

---

## Phase 3 Detail: Silicon Realities Lessons

Two inline **"Stop and Read"** engineering lessons added to project module READMEs. Documentation only; no source changes.

### 3a — `float3` Alignment Trap → Path B Ray Tracer README(s)

Location: `03_GraphicsHPC/` — insert in the submodule README covering the primary Ray Tracer (`02_Ray_Tracer_Basic` or `03_BVH_Ray_Tracer`).

Content:

- OpenCL aligns `float3` to 16 bytes (same as `float4`). Struct layouts with `float3` members produce invisible 4-byte padding holes.
- AMD driver `NaN` generation on `normalize()` of zero-length `float3` — defensive `dot3` helpers required.
- Enforce custom `dot3(a, b)` = `a.x*b.x + a.y*b.y + a.z*b.z` rather than `dot(a, b)` on `float3` operands.
- Callout box format: `> **Stop and Read: The float3 Alignment Trap**`.

### 3b — Hardware-Safe C++ Structs → Path C Robotics README(s)

Location: `04_Robotics/` — insert in the submodule README covering the Perception Node or Costmap module where host-device struct sharing is explained.

Content:

- Mandatory `int pad[N]` fields to satisfy device alignment rules.
- Mandatory `static_assert(sizeof(MyStruct) == EXPECTED_SIZE)` on host — catches ABI drift at compile time, not at runtime with silent corruption.
- Callout box format: `> **Stop and Read: Hardware-Safe C++ Structs**`.

---

## Phase 4 Detail: Grading Pass *(Carried from D11)*

> **Status:** Approved for v2.2.

### Grading Scope

Run `/grade-module` on all 7 top-level modules. For modules with multiple distinct submodules, grade each submodule independently (the `@evaluator` reads the submodule README + source files):

1. `00_Setup/`
2. `01_Host_API/` — grade each of the 3 submodules
3. `02_Multimedia/` — grade each of the 9 submodules
4. `03_GraphicsHPC/` — grade each of the 3 submodules
5. `04_Robotics/` — grade each of the 3 submodules
6. `05_Toolbox/` — grade each submodule (15 slots, slot 10 filled by Phase 1)
7. `06_Bonus/` — grade each of the 4 submodules

### Protocol

**A — Per-submodule `/grade-module` pass:** Run `@evaluator` on each submodule directory using the `grading` skill. Collect the strict output template (scores table + justification + actionable items) verbatim for each submodule.

**B — Aggregate report:** Write all findings to `workflow/tasks/grade_report_v2.md`. Structure:

```markdown
# Grading Report v2

## Merged Scores

| Module | Submodule | Theory/App | Uniqueness | HW Dep. | Repetitiveness | Clarity | Reproducibility | **FINAL SCORE** |
|--------|-----------|------------|------------|---------|----------------|---------|-----------------|-----------------|
| 00_Setup | — | x/10 | x/10 | x/10 | x/10 | x/10 | x/10 | **x/10** |
| 01_Host_API | 01_Visual_Kernel | ... | | | | | | |
| ... | | | | | | | | |

## Actionable Items

### [Module / Submodule]
| # | Issue | Criterion | Severity | Proposed Fix | Fix? |
|---|-------|-----------|----------|--------------|------|
```

Severity derived from score gap: criterion score ≤ 5 → HIGH, 6–7 → MED, 8+ → LOW/skip.

**C — MANUAL: Human triage:** Human ticks `[x]` in the `Fix?` column. Items typically feed back into Phase 2 (README fixes) or future design backlog entries.

**D — No auto-fixes in this phase.** Grading is read-only. Selected action items are graduated to tasks or appended to Phase 2/3 fix lists.

### Grading Criteria Reference

Per `grading` skill (`SKILL.md`):

| Criterion | What it measures |
| --- | --- |
| Theory/App | Just-in-Time theory placement vs. dry theory dump |
| Uniqueness | Value vs. generic online OpenCL tutorials |
| HW Dep. | Portability; graceful OpenCL 1.2 fallback |
| Repetitiveness | README conciseness; no code-comment parroting |
| Clarity | WHY-focused comments; architectural reasoning |
| Reproducibility | Self-contained CMake build; no environment hell |

### Grading Risks

- **Strict output template must be reproduced verbatim** — do not reformat or summarize evaluator output. Append raw table + justification for each submodule, then extract actionable items into the triage table.
- **Phase 1 dependency:** Grade `10_Sub_Buffers_Partitioning` only after D12 Phase 1 is complete (slot 10 vacant until then).
- **Large scope:** ~40 submodules. Run `@evaluator` agents in parallel (up to 3 at a time) to keep wall-clock time reasonable.

### Grading Prerequisites

- All submodule READMEs and source files in v2.0 paths.
- `workflow/design/00-executive-summary.md` — `@evaluator` reads this for project vision.
- `workflow/design/00_master_specs.md` — normative reference for design decisions and known limitations.

---

## Architecture (high-level)

### Components

- **`05_Toolbox/10_Sub_Buffers_Partitioning/`**: New self-contained tool submodule (Phase 1).
- **Three existing Toolbox READMEs**: `LocalMemory.md`, `CoalescedAccess.md`, `WorkGroupSizing.md` — Advanced Challenge sections appended (Phase 2).
- **Two project-track READMEs**: `03_GraphicsHPC/` Ray Tracer + `04_Robotics/` Perception — Silicon Realities callout boxes inserted (Phase 3).
- **`workflow/tasks/grade_report_v2.md`**: Aggregate grading output artifact (Phase 4).

### Data Flow

1. Phase 1 delivers a buildable sub-buffer demo; slot 10 is populated in `Toolbox.md`.
2. Phase 2 enriches existing tool READMEs with advanced content; no new binaries.
3. Phase 3 enriches project READMEs with inline engineering lessons; no new binaries.
4. Phase 4 reads all submodule READMEs + source files, produces the grading report, feeds actionable items back into documentation fix lists.

## Key Decisions

1. **Phase ordering** — Phase 1 (Sub-Buffers) runs before Phase 4 (Grading) so that the new tool is graded in the same pass as the rest of the Toolbox.
2. **Documentation-only for Phases 2, 3, 4** — No `.cpp`, `.cl`, or `CMakeLists.txt` files are modified; preserves Standard DoD compliance for all existing modules without requiring re-validation.
3. **Advanced Challenges nested, not standalone** — Keeping Bank Conflicts, AoS/SoA, and Register Pressure inside existing tools avoids Toolbox bloat and respects the Hub & Spoke philosophy from the Executive Summary.
4. **Silicon Realities as callout boxes** — Inline `> **Stop and Read:**` format prevents these lessons from becoming isolated theory dumps disconnected from the project context where they are encountered.
5. **Grading deferred from D11** — Phase 4 was deferred because Phases 1–3 (new/updated content) must be stable before grading; grading unstable content produces noise, not signal.

## Known Issues / Risks

- **`CL_MISALIGNED_SUB_BUFFER_OFFSET` portability**: Runtime alignment query is mandatory in Phase 1 implementation — hardcoded values will silently fail on some hardware.
- **Toolbox slot 10 vacancy**: Phase 1 must not shift any other tool's numbering. Verify `Toolbox.md` index table does not reorder rows.
- **Grading scope (~40 submodules)**: Wall-clock time for Phase 4 is significant. Parallel `@evaluator` invocations (max 3) are expected.
- **Phase 3 AMD NaN note**: The `normalize()` / `float3` zero-vector behavior is AMD-specific. The lesson must be framed as a hardware reality check, not a spec bug, to avoid misleading students on other platforms.
- **Phase 1 open human gates (2026-03-28)**: `@educator /test-ux` pass on `SubBuffers.md` and MANUAL visual inspection of `output.bmp` are deferred. All automated DoD items pass on Intel Iris Xe. These must be resolved before Phase 4 grading of `10_Sub_Buffers_Partitioning`.
- **Phase 2 Advanced Challenge formatting (2026-03-28)**: Task spec mandated `> **Advanced Challenge: <Title>**` blockquote callout format. Implementation used plain `## Advanced Challenge: <Title>` headings instead to match the existing heading style of each submodule README. Plain headings were retained as they fit the surrounding document structure better than nested blockquotes.

## Performance Gate

N/A — this is a maintenance and enrichment backlog, not a module with runtime performance targets.

## Specifications & Standards

> **Inherits**: `workflow/design/00_master_specs.md`

- **Phase 1 binary**: Must satisfy Standard DoD §8 (build clean, runs without args, `--help`, `GPU=<vendor>` selection).
- **Phase 1 visual artifact**: BMP output showing partitioned sub-buffer strips required per §3.
- **Phases 2, 3**: Documentation edits only — zero `.cpp`/`.cl`/`CMakeLists.txt` modifications.
- **Phase 4**: No auto-fixes. `grade_report_v2.md` is the sole output artifact.
- **Sub-buffer alignment**: `CL_DEVICE_MEM_BASE_ADDR_ALIGN` queried at runtime; never hardcoded.

## Prerequisites

See [main README](../README.md) for base requirements.
- D11 Phases 1–3 complete (v2.1 baseline stable).
- `05_Toolbox/01_Local_Memory/`, `02_Coalesced_Access/`, `14_Work_Group_Sizing/` — existing submodules (Phase 2 target files).
- `03_GraphicsHPC/` Ray Tracer submodule READMEs (Phase 3 target).
- `04_Robotics/` Perception submodule README (Phase 3 target).
