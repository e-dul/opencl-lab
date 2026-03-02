# Task 017: Toolbox — Module Review and Cleanup

## Context
- **Design Feature:** `workflow/design/07-toolbox.md`
- **Milestone:** Phase 12 — Module review and cleanup
- **Relevant Files:**
  - `workflow/design/07-toolbox.md` — (read-only: DoD definitions, cross-link requirements, standalone build rule)
  - `.claude/rules/00_master_specs.md` — (read-only: CLI11, CMake, profiling rules)
  - `99_Toolbox/` — (all tool subdirectories, to audit and patch)

## Objective

Verify that every Toolbox tool builds standalone, confirms its performance gate passes, and contains correct "Used In" cross-references to Module 2 projects; patch any tool that fails.

## Constraints & Rules
- **No Design Changes:** Do not modify `workflow/design/*.md`. If architecture is wrong, stop and ask.
- **No New Features:** This task is audit-and-fix only. Do not add functionality beyond what is required to satisfy existing DoDs.
- **Language/Standard:** C++17.
- **Error Handling:** Throw `std::runtime_error` on CL errors; use `CL_CHECK()` macro.

---

## Implementation

### A — Standalone Build Verification

**Problem:** Any tool whose `CMakeLists.txt` was written with an implicit parent-project assumption will fail when built in isolation.

**Decision:** Run `cmake -B build && cmake --build build` from each tool directory in sequence. Treat any non-zero exit as a blocker.

**Action:**
1. For each tool directory under `99_Toolbox/` (ZeroCopy, CoalescedAccess, LocalMemory, ThreadDivergence, WorkGroupSizing, Debugging, GenericKernelTemplates, AsyncMultiThread, MultiGPU_Strategy, SVM, FastMath):
   - Run `cmake -B build && cmake --build build` from within that directory.
   - Record pass/fail.
2. For each failing tool, identify the root cause (missing `find_package`, bad `target_link_libraries`, missing kernel copy command) and patch `CMakeLists.txt`.
3. Re-run until all tools build cleanly.

---

### B — Performance Gate Spot-Check

**Problem:** A tool that builds but never meets its speedup gate provides no educational value.

**Decision:** Run each tool's binary with the canonical CLI flags from the design doc and confirm the speedup ratio printed in the console table meets or exceeds the target.

**Action:**
For each tool listed below, run the binary and verify the table output against the gate:

| Tool | Canonical command | Gate |
|------|-------------------|------|
| ZeroCopy | `./build/zero_copy --width 1920 --height 1080` | `ALLOC_HOST_PTR` ≥ 3× vs `COPY_HOST_PTR` |
| CoalescedAccess | `./build/coalesced_access --width 1920 --height 1080` | row-major ≥ 5× vs column-major |
| LocalMemory | `./build/local_memory --width 1920 --height 1080 --radius 5` | local ≥ 3× vs global |
| ThreadDivergence | `./build/thread_divergence --width 1920 --height 1080` | `select()` ≥ 1.5× vs `if-else` |
| WorkGroupSizing | `./build/workgroup_sizing --width 1920 --height 1080` | ≥ 2× gap visible in sweep |
| GenericKernelTemplates | `./build/generic_kernel --width 1920 --height 1080` | `float` MAD < 1 ms |
| AsyncMultiThread | `./build/async_multithread --size 4194304` | OOO pipeline ≥ 2× vs blocking |
| MultiGPU_Strategy | `./build/multigpu_strategy --size 3840 --height 2160` | dual-GPU ≥ 2× vs single (skip if only 1 GPU) |
| FastMath | `./build/fast_math --rays 1000000` | `native_rsqrt` ≥ 4× vs standard |

If a tool fails its gate: add a comment block at the top of `main.cpp` documenting the observed ratio and the hardware it was tested on (do NOT lower the gate in the design doc without explicit approval).

---

### C — "Used In" Cross-Reference Audit

**Problem:** The design requires each tool to link back to the Module 2 project where the technique applies. Missing or stale links reduce discoverability.

**Decision:** Check each tool's `README.md` (or leading comment block in `main.cpp` if no README exists at the tool level) for a "Used In:" or "Related:" section pointing to the relevant Module 2 sub-project.

**Action:**
1. For each tool, locate its user-facing doc (`99_Toolbox/<Tool>/<Tool>.md` per design, or `main.cpp` header comment if absent).
2. Verify a "Used In:" line references a specific Module 2 path (e.g., `02_Projects/ImageFilter/`).
3. If missing, add a `## Used In` section to the tool's `.md` file (or a `// Used In:` block comment at the top of `main.cpp`).
4. Do not create new README files — patch existing docs only.

---

### D — CLI `--help` Smoke Test

**Problem:** Broken CLI11 integration produces either a crash or an undocumented interface.

**Decision:** Run `<binary> --help` for every tool and confirm it exits 0 and prints at least the flags defined in the design spec.

**Action:**
1. For each tool binary, run `./build/<binary> --help`.
2. Verify exit code 0 and that required flags appear in output (e.g., `--width`, `--height`, `--rays`, `--size`).
3. Patch `main.cpp` App description or flag registration if any required flag is absent.

---

## Definition of Done (DoD)

- [ ] All 11 tool directories produce a successful `cmake -B build && cmake --build build` with zero errors and zero warnings.
- [ ] All 9 performance-gated tools print a timing table where the speedup ratio meets or exceeds the target defined in the design doc (measured on a discrete GPU, or documented as skipped with hardware note for MultiGPU/SVM).
- [ ] Every tool has a "Used In" cross-reference to at least one Module 2 project path, in either its `.md` doc or `main.cpp` header comment.
- [ ] `<binary> --help` exits 0 and lists all required CLI flags for all 11 tools.
- [ ] No performance gates in `workflow/design/07-toolbox.md` were modified.

---

## Execution Report
<!-- Filled by @coder after implementation. -->

- **Status:** PENDING
- **Session:** —

### Completed
| Item | Action |
|------|--------|
| A — Standalone Build Verification | — |
| B — Performance Gate Spot-Check | — |
| C — "Used In" Cross-Reference Audit | — |
| D — CLI `--help` Smoke Test | — |

### Validation
```
[output here]
```

### Changed Files
| File | Change |
|------|--------|
| `99_Toolbox/*/CMakeLists.txt` | Patched (if needed) |
| `99_Toolbox/*/<Tool>.md` | Added "Used In" section (if missing) |

### Remaining
- [ ] Implementation
