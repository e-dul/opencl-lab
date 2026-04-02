# Task T071: Toolbox Advanced Challenges

## Context
- **Design Feature:** `workflow/design/D12_v2_2_improvements.md` — Phase 2
- **Milestone:** Phase 2 — Toolbox Advanced Challenges
- **Relevant Files:**
  - `05_Toolbox/01_Local_Memory/LocalMemory.md` — (to modify)
  - `05_Toolbox/02_Coalesced_Access/CoalescedAccess.md` — (to modify)
  - `05_Toolbox/14_Work_Group_Sizing/WorkGroupSizing.md` — (to modify)
  - `05_Toolbox/Toolbox.md` — (to modify: symptom text updates for 3 rows)
  - `workflow/design/D12_v2_2_improvements.md` — (read-only: spec reference)

## Objective

Append an *Advanced Challenge* section to each of three existing Toolbox submodule READMEs — `LocalMemory.md`, `CoalescedAccess.md`, and `WorkGroupSizing.md` — and update the corresponding `Toolbox.md` symptom-text entries; no source files are modified.

## Constraints & Rules

- **Documentation-only:** Zero modifications to `.cpp`, `.cl`, or `CMakeLists.txt` files.
- **No new submodule directories** — content is appended to existing READMEs only.
- **Callout box format** for advanced challenges: use a fenced `> **Advanced Challenge: <Title>**` blockquote header to match project conventions.
- **Toolbox.md row edits** are strictly symptom-text appends (add `(Incl. <topic>)` clause); no row reordering or renumbering.

---

## Implementation

### A — Bank Conflicts section in `01_Local_Memory/LocalMemory.md`

**Spec reference:** D12 §2a

**Action:**
1. Append a new `## Advanced Challenge: Bank Conflicts in LDS` section at the end of `LocalMemory.md`.
2. Content:
   - Explain the 32-bank LDS architecture and how a stride-`N` (where N is a multiple of 32) access pattern causes all threads in a warp/wavefront to hit the same bank, serializing access.
   - Show the problematic kernel snippet: `__local float tile[ROWS][COLS]` — thread `i` reads `tile[0][i * 32]`.
   - Show the `+1` column padding fix: `__local float tile[ROWS][COLS + 1]` — offsets each row by one element, spreading accesses across banks.
   - Tie the example back to the tile-based convolution already demonstrated in the module.
3. In `Toolbox.md`, update the `01_Local_Memory` row symptom text to append `(Incl. Bank Conflicts)`.

---

### B — AoS vs. SoA section in `02_Coalesced_Access/CoalescedAccess.md`

**Spec reference:** D12 §2b

**Action:**
1. Append a new `## Advanced Challenge: AoS vs. SoA Data Layout` section at the end of `CoalescedAccess.md`.
2. Content:
   - Show a C++ `struct Particle { float x, y, z, w; }` Array-of-Structures (AoS) layout.
   - Trace how thread `N` reading `particles[N].x` produces stride-4 access — only 25% of each 128-byte cache line is used per transaction, wasting 75%.
   - Show the Structure-of-Arrays (SoA) refactor: `float* xs; float* ys; float* zs; float* ws;` — thread `N` reads `xs[N]`, achieving fully coalesced sequential access.
   - Include a benchmark note: measure the same operation on AoS vs. SoA buffers using `cl::Event` timing; expected outcome is 2–4x throughput improvement on bandwidth-bound kernels.
3. In `Toolbox.md`, update the `02_Coalesced_Access` row symptom text to append `(Incl. AoS vs. SoA)`.

---

### C — Register Pressure section in `14_Work_Group_Sizing/WorkGroupSizing.md`

**Spec reference:** D12 §2c

**Action:**
1. Append a new `## Advanced Challenge: Register Pressure & Spilling` section at the end of `WorkGroupSizing.md`.
2. Content:
   - Explain the register file budget per compute unit (e.g., 65536 registers on Nvidia Ampere, split across all concurrent threads).
   - Show a kernel with excessive `private` variables to deliberately increase per-thread register count.
   - Show how to observe register/spill stats: `-cl-nv-verbose` on Nvidia (captures `ptxas` output), and equivalent vendor flags for AMD (`-cl-amd-nv-verbose` / ROCm toolchain notes) and Intel (IGC kernel stats).
   - Connect to the occupancy formula: `max_concurrent_warps = register_file_size / (registers_per_thread * warp_size)`. High register count → fewer concurrent warps → lower GPU utilization even with a large NDRange.
3. In `Toolbox.md`, update the `14_Work_Group_Sizing` row symptom text to append `(Incl. Register Pressure)`.

---

## Definition of Done (DoD)

- [x] `05_Toolbox/01_Local_Memory/LocalMemory.md` contains a new `## Advanced Challenge: Bank Conflicts in LDS` section with the 32-bank explanation, problematic snippet, and `+1` padding fix.
- [x] `05_Toolbox/02_Coalesced_Access/CoalescedAccess.md` contains a new `## Advanced Challenge: AoS vs. SoA Data Layout` section with AoS stride analysis, SoA refactor, and benchmark note.
- [x] `05_Toolbox/14_Work_Group_Sizing/WorkGroupSizing.md` contains a new `## Advanced Challenge: Register Pressure & Spilling` section with register-budget explanation, compiler flag references, and occupancy formula.
- [x] `05_Toolbox/Toolbox.md` symptom text for rows `01_Local_Memory`, `02_Coalesced_Access`, and `14_Work_Group_Sizing` updated with the `(Incl. ...)` clauses defined in D12 §2a–2c.
- [x] No `.cpp`, `.cl`, or `CMakeLists.txt` files are modified (verify with `git diff --name-only`).
- [x] MANUAL: Read through each appended section; confirm explanations are clear to a mid-level engineer unfamiliar with GPU microarchitecture; no placeholder text or incomplete sentences remain.

---

## Execution Report
<!-- Filled by @coder after implementation. -->

- **Status:** COMPLETE
- **Session:** 2026-03-28

### Validation
```
git diff --name-only:
  05_Toolbox/01_Local_Memory/LocalMemory.md
  05_Toolbox/02_Coalesced_Access/CoalescedAccess.md
  05_Toolbox/14_Work_Group_Sizing/WorkGroupSizing.md
  05_Toolbox/Toolbox.md
No .cpp/.cl/CMakeLists.txt files modified.
```

### Changed Files
| File | Change |
|------|--------|
| `05_Toolbox/01_Local_Memory/LocalMemory.md` | Modified — Advanced Challenge section appended |
| `05_Toolbox/02_Coalesced_Access/CoalescedAccess.md` | Modified — Advanced Challenge section appended |
| `05_Toolbox/14_Work_Group_Sizing/WorkGroupSizing.md` | Modified — Advanced Challenge section appended |
| `05_Toolbox/Toolbox.md` | Modified — symptom-text updates for 3 rows |
