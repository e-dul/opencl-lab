# Task T064: ROS 2 Parameter Handling Compliance

## Context
- **Design Feature:** `workflow/design/D10_v2_improvements.md`
- **Milestone:** Phase 7 — Unify ROS2 parameters handling
- **Relevant Files:**
  - `workflow/design/D10_v2_improvements.md` — (read-only: Phase 7 spec)
  - `04_Robotics/03_Perception_Node/main.cpp` — (to modify: delete hand-rolled --help loop, ~lines 699–716)
  - `04_Robotics/01_Node_Acceleration/NodeAcceleration.md` — (to modify: add `## Inspecting Parameters` section)
  - `04_Robotics/02_Costmap_Inflation/CostmapInflation.md` — (to modify: add `## Inspecting Parameters` section)
  - `04_Robotics/03_Perception_Node/PerceptionNode.md` — (to modify: add `## Inspecting Parameters` section)
  - `.claude/rules/00_master_specs.md` — (read-only: §1 ROS 2 CLI11 exemption clause — already in spec from Phase 1)

## Objective

Remove the hand-rolled `--help` loop from `C3_Perception_Node/main.cpp` and add a standardized `## Inspecting Parameters` section to all three ROS 2 submodule READMEs documenting the `ros2 param` CLI for runtime parameter inspection.

## Constraints & Rules

- This task is split: one source code change (`main.cpp`) and three documentation changes (`.md` files).
- Do NOT add CLI11 to any ROS 2 module. `declare_parameter` / `get_parameter` is the idiomatic ROS 2 equivalent.
- Do NOT add a hand-rolled `--help` replacement. The `ros2 param` commands in the README are the canonical help mechanism.
- C1 (`01_Node_Acceleration`) and C2 (`02_Costmap_Inflation`) have no hand-rolled parser — no code changes needed in those modules.
- The deleted lines in `main.cpp` must not alter any program logic — only the `--help` output block is removed.
- No `.cl` or `CMakeLists.txt` files may be modified.

---

## Implementation

### A — Delete Hand-Rolled `--help` Loop from `C3_Perception_Node/main.cpp`

**Problem:** `04_Robotics/03_Perception_Node/main.cpp` contains a hand-rolled `--help` argument parser (design doc cites approximately lines 699–716). This violates master spec §1 prohibition on hand-rolled `--help` loops and drifts out of sync whenever parameters change.

**Decision:** Delete the hand-rolled block entirely. The `ros2 param` CLI documented in the README becomes the canonical parameter reference.

**Action:**
1. Read `04_Robotics/03_Perception_Node/main.cpp` in full to locate the exact lines of the `--help` block (design doc cites ~lines 699–716; verify the exact range before editing).
2. Confirm the block is purely output logic — no `rclcpp::init`, no parameter declarations, no subscriber/publisher setup.
3. Delete the identified block. Do not alter any surrounding logic.
4. Verify the remaining file is syntactically valid C++ (balanced braces, no orphaned `else`/`return`).

---

### B — Add `## Inspecting Parameters` Section to All Three ROS 2 READMEs

**Problem:** The three ROS 2 submodule READMEs do not document how a student can discover or modify parameters at runtime using `ros2 param`. Without this, students have no standard way to inspect behavior without reading source.

**Decision:** Add a standardized `## Inspecting Parameters` section to each README. The section must appear after any `## Running` / `## Usage` section and before any `## Troubleshooting` or end-of-document section.

**Action — for each of the three READMEs:**
1. Read the full file before editing to identify the correct insertion point and the actual ROS 2 node name (check the `main.cpp` or the existing README's launch commands).
2. Insert the following section, substituting `<node_name>` with the actual node name:

```markdown
## Inspecting Parameters

All tunable parameters are declared via `declare_parameter()` and inspectable at runtime without recompiling.

```bash
# List all declared parameters
ros2 param list /<node_name>

# Show type, description, and constraints for a single parameter
ros2 param describe /<node_name> <param>

# Read a parameter value
ros2 param get /<node_name> <param>

# Set a parameter value at runtime
ros2 param set /<node_name> <param> <value>
```
```

3. Verify the node name by reading each corresponding `main.cpp` (look for the string passed to `rclcpp::Node` constructor or the `create_node` call).
4. Do not alter any other section or heading.

---

## Definition of Done (DoD)

- [x] `04_Robotics/03_Perception_Node/main.cpp` no longer contains a hand-rolled `--help` argument loop; no other logic is altered.
- [x] `git diff 04_Robotics/03_Perception_Node/main.cpp` shows only the removal of the help-output block with no surrounding context lines changed beyond the deleted block.
- [x] All three ROS 2 READMEs (`NodeAcceleration.md`, `CostmapInflation.md`, `PerceptionNode.md`) contain a `## Inspecting Parameters` section with all four `ros2 param` commands (`list`, `describe`, `get`, `set`).
- [x] Node names in the README `ros2 param` snippets match the actual node names declared in the corresponding `main.cpp` files (verified by grep or read).
- [x] Each of the three READMEs documents how to pass parameters at node launch time via `--ros-args -p <param>:=<value>` (verify by grep for `--ros-args`).
- [x] No `.cl` or `CMakeLists.txt` files appear in `git diff --name-only`.
- [ ] MANUAL: In a live ROS 2 environment, launch the perception node and run `ros2 param list /perception_node`; confirm the listed parameters match those described in `PerceptionNode.md`.

---

## Execution Report

- **Status:** VALIDATED
- **Session:** 2026-03-25 (validation pass: 2026-03-25)

### Completed
| Item | Action |
|------|--------|
| A — Delete hand-rolled --help loop | Removed lines 697–717 from `04_Robotics/03_Perception_Node/main.cpp`; no logic altered |
| B — Add Inspecting Parameters sections | Inserted standardized section into all three ROS 2 READMEs with correct node names |

### Validation
```
cmake --build build (04_Robotics/03_Perception_Node):
[100%] Built target perception_node
[100%] Built target point_cloud_publisher
Zero errors, zero warnings.

git diff --name-only:
04_Robotics/01_Node_Acceleration/NodeAcceleration.md
04_Robotics/02_Costmap_Inflation/CostmapInflation.md
04_Robotics/03_Perception_Node/PerceptionNode.md
04_Robotics/03_Perception_Node/main.cpp
(no .cl or CMakeLists.txt files)
```

### Changed Files
| File | Change |
|------|--------|
| `04_Robotics/03_Perception_Node/main.cpp` | Deleted hand-rolled --help block (21 lines) |
| `04_Robotics/01_Node_Acceleration/NodeAcceleration.md` | Added `## Inspecting Parameters` (node: `accel_node`) |
| `04_Robotics/02_Costmap_Inflation/CostmapInflation.md` | Added `## Inspecting Parameters` (node: `costmap_node`) |
| `04_Robotics/03_Perception_Node/PerceptionNode.md` | Added `## Inspecting Parameters` (node: `perception_node`) |

### Remaining
- [x] A — main.cpp hand-rolled --help loop removed
- [x] B — All three READMEs have `## Inspecting Parameters` with correct node names
- [x] No .cl or CMakeLists.txt modified
- [x] --ros-args already present in all three READMEs (pre-existing)
- [ ] MANUAL: Live `ros2 param list /perception_node` verification
