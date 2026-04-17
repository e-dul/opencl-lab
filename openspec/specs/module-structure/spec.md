# Capability: module-structure

Source: `00_master_specs.md` §2

## Overview

Submodule directories follow a fixed `NN_Title_Snake_Case` naming convention. Renumbering is forbidden. Third-party dependencies arrive via FetchContent only.

---

#### Scenario: Submodule directory naming

WHEN a submodule directory is created
THEN it is named `NN_Title_Snake_Case` where `NN` is a zero-padded two-digit number scoped per parent module

#### Scenario: Renumbering forbidden

WHEN a submodule is removed or a gap is created in the numbering sequence
THEN existing entries are NOT renumbered
AND gaps in the sequence are acceptable

#### Scenario: No vendor directory

WHEN a third-party header-only library is needed
THEN it is fetched via CMake FetchContent at configure time
AND no `vendor/` directory is created
