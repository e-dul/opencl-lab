# Task T062: Add Missing Bonus READMEs (D10 Phase 5)

## Context
- **Design Feature:** `workflow/design/D10_v2_improvements.md`
- **Milestone:** Phase 5 — Add Missing Bonus READMEs
- **Relevant Files:**
  - `06_Bonus/01_CLBlast_MatMul/main.cpp` — (read-only: implementation reference)
  - `06_Bonus/01_CLBlast_MatMul/CMakeLists.txt` — (read-only: executable name, dependencies)
  - `06_Bonus/02_Device_Enqueue/main.cpp` — (read-only: implementation reference)
  - `06_Bonus/02_Device_Enqueue/CMakeLists.txt` — (read-only: executable name, dependencies)
  - `06_Bonus/Bonus.md` — (to modify: update two broken submodule links)
  - `06_Bonus/03_VkFFT_Audio/vkFFTAudio.md` — (read-only: style reference for bonus READMEs)
  - `06_Bonus/04_Voxel_Mapping/VoxelMapping.md` — (read-only: style reference for bonus READMEs)
  - `workflow/design/D10_v2_improvements.md` — (read-only: authoritative spec)
  - Reference (main branch only): `02_Projects/B_Graphics_HPC/GraphicsHPC.md` — prior content for both CLBlast and Device Enqueue modules

## Objective

Author two student-facing README files — `06_Bonus/01_CLBlast_MatMul/CLBlastMatMul.md` and `06_Bonus/02_Device_Enqueue/DeviceEnqueue.md` — and update `06_Bonus/Bonus.md` to link to them, so that both Bonus submodules are no longer undocumented.

## Constraints & Rules

- **Markdown files only.** Do NOT touch `.cpp`, `.cl`, or `CMakeLists.txt`.
- **Style parity.** Follow the structure of `03_VkFFT_Audio/vkFFTAudio.md` and `04_Voxel_Mapping/VoxelMapping.md`: Goals section, Prerequisites, Build & Run, Expected Output, Key Concepts, and a back-link to `../Bonus.md`.
- **Executable names must match Phase 2 renames.** `01_CLBlast_MatMul` binary is `clblast_matmul`; `02_Device_Enqueue` binary is `device_enqueue`. Verify against `CMakeLists.txt` before writing.
- **HW/dependency callout (per Phase 6 preview).** `01_CLBlast_MatMul` has no special hardware tag. `02_Device_Enqueue` must include a `> **Requires:** OpenCL 2.0+` callout block near the top (prerequisite for Phase 6 — pre-populates the tag).
- **OpenCL 2.0 graceful-fallback note.** The Device Enqueue README must note that the binary prints a descriptive message and exits cleanly (code 0) on devices that do not support OpenCL 2.0 (per `00_master_specs.md §4`).
- **Reference material access.** Read `02_Projects/B_Graphics_HPC/GraphicsHPC.md` from `main` branch (`git show main:02_Projects/B_Graphics_HPC/GraphicsHPC.md`) — this is the canonical prior content for both CLBlast and Device Enqueue modules.
- **Asset paths.** Any asset referenced in the READMEs must follow the pattern `assets/<file>` (CLI arg, not hardcoded path).
- **No duplication with index.** Prerequisites already listed in `Bonus.md` need only a brief cross-reference in the submodule README, not full repetition.

---

## Implementation

### A — Author `06_Bonus/01_CLBlast_MatMul/CLBlastMatMul.md`

**Problem:** The `Bonus.md` index links directly to the subdirectory (`01_CLBlast_MatMul/`) with no README landing page — students have no guided entry point.

**Decision:** Author a student-facing README modelled on `vkFFTAudio.md`. CLBlast is a drop-in BLAS library; the README must explain what BLAS GEMM is and why GPU-accelerated BLAS matters before showing the build/run workflow.

**Action:**

1. `git show main:02_Projects/B_Graphics_HPC/GraphicsHPC.md` — extract CLBlast section for prior Goals and Expected Output.
2. Read `06_Bonus/01_CLBlast_MatMul/main.cpp` and `CMakeLists.txt` — confirm binary name (`clblast_matmul`), CLI flags, and output format.
3. **Invoke `/create-readme` (@educator)** with the gathered context to author `06_Bonus/01_CLBlast_MatMul/CLBlastMatMul.md`. The README must include:
   - `# CLBlast Matrix Multiplication` heading.
   - Goals bullets (what the student will learn).
   - `## Prerequisites` — CLBlast library installed; Module 1 sufficient.
   - `## Build & Run` — `cmake -B build && cmake --build build`, then `./build/clblast_matmul`.
   - `## Expected Output` — describe console timing table (no BMP; this is a benchmark module per `00_master_specs.md §3`).
   - `## Key Concepts` — BLAS GEMM, drop-in GPU acceleration, why CLBlast over cuBLAS.
   - Back-link: `[← Bonus Modules](../Bonus.md)`.

---

### B — Author `06_Bonus/02_Device_Enqueue/DeviceEnqueue.md`

**Problem:** Same as A — no README exists; `Bonus.md` has no link to a submodule doc.

**Decision:** Author a student-facing README. Device-side enqueue is an OpenCL 2.0 feature; the README must set clear hardware expectations upfront before any build instructions.

**Action:**

