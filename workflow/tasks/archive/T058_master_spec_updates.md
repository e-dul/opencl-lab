# Task T058: Master Spec Updates (D10 Phase 1)

## Context
- **Design Feature:** `workflow/design/D10_v2_improvements.md`
- **Milestone:** Phase 1 — Master Spec Updates
- **Relevant Files:**
  - `.claude/rules/00_master_specs.md` — (to modify)
  - `workflow/design/D10_v2_improvements.md` — (read-only: authoritative source for changes)

## Objective
Apply the three targeted edits to `.claude/rules/00_master_specs.md` that are prerequisites for D10 Phases 4 and 7: replace the POST_BUILD kernel-copy template with a symlink variant, add a ROS 2 CLI11 exemption clause, and codify the naming conventions for executables, `project()` strings, and submodule directories.

## Constraints & Rules
- No `.cpp`, `.cl`, or `CMakeLists.txt` files may be touched — this task is documentation-only.
- The three changes must be applied atomically in a single edit session; do not split across multiple commits.
- Do not alter any other section of `00_master_specs.md` beyond the three items listed below.

---

## Implementation

### A — POST_BUILD Kernel Template (prerequisite for Phase 4)
**Problem:** §1 Build System currently mandates `cmake -E copy_directory`. D10 Key Decision 6 specifies symlinks as the canonical approach.

**Decision:** Replace the `copy_directory` code block with `create_symlink`; keep the surrounding prose intact.

**Action:**
Replace the POST_BUILD cmake block in §1 Build System:

```cmake
# OLD
add_custom_command(TARGET <target> POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          ${CMAKE_CURRENT_SOURCE_DIR}/kernels
          $<TARGET_FILE_DIR:<target>>/kernels
  COMMENT "Copying kernels"
)

# NEW
add_custom_command(TARGET <target> POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E create_symlink
          ${CMAKE_CURRENT_SOURCE_DIR}/kernels
          $<TARGET_FILE_DIR:<target>>/kernels
  COMMENT "Symlinking kernels (source stays canonical; build dir mirrors live)"
)
```

Add a portability note immediately after the block:
> **Note:** `cmake -E create_symlink` requires source and build directory to reside on the same filesystem. Cross-filesystem or container builds must fall back to `copy_directory`.

Hint to use function from `common/common.cmake` instead duplicating same content.

---

### B — CLI11 ROS 2 Exemption (prerequisite for Phase 7)
**Problem:** §1 CLI Parsing currently forbids hand-rolled parsers with no carve-out for ROS 2. The `declare_parameter` / `get_parameter` / `ros2 param` pattern is the idiomatic ROS 2 equivalent.

**Decision:** Add an exemption clause under the CLI Parsing bullet.

**Action:**
Append after the `Integrated via common/common.cmake; linked as CLI11::CLI11.` line:
> **ROS 2 Exemption:** Modules using ROS2 under `04_Robotics/` or `06_Bonus` are exempt from the CLI11 requirement. Use `declare_parameter()` / `get_parameter()` for parameter handling and `ros2 param` CLI for runtime inspection. Hand-rolled `--help` loops are still forbidden.

---

### C — Naming Conventions (prerequisite for Phases 2 and 3)
**Problem:** §2 Directory Structure Protocol does not codify executable, `project()`, or submodule directory naming rules. D10 Key Decisions 1–5 define these rules but they exist only in the design doc.

**Decision:** Add a `### Naming Conventions` subsection to §2.

**Action:**
Insert the following subsection at the end of §2 Directory Structure Protocol:

```markdown
### Naming Conventions
- **Executable names (`add_executable` target):** `snake_case`. No module prefix (`A1_`, `b2_`, `c3_`). No `_demo` suffix.
- **`project()` names:** `PascalCase` matching the submodule directory name exactly (after stripping the numeric prefix).
- **Submodule directory names:** `NN_Title_Snake_Case` where `NN` is a two-digit number scoped per parent module. Gaps on add/remove are acceptable; renumbering existing entries is **FORBIDDEN**.
- **Acronyms:** ALLCAPS for well-known technical initialisms (`SVM`, `BVH`, `YUV`, `ISP`, `DNN`). Preserve upstream spelling for product/library names (`OpenCV`, `OpenVINO`, `CLBlast`, `VkFFT`).
- **Multi-target modules:** Keep a descriptive per-target `snake_case` name when a module exposes multiple binaries.
```

---

## Definition of Done (DoD)

- [x] `.claude/rules/00_master_specs.md` §1 POST_BUILD block replaced with `create_symlink` variant, including the portability note.
- [x] `.claude/rules/00_master_specs.md` §1 CLI Parsing section contains the ROS 2 exemption clause.
- [x] `.claude/rules/00_master_specs.md` §2 contains a `### Naming Conventions` subsection with all five bullet rules.
- [x] No other sections of `00_master_specs.md` are modified.
- [x] No `.cpp`, `.cl`, or `CMakeLists.txt` files are changed.
- [ ] `workflow/design/D10_v2_improvements.md` Phase 1 checkbox is ticked after this task is archived.

---

## Execution Report
<!-- Filled by @coder after implementation. -->

- **Status:** COMPLETE
- **Session:** 2026-03-24

### Validation
```
PASS  §1 POST_BUILD block — create_symlink variant present (line 17).
PASS  §1 POST_BUILD portability note — immediately after cmake block (line 23).
PASS  §1 POST_BUILD common.cmake hint — present after note (line 24).
PASS  §1 CLI Parsing — ROS 2 exemption clause present (line 28), wording matches task exactly.
PASS  §2 Naming Conventions — subsection present with all 5 bullet rules (lines 37–42).
PASS  No other sections modified — §3–§9 unchanged.
PASS  No .cpp/.cl/CMakeLists.txt files changed — documentation-only task.
SKIP  D10_v2_improvements.md Phase 1 checkbox — deferred to /sync.
```

### Changed Files
| File | Change |
|------|--------|
| `.claude/rules/00_master_specs.md` | Modified — three targeted edits per Items A, B, C |

### Remaining
- [x] All three edits applied and reviewed
