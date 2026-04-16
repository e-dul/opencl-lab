# Capability: performance

Source: `00_master_specs.md` §6

## Overview

GPU timing uses `cl::Event` profiling. Wall-clock measurements do not satisfy performance gates. CPU stages use `std::chrono::steady_clock`.

---

#### Scenario: GPU stage timing

WHEN an "Optimized" milestone measures GPU kernel execution time
THEN timing uses `cl::Event` with `CL_PROFILING_COMMAND_START` and `CL_PROFILING_COMMAND_END`
AND wall-clock time (`std::chrono`) is not used for the GPU gate

#### Scenario: CPU stage timing

WHEN host-side logic (serialization, data preparation, publish) is timed
THEN `std::chrono::steady_clock` is used

#### Scenario: Time reporting format

WHEN execution time is reported to the user
THEN it is expressed in milliseconds to 3 decimal places (e.g. `12.345 ms`)

#### Scenario: Profiling mandatory for Optimized milestones

WHEN a submodule is an "Optimized" variant (e.g. `02_Optimized/`)
THEN `cl_event` profiling is enabled on the command queue
AND profiling data is printed to stdout

#### Scenario: Hardware performance waiver

WHEN a performance gate cannot be met on a specific device
THEN the gate includes an explicit hardware-waiver clause rather than a fixed speedup ratio
AND `GPU=AMD` is tried before concluding a gate failure
