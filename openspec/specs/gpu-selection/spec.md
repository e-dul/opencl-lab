# Capability: gpu-selection

Source: `00_master_specs.md` §5

## Overview

Device selection is controlled entirely by the `GPU` environment variable. Hard-coded platform or device indices are forbidden.

---

#### Scenario: GPU env var selects device

WHEN `GPU=<vendor>` is set (e.g. `GPU=NVIDIA`, `GPU=AMD`, `GPU=INTEL`)
THEN the binary selects the first device whose `CL_PLATFORM_VENDOR` or `CL_DEVICE_VENDOR` contains the substring (case-insensitive)

#### Scenario: Default device selection

WHEN `GPU` is not set
THEN the binary selects the first platform with a GPU
AND falls back to CPU if no GPU platform is found

#### Scenario: Hard-coded indices forbidden

WHEN module code selects a platform or device
THEN no hard-coded integer indices (e.g. `platforms[0]`, `devices[1]`) appear in module source files
AND device selection is delegated to `common/ocl_wrapper.hpp::create_context()`

#### Scenario: Mesa/rusticl stack support

WHEN the system uses a Mesa/rusticl OpenCL stack
THEN vendor substring matching against both `CL_PLATFORM_VENDOR` and `CL_DEVICE_VENDOR` allows correct device selection
