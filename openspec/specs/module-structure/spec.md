# Capability: module-structure

Source: `00_master_specs.md` §2; `CLAUDE.md` Code Conventions

## Overview

Module and submodule directories follow a fixed naming convention. Snapshots are used instead of branches. Dependencies are header-only via FetchContent.

---

#### Scenario: Submodule directory naming

WHEN a submodule directory is created
THEN it is named `NN_Title_Snake_Case` where `NN` is a zero-padded two-digit number scoped per parent module

#### Scenario: Renumbering forbidden

WHEN a submodule is removed or a gap is created in the numbering sequence
THEN existing entries are NOT renumbered
AND gaps in the sequence are acceptable

#### Scenario: Executable target naming

WHEN an executable target is named in `add_executable()`
THEN it uses `snake_case`
AND no module prefix (`A1_`, `b2_`) or `_demo` suffix is used

#### Scenario: CMake project naming

WHEN the `project()` macro is called in a submodule CMakeLists.txt
THEN the name is `PascalCase` matching the submodule directory name with numeric prefix stripped

#### Scenario: Acronym casing

WHEN a well-known technical initialism is used in a name (SVM, BVH, YUV, ISP, DNN)
THEN it is written ALLCAPS
AND product/library names preserve upstream spelling (OpenCV, OpenVINO, CLBlast, VkFFT)

#### Scenario: Snapshot evolution

WHEN a module adds a more optimized or extended version of existing code
THEN the new variant is placed in a new numbered subdirectory (e.g. `02_Optimized/`)
AND the original is not modified

#### Scenario: Variable naming

WHEN a C++ variable is named
THEN it uses `snake_case`

#### Scenario: Class naming

WHEN a C++ class or struct is named
THEN it uses `PascalCase`

#### Scenario: Constant naming

WHEN a C++ constant or `#define` macro is named
THEN it uses `UPPER_CASE`

#### Scenario: No vendor directory

WHEN a third-party header-only library is needed
THEN it is fetched via CMake FetchContent at configure time
AND no `vendor/` directory is created
