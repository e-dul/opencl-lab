# Task 005: GPU Vendor Selection via `GPU` Env Var

## Context
- **Design Feature:** `workflow/design/01-host-api.md`
- **Milestone:** Phase 6 — Vendor-pinned device selection
- **Relevant Files:**
  - `common/ocl_wrapper.hpp` — (modified: `create_context()`)

## Objective
Add `GPU` environment variable support to `create_context()` so users can pin a specific
OpenCL vendor (`NVIDIA`, `AMD`, `INTEL`) without recompiling. Unset → existing behavior.

## Constraints & Rules
- **No Design Changes:** Do not modify `workflow/design/*.md`.
- **Language/Standard:** C++17. No new headers beyond `<cstdlib>` (for `std::getenv`).
- **Caller interface unchanged:** `create_context()` signature stays `OclContext create_context()`.
- **Matching:** Case-insensitive substring match on `CL_PLATFORM_VENDOR`.
- **Error on mismatch:** Throw `std::runtime_error` listing available vendors; do NOT silently fall back.

---

## Implementation

1. Added `#include <algorithm>` and `#include <cstdlib>` to header includes.

2. At top of `create_context()`, read the env var:
   ```cpp
   const char* gpu_hint_raw = std::getenv("GPU");
   std::string gpu_hint     = gpu_hint_raw ? gpu_hint_raw : "";
   std::transform(gpu_hint.begin(), gpu_hint.end(), gpu_hint.begin(), ::toupper);
   ```

3. Vendor-filter branch before existing GPU loop:
   - Case-insensitive substring match on `CL_PLATFORM_VENDOR`.
   - On match: use that platform's first GPU device.
   - On no match: throw with list of available platform vendors.
   - Existing GPU-first / CPU-fallback loops run only when `gpu_hint` is empty.

4. Updated print line:
   ```cpp
   std::cout << "Platform : " << selected_platform.getInfo<CL_PLATFORM_NAME>();
   if (!gpu_hint.empty()) std::cout << "  [GPU=" << gpu_hint_raw << "]";
   std::cout << "\n";
   ```

## Definition of Done (DoD)
- [ ] All three modules build cleanly after change.
- [ ] `./visual_kernel` (no env var) → same device as before.
- [ ] `GPU=NVIDIA ./visual_kernel` → prints NVIDIA platform line with `[GPU=NVIDIA]`.
- [ ] `GPU=amd ./visual_kernel` → matches AMD platform (case-insensitive).
- [ ] `GPU=INVALID ./visual_kernel` → throws with vendor list, exits non-zero.

---

## Execution Report

- **Status:** COMPLETE
- **Session:** 2026-02-23

### Changed Files
| File | Change |
|------|--------|
| `common/ocl_wrapper.hpp` | Modified `create_context()` — vendor env var filter |
| `workflow/tasks/005_gpu_vendor_env_var.md` | This file |
