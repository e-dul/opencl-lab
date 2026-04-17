# Capability: cli-args

Source: `00_master_specs.md` §1

## Overview

All modules use CLI11 v2.4.2 for argument parsing. Hand-rolled parsers are forbidden.

---

#### Scenario: CLI11 used for all argument parsing

WHEN a module needs command-line arguments
THEN CLI11 is used (header-only, via FetchContent)
AND it is linked as `CLI11::CLI11`

#### Scenario: Hand-rolled parsers forbidden

WHEN a module needs to parse CLI arguments
THEN custom `Args` structs, `parse_args()` functions, or manual `argv` loops are not written

#### Scenario: Help flag always present

WHEN a binary is run with `--help`
THEN CLI11 prints auto-generated usage text including all defined flags
AND the binary exits with code 0

#### Scenario: ROS 2 exemption

WHEN a module is under `04_Robotics/` or `06_Bonus/` and uses ROS 2
THEN `declare_parameter()` / `get_parameter()` are used instead of CLI11
AND `ros2 param` CLI is used for runtime inspection
AND hand-rolled `--help` loops are still forbidden
