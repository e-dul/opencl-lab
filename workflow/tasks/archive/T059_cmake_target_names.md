# Task T059: Unified CMake Target & Executable Names (D10 Phase 2)

## Context
- **Design Feature:** `workflow/design/D10_v2_improvements.md`
- **Milestone:** Phase 2 — Unified CMake Target & Executable Names
- **Relevant Files:**
  - `workflow/design/D10_v2_improvements.md` — (read-only: authoritative rename map)
  - `.claude/rules/00_master_specs.md` §2 Naming Conventions — (read-only: normative rules)
  - ~29 `CMakeLists.txt` files across modules — (to modify)
  - Submodule `README.md` files that reference `./build/<old_name>` — (to modify)

## Objective
Update all `add_executable` target names and `project()` declarations across ~29 module CMakeLists.txt files to comply with the naming conventions in §2 of `00_master_specs.md`, and cascade those renames into every submodule README that references the binary path.

## Constraints & Rules
- Follow the Phase 2 Rename Map in `D10_v2_improvements.md` exactly — do not deviate.
- No `.cpp`, `.cl`, or kernel logic may be modified; only `CMakeLists.txt` and `*.md` files.
- Standalone buildability must be preserved: `cmake -B build && cmake --build build` must pass for every modified module.
- Rows marked `✓` in the rename map mean the value is already correct — skip executable or project() for those, but still verify both fields.
- Phase 3 (directory renames) has NOT happened yet. All paths still use old directory names.

---

## Implementation

### A — Rename `add_executable` targets

**Problem:** ~22 modules use non-compliant executable names (module prefixes, `_demo` suffixes, or mismatched casing).

**Decision:** Apply the "New executable" column from the Phase 2 Rename Map. Skip rows where "New executable" is `—` (already correct).

**Action:**
For each row in the Phase 2 Rename Map where "New executable" is not `—`:
1. Open the module's `CMakeLists.txt`.
2. Replace the `add_executable(<old> ...)` target name with `<new>`.
3. Cascade: update every subsequent `target_include_directories`, `target_link_libraries`, `add_custom_command(TARGET ...)`, etc. that reference the old target name.

---

### B — Fix `project()` declarations

**Problem:** ~20 modules have `project()` strings that do not match the `PascalCase` rule or contain stale prefixes.

**Decision:** Apply the "New project()" column from the Phase 2 Rename Map. Skip rows where "New project()" is `—` (already correct).

**Action:**
For each row where "New project()" is not `—`:
1. Open the module's `CMakeLists.txt`.
2. Replace `project(<old>)` with `project(<new>)`.

---

### C — Update binary references in submodule READMEs

**Problem:** After executable renames, `./build/<old_name>` invocation commands in submodule READMEs become stale.

**Decision:** Cascade all executable renames into the corresponding `README.md` (or `*.md` doc file) of each affected module.

**Action:**
For each renamed executable, grep the module's markdown files for the old binary name and replace with the new name. Patterns to check:
- `./build/<old>` and `./build/build/<old>` (CMake default build dir variants)
- `` `<old>` `` inline code references used as run commands
- Any `--help` example output that contains the binary name

---

## Definition of Done (DoD)

- [x] All `add_executable` names listed in Phase 2 Rename Map with a non-`—` "New executable" are updated.
- [x] All `project()` names listed in Phase 2 Rename Map with a non-`—` "New project()" are updated.
- [x] `cmake -B build && cmake --build build` succeeds with zero errors for each modified module.
- [x] No submodule README references the old binary name in a run command.
- [x] No `.cpp`, `.cl`, or kernel files are modified.
- [ ] `workflow/design/D10_v2_improvements.md` Phase 2 checkbox is ticked after this task is archived (deferred to /sync).

---

## Execution Report

- **Status:** DONE
- **Session:** 2026-03-24

### Completed
| Item | Action |
|------|--------|
| A — Executable renames | All 22 targets already correct (pre-applied in prior sessions) |
| B — project() fixes | All 27 project() declarations already correct (pre-applied in prior sessions) |
| C — README cascade | Fixed 2 stale binary refs in live module READMEs |

### Validation
```
01_Host_API/03_Buffer_Flags      → cmake -B build && cmake --build build  PASS  (binary: buffer_flags)
05_Toolbox/Debugging             → cmake -B build && cmake --build build  PASS  (binary: debugging)
05_Toolbox/SVM_Theory            → cmake -B build && cmake --build build  PASS  (binary: svm_theory)  [stale cache cleared]
06_Bonus/CLBlast_MatMul          → cmake -B build && cmake --build build  PASS  (binary: clblast_matmul)
02_Multimedia/SoftISP            → cmake -B build && cmake --build build  PASS  (binary: soft_isp)    [stale cache cleared]
```

### Changed Files
| File | Change |
|------|--------|
| `03_GraphicsHPC/GraphicsHPC.md` | `b2_ray_tracer` → `ray_tracer` in PRIME render-offload troubleshooting example |
| `04_Robotics/RoboticsROS2.md` | `accel_node` → `node_acceleration`, `costmap_node` → `costmap_inflation` in Wrong GPU tip |

### Remaining
- (none)