1. `git show main:02_Projects/B_Graphics_HPC/GraphicsHPC.md` — extract Device Enqueue section for prior Goals and Expected Output.
2. Read `06_Bonus/02_Device_Enqueue/main.cpp` and `CMakeLists.txt` — confirm binary name (`device_enqueue`), CLI flags, output format, and graceful-fallback behaviour.
3. **Invoke `/create-readme` (@educator)** with the gathered context to author `06_Bonus/02_Device_Enqueue/DeviceEnqueue.md`. The README must include:
   - `# Device-Side Enqueue` heading.
   - `> **Requires:** OpenCL 2.0+ device` callout block immediately after the title.
   - Goals bullets.
   - `## Prerequisites` — OpenCL 2.0 capable device; note binary exits cleanly on unsupported hardware.
   - `## Build & Run` — `cmake -B build && cmake --build build`, then `./build/device_enqueue`.
   - `## Expected Output` — describe expected console output (queue depth, timings, or BMP if applicable).
   - `## Key Concepts` — device-side enqueue, parent/child kernel relationship, OpenCL 2.0 device queue.
   - Back-link: `[← Bonus Modules](../Bonus.md)`.

---

### C — Update `06_Bonus/Bonus.md` index links

**Problem:** The `Bonus.md` table currently links `CLBlast MatMul` to `01_CLBlast_MatMul/` (directory, no README) and `Device Enqueue` to `02_Device_Enqueue/` (directory, no README).

**Decision:** Update both links to point to the newly authored README files.

**Action:**
Replace the two broken table entries in `Bonus.md`:
- `[CLBlast MatMul](01_CLBlast_MatMul/)` → `[CLBlast MatMul](01_CLBlast_MatMul/CLBlastMatMul.md)`
- `[Device Enqueue](02_Device_Enqueue/)` → `[Device Enqueue](02_Device_Enqueue/DeviceEnqueue.md)`

---

## Definition of Done (DoD)

- [x] `06_Bonus/01_CLBlast_MatMul/CLBlastMatMul.md` exists and contains Goals, Prerequisites, Build & Run, Expected Output, Key Concepts, and a back-link to `../Bonus.md`.
- [x] `06_Bonus/02_Device_Enqueue/DeviceEnqueue.md` exists and contains Goals, Prerequisites, Build & Run, Expected Output, Key Concepts, and a back-link to `../Bonus.md`.
- [x] `DeviceEnqueue.md` contains a `> **Requires:** OpenCL 2.0+` callout block near the top.
- [x] `DeviceEnqueue.md` states that the binary exits cleanly (code 0) on devices without OpenCL 2.0 support.
- [x] `06_Bonus/Bonus.md` table links `CLBlast MatMul` → `01_CLBlast_MatMul/CLBlastMatMul.md` and `Device Enqueue` → `02_Device_Enqueue/DeviceEnqueue.md`.
- [x] Executable names in Build & Run sections match Phase 2 renames: `clblast_matmul` and `device_enqueue`. Verify against each module's `CMakeLists.txt`.
- [x] No `.cpp`, `.cl`, or `CMakeLists.txt` files were modified.
- [x] MANUAL: Read both READMEs end-to-end; confirm the pedagogical flow is clear for a student who has completed Module 1 only.

---

## Execution Report

- **Status:** DONE
- **Session:** 2026-03-25

### Completed
| Item | Action |
|------|--------|
| A — CLBlastMatMul.md | File exists at `06_Bonus/01_CLBlast_MatMul/CLBlastMatMul.md`; all required sections present |
| B — DeviceEnqueue.md | File exists at `06_Bonus/02_Device_Enqueue/DeviceEnqueue.md`; all required sections present |
| C — Bonus.md link updates | Both table entries point to `CLBlastMatMul.md` and `DeviceEnqueue.md` respectively |

### Validation
```
DoD 1: CLBlastMatMul.md — Goals (line 5), Prerequisites (line 13), Build & Run (line 20),
        Expected Output (line 34), Key Concepts (line 52), back-link (line 72). PASS.
DoD 2: DeviceEnqueue.md — Goals (line 8), Prerequisites (line 15), Build & Run (line 24),
        Expected Output (line 49), Key Concepts (line 79), back-link (line 116). PASS.
DoD 3: `> **Requires:** OpenCL 2.0+` present at DeviceEnqueue.md line 3. PASS.
DoD 4: "The process exits with code 0 — no crash, no silent hang." at line 22. PASS.
DoD 5: Bonus.md line 15 → `01_CLBlast_MatMul/CLBlastMatMul.md`;
        Bonus.md line 16 → `02_Device_Enqueue/DeviceEnqueue.md`. PASS.
DoD 6: CLBlastMatMul.md Build & Run uses `./build/clblast_matmul`;
        CMakeLists.txt line 28: `add_executable(clblast_matmul ...)`. PASS.
        DeviceEnqueue.md Build & Run uses `./build/device_enqueue`;
        CMakeLists.txt line 28: `add_executable(device_enqueue ...)`. PASS.
DoD 7: `git diff --name-only HEAD -- '*.cpp' '*.cl' '*/CMakeLists.txt'` — no output. PASS.
```

### Changed Files
| File | Change |
|------|--------|
| `06_Bonus/01_CLBlast_MatMul/CLBlastMatMul.md` | Created |
| `06_Bonus/02_Device_Enqueue/DeviceEnqueue.md` | Created |
| `06_Bonus/Bonus.md` | Modified — update two table links |

### Remaining
- [ ] MANUAL: Read both READMEs end-to-end; confirm the pedagogical flow is clear for a student who has completed Module 1 only.
