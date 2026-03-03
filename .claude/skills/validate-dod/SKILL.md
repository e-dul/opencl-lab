---
name: validate-dod
description: Builds, runs, and validates the implementation against the task's Definition of Done, then fills in the Execution Report.
allowed-tools: Read, FileEdit, Bash, Glob, Grep
---
# DoD Validation Skill

You are executing the Validation phase. Your job is to confirm the implementation works correctly at runtime and record the results. You MUST NOT modify any source files (`.cpp`, `.cl`, `CMakeLists.txt`).

## Instructions

### 1. Read Context
- Read the task file to extract: build directory, binary name, and every DoD checkbox.
- Read the task's `## Relevant Files` to locate the module root.

### 2. Build
- `cd` into the module directory and run:
  ```
  cmake -B build && cmake --build build 2>&1
  ```
- Capture full output. If the build fails, record the error in the Execution Report and stop — do not proceed to runtime checks.

### 3. Runtime Validation
Run each DoD item that requires execution. For each check:

| DoD pattern | Command / assertion |
|---|---|
| Binary runs without arguments | `./build/<binary>` — exit code must be 0 |
| `output.bmp` produced | `ls -lh output.bmp` after run |
| Timing table printed | grep for the required row labels in stdout |
| All kernel times non-zero | grep rows, assert values are not `0.000` |
| `--help` flag works | `./build/<binary> --help` — exit code 0, flags visible |
| Pixel verification passes silently | no `[ERROR]` lines in stdout |
| GPU env var respected | `GPU=<vendor> ./build/<binary>` — no crash |

Capture stdout and stderr for each run.

### 4. Fill Execution Report
Update the `## Execution Report` section of the task file **only**. Use FileEdit. Set:
- `**Status:**` → `COMPLETE` (all DoD pass) or `FAILED` (any DoD fails)
- `**Session:**` → today's date (YYYY-MM-DD)
- `### Validation` code block → trimmed stdout of the main run (≤ 40 lines)
- Check each `- [ ]` DoD checkbox that passed → `- [x]`
- Leave failed checkboxes as `- [ ]` and append a note explaining the failure

### 5. Communication Rules
- Terse. No filler.
- Report only: Status, which DoD items passed/failed, and the trimmed validation output.
