# Task T066: Add Scripts README

## Context
- **Design Feature:** `workflow/design/D10_v2_improvements.md`
- **Milestone:** Phase 9 — Add scripts README
- **Relevant Files:**
  - `workflow/design/D10_v2_improvements.md` — (read-only: §Phase 9 Scripts Scope)
  - `scripts/build_all.sh` — (read-only: reference)
  - `scripts/test_all.sh` — (read-only: reference)
  - `scripts/progress.sh` — (read-only: reference)
  - `scripts/gen_pgm.py` — (read-only: reference)
  - `scripts/gen_bayer.py` — (read-only: reference, added in Phase 8)
  - `scripts/README.md` — (new file)

## Objective

Create `scripts/README.md` documenting all five helper scripts (`build_all.sh`, `test_all.sh`, `progress.sh`, `gen_pgm.py`, `gen_bayer.py`) — their purpose, usage, and expected output — so contributors have a single reference without needing to read each script.

## Constraints & Rules

- Do NOT modify any `.cpp`, `.cl`, `CMakeLists.txt`, or existing script file.
- Do NOT modify any existing module README or design doc.
- The design spec lists four scripts; `gen_bayer.py` was added in Phase 8 and must also be documented (it is a resident script in the same directory).
- All usage examples must be invocable from the repository root (no assumed `cd`).
- Educational tone consistent with the rest of the lab (mid-level engineer audience).

---

## Implementation

### A — Create `scripts/README.md`

**Problem:** The `scripts/` directory has no index. Contributors discovering `build_all.sh`, `progress.sh`, or the generator scripts cannot tell at a glance what each does, what prerequisites they have, or what output to expect.

**Decision:** Single README covering all five scripts; each gets its own `##` section with Purpose, Prerequisites, Usage, and Expected Output sub-items. No duplication of script internals — refer to the script docstring/comments for implementation detail.

**Action:**

Read each of the five scripts in full before writing, then author `scripts/README.md` with:

1. A short top-level paragraph explaining the `scripts/` directory's role in the lab.

2. One `##` section per script in this order:
   - `build_all.sh`
   - `test_all.sh`
   - `progress.sh`
   - `gen_pgm.py`
   - `gen_bayer.py`

3. Each section must include:
   - **Purpose** — one sentence.
   - **Prerequisites** — list any tools or files that must exist before running.
   - **Usage** — copy-pasteable shell command(s) from the repository root.
   - **Expected Output** — what the user should see or what file(s) are produced.

4. Notes to observe from reading the scripts:
   - `build_all.sh` builds only modules 0 and 1 by default (all other `BUILD_*` flags are `OFF`). Document this clearly so students understand it is not a full build.
   - `test_all.sh` requires a prior `cmake -B build` + `cmake --build build` (the build directory must exist).
   - `progress.sh` reads `workflow/design/*.md` and counts `[x]`/`[ ]` checkbox items; output is a progress table with per-design-doc rows and a TOTAL line.
   - `gen_pgm.py` accepts `--output`, `--width`, `--height`, `--no-border` flags; default output is `warehouse.pgm` in the current directory. Document the canonical invocation that writes to `assets/`.
   - `gen_bayer.py` has no CLI flags; it reads `assets/rgb_4k.bmp` and writes `assets/raw_bayer_4k.raw` unconditionally. Document the prerequisite (`rgb_4k.bmp` must exist first).

---

## Definition of Done (DoD)

- [x] `scripts/README.md` exists.
- [x] `scripts/README.md` contains exactly five `##`-level sections, one per script (`build_all.sh`, `test_all.sh`, `progress.sh`, `gen_pgm.py`, `gen_bayer.py`).
- [x] Each section contains a Purpose statement, a Prerequisites list, at least one copy-pasteable usage command, and an Expected Output description.
- [x] `build_all.sh` section documents that only modules 0 and 1 are built by default.
- [x] `gen_bayer.py` section documents the `assets/rgb_4k.bmp` prerequisite.
- [x] `gen_pgm.py` section shows the canonical invocation writing to `assets/warehouse.pgm`.
- [x] All usage commands are invocable from the repository root (no bare `cd` into `scripts/`).
- [x] No `.cpp`, `.cl`, `CMakeLists.txt`, or existing script file appears in `git diff --name-only`.

---

## Execution Report
<!-- Filled by @coder after implementation. -->

- **Status:** COMPLETED
  <!-- PENDING → IN PROGRESS → COMPLETED -->
- **Session:** 2026-03-25

### Completed
| Item | Action |
|------|--------|
| A — Create `scripts/README.md` | Authored 5-section README covering all scripts; each section has Purpose, Prerequisites, Usage, and Expected Output. |

### Validation
```
5 ## sections present: build_all.sh, test_all.sh, progress.sh, gen_pgm.py, gen_bayer.py.
git diff --name-only: no .cpp/.cl/CMakeLists.txt modified.
All usage commands invocable from repo root.
build_all.sh section documents modules 0 and 1 only.
gen_bayer.py section documents assets/rgb_4k.bmp prerequisite.
gen_pgm.py section shows canonical invocation writing to assets/warehouse.pgm.
```

### Changed Files
| File | Change |
|------|--------|
| `scripts/README.md` | Created |

### Remaining
- [ ] Archive this task after `/sync`.
