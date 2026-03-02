# Task 015: Toolbox — SVM Tool

## Context
- **Design Feature:** `workflow/design/07-toolbox.md`
- **Milestone:** Phase 10 — SVM (`99_Toolbox/SVM/`)
- **Relevant Files:**
  - `workflow/design/07-toolbox.md` — (read-only: architecture reference, §SVM constraints)
  - `.claude/rules/00_master_specs.md` — (read-only: global constraints)
  - `99_Toolbox/SVM/SVM.md` — (read-only: user-facing README, expected output format)
  - `common/opencl_utils.hpp` — (read-only: `CL_CHECK` macro definition — do NOT include directly; see OpenCL Version note below)
  - `common/ocl_wrapper.hpp` — (read-only: `GPU` env var convention — do NOT include directly; implement inline; see note below)
  - `common/common.cmake` — (read-only: CMake helper reference — do NOT call `opencl_lab_target()` for this tool; see note below)
  - `99_Toolbox/SVM/CMakeLists.txt` — (new file)
  - `99_Toolbox/SVM/main.cpp` — (new file)
  - `99_Toolbox/SVM/kernels/svm_kernel.cl` — (new file)

## Objective

Implement the `SVM` tool: a standalone C++17 binary (`svm_demo`) that benchmarks three OpenCL host-device data-sharing strategies — buffer + map/unmap baseline, coarse-grained SVM, and fine-grained SVM — on a synthetic float buffer workload, prints a structured before/after timing table, and falls back gracefully (exit 0) on OpenCL 1.2 devices.

## Constraints & Rules

- **No Design Changes:** Do not modify `workflow/design/*.md`. If architecture is wrong, stop and ask.
- **Language/Standard:** C++17. No C++20.
- **OpenCL Version Note (Critical):** `common/opencl_utils.hpp` hardcodes `CL_HPP_TARGET_OPENCL_VERSION 120`. SVM APIs (`clSVMAlloc`, `cl::SVMAllocator`, `CL_MEM_SVM_FINE_GRAIN_BUFFER`) require version 200. To avoid macro redefinition conflicts:
  - Do **NOT** include `common/opencl_utils.hpp` or `common/ocl_wrapper.hpp` in this tool.
  - In `main.cpp`, define version macros manually before including `<CL/opencl.hpp>`:
    ```cpp
    #define CL_HPP_TARGET_OPENCL_VERSION  200
    #define CL_HPP_MINIMUM_OPENCL_VERSION 120
    #define CL_HPP_ENABLE_EXCEPTIONS
    #include <CL/opencl.hpp>
    ```
  - Define `CL_CHECK(err)` locally in `main.cpp` (copy the macro from `common/opencl_utils.hpp`).
  - Implement `GPU` env var device selection inline (substring match on `CL_PLATFORM_VENDOR` / `CL_DEVICE_VENDOR`, case-insensitive). Hard-coded device indices are FORBIDDEN.
- **CMakeLists.txt Note:** Do NOT call `opencl_lab_target()` from `common/common.cmake` — it wires in the `common` interface target which propagates the version 120 defines. Instead:
  - Use `FetchContent` directly for CLI11 v2.4.2.
  - Link manually: `target_link_libraries(svm_demo PRIVATE OpenCL::OpenCL CLI11::CLI11)`.
  - Add `target_include_directories` for the `vendor/` directory (for `cl.hpp` / `opencl.hpp` if needed by the system).
- **OpenCL 2.0 Guard:** All SVM code paths must be inside `#ifdef CL_VERSION_2_0 ... #endif`. At runtime, query `CL_DEVICE_OPENCL_C_VERSION` and `CL_DEVICE_EXTENSIONS`. If SVM is not supported, print a descriptive message and `exit(0)`. Crash or silent hang is FORBIDDEN.
- **Fine-Grained SVM Guard:** Check `CL_DEVICE_SVM_CAPABILITIES` for `CL_DEVICE_SVM_FINE_GRAIN_BUFFER`. If unavailable, skip the fine-grained variant gracefully and note it in the output table.
- **CLI Parsing:** CLI11 v2.4.2 via FetchContent. Required flags: `--size` (element count, default 1048576 = 4 M floats), `--mode` (optional; `all`, `buffer_map`, `coarse_svm`, `fine_svm`; default `all`).
- **Profiling:** GPU kernel timing via `cl::Event` with `CL_QUEUE_PROFILING_ENABLE`. Host map/unmap overhead measured via `std::chrono::steady_clock` (no GPU event available for host-side map). Both must appear in the output table.
- **No BMP output:** SVM is a numeric/pipeline tool per design doc. Structured console table is the required artifact.
- **Error Handling:** `CL_CHECK(err)` on all OpenCL return codes. `std::runtime_error` on fatal errors.
- **Standalone Build:** `cmake -B build && cmake --build build` from `99_Toolbox/SVM/` must succeed with no parent CMake required. No warnings on GCC/Clang with `-Wall`.
- **Kernel Copy:** `copy_kernels` post-build command per master specs (copy `kernels/` to `$<TARGET_FILE_DIR:svm_demo>/kernels/`). Implement directly in `CMakeLists.txt` without calling `copy_kernels()` from `common.cmake`.

---

## Implementation

1. **Directory scaffold**: Create `99_Toolbox/SVM/` with `CMakeLists.txt`, `main.cpp`, and `kernels/svm_kernel.cl`.

2. **`CMakeLists.txt`**:
   - `cmake_minimum_required(VERSION 3.18)`, `project(SVM CXX)`, `set(CMAKE_CXX_STANDARD 17)`.
   - `find_package(OpenCL REQUIRED)`.
   - FetchContent for CLI11 v2.4.2.
   - Target: `svm_demo`, source: `main.cpp`.
   - `target_link_libraries(svm_demo PRIVATE OpenCL::OpenCL CLI11::CLI11)`.
   - Include path: `${CMAKE_CURRENT_SOURCE_DIR}/../../vendor` (for `cl.hpp` if system does not provide `<CL/opencl.hpp>`).
   - Post-build kernel copy:
     ```cmake
     add_custom_command(TARGET svm_demo POST_BUILD
       COMMAND ${CMAKE_COMMAND} -E copy_directory
               ${CMAKE_CURRENT_SOURCE_DIR}/kernels
               $<TARGET_FILE_DIR:svm_demo>/kernels
       COMMENT "Copying kernels for svm_demo"
     )
     ```

3. **`kernels/svm_kernel.cl`**:
   - Kernel name: `scale_add`.
   - Signature: `__kernel void scale_add(__global float* data, int n, float scale, float offset)`.
   - Operation: `data[i] = data[i] * scale + offset` for `i < n`.
   - Same kernel is used for all three modes. In the buffer+map and coarse SVM modes it reads from a `cl::Buffer` or SVM pointer passed as `__global float*`. No mode-specific kernel variants needed.

4. **`main.cpp` — startup and device capability check**:
   - Parse CLI args with CLI11: `--size` (default 1048576), `--mode` (default `all`).
   - Enumerate OpenCL platforms and devices. Select device respecting `GPU` env var (substring match on `CL_PLATFORM_VENDOR` / `CL_DEVICE_VENDOR`, case-insensitive; fallback to first GPU, then first CPU if no GPU found).
   - Print selected device name and OpenCL C version.
   - Query `CL_DEVICE_OPENCL_C_VERSION`. If version < 2.0:
     ```
     [INFO] Device reports OpenCL C 1.x. SVM is an OpenCL 2.0 feature.
     [INFO] Use the ZeroCopy tool (99_Toolbox/ZeroCopy/) for host-device transfer optimization on OpenCL 1.x.
     ```
     Then `exit(0)`.
   - Query `CL_DEVICE_SVM_CAPABILITIES`. Record which SVM levels are available: `CL_DEVICE_SVM_COARSE_GRAIN_BUFFER`, `CL_DEVICE_SVM_FINE_GRAIN_BUFFER`. Print a capability summary before the benchmark.

5. **`main.cpp` — Mode A: buffer+map (baseline)**:
   - Allocate `std::vector<float>` host data (size elements), fill with `i * 0.001f`.
   - Create `cl::Buffer` with `CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR`.
   - Create `cl::CommandQueue` with `CL_QUEUE_PROFILING_ENABLE`.
   - Time the map operation with `std::chrono::steady_clock`: call `queue.enqueueMapBuffer(buf, CL_TRUE, CL_MAP_READ | CL_MAP_WRITE, ...)` to get a host pointer. Unmap immediately.
   - Enqueue `scale_add` kernel on the buffer, capture `cl::Event`.
   - Call `queue.finish()`.
   - Map again for read-back, checksum first 4 elements, unmap.
   - Report: map time (chrono, ms), kernel time (`cl::Event`, ms).

6. **`main.cpp` — Mode B: coarse-grained SVM** (inside `#ifdef CL_VERSION_2_0`):
   - If `CL_DEVICE_SVM_COARSE_GRAIN_BUFFER` not in capabilities, print `[SKIP] Coarse SVM not supported` and skip.
   - Allocate with `clSVMAlloc(context(), CL_MEM_READ_WRITE, size * sizeof(float), 0)`. Store in `float*`.
   - Time the map operation (`clEnqueueSVMMap`) with `std::chrono::steady_clock`. Unmap with `clEnqueueSVMUnmap`.
   - Set kernel arg via `clSetKernelArgSVMPointer`. Enqueue `scale_add`, capture `cl::Event`.
   - `queue.finish()`.
   - Map for read, checksum, unmap.
   - Free SVM allocation with `clSVMFree`.
   - Report: map time (chrono, ms), kernel time (`cl::Event`, ms).

7. **`main.cpp` — Mode C: fine-grained SVM** (inside `#ifdef CL_VERSION_2_0`):
   - If `CL_DEVICE_SVM_FINE_GRAIN_BUFFER` not in capabilities, print `[SKIP] Fine-grained SVM not supported on this device` and skip.
   - Allocate with `clSVMAlloc(context(), CL_MEM_READ_WRITE | CL_MEM_SVM_FINE_GRAIN_BUFFER, size * sizeof(float), 0)`.
   - No explicit map/unmap needed. Write to the pointer directly on the host (fill with `i * 0.001f`).
   - Enqueue `scale_add`. No map overhead — report map time as `0.000 ms (no sync required)`.
   - Capture `cl::Event`, `queue.finish()`.
   - Read result directly from pointer. Checksum first 4 elements.
   - Free with `clSVMFree`.
   - Report: map time `0.000 ms`, kernel time (`cl::Event`, ms).

8. **`main.cpp` — output table**:
   Print in this format (ms to 3 decimal places):
   ```
   Device: Intel(R) UHD Graphics 770 (OpenCL C 3.0)
   Buffer size: 4194304 bytes (1048576 floats)

   SVM Capability: Coarse=YES  Fine-grain=YES

   ┌─────────────────────────┬──────────────┬──────────────┐
   │ Strategy                │ Map (ms)     │ Kernel (ms)  │
   ├─────────────────────────┼──────────────┼──────────────┤
   │ Buffer + Map/Unmap      │    X.XXX     │    X.XXX     │
   │ Coarse-grained SVM      │    X.XXX     │    X.XXX     │
   │ Fine-grained SVM        │    0.000 *   │    X.XXX     │
   └─────────────────────────┴──────────────┴──────────────┘
   * Fine-grained SVM requires no explicit synchronization.

   [PASS] All active variants produced correct output.
   ```

9. **Correctness check**:
   - For each mode, after running `scale_add` once with `scale=2.0f, offset=1.0f`, verify `result[0] == 2.0f * (0 * 0.001f) + 1.0f = 1.0f`. Tolerance: `fabs(result - expected) < 1e-4f`.
   - On mismatch, print `[FAIL] Mode <name>: result[0] = X.XXXXXX, expected Y.XXXXXX` and `exit(1)`.
   - On all pass, print `[PASS] All active variants produced correct output.`

---

## Definition of Done (DoD)

- [ ] `cmake -B build && cmake --build build` from `99_Toolbox/SVM/` succeeds with zero errors and zero warnings on GCC/Clang with `-Wall`.
- [ ] `./build/svm_demo --help` prints CLI11-generated usage listing `--size` and `--mode`.
- [ ] On an OpenCL 1.2-only device (or simulated via `GPU=<cpu>` env var), the binary prints the SVM-not-supported informational message and exits with code 0 — no crash or unhandled exception.
- [ ] On an OpenCL 2.0+ device with coarse-grained SVM, the output table shows `Buffer + Map/Unmap` and `Coarse-grained SVM` rows with non-zero kernel times from `cl::Event` profiling.
- [ ] If fine-grained SVM is unavailable, `[SKIP] Fine-grained SVM not supported` is printed without crashing.
- [ ] Correctness check prints `[PASS]` for all active variants (no `[FAIL]` output on correct hardware).
- [ ] All SVM code paths are inside `#ifdef CL_VERSION_2_0 ... #endif` guards — the binary compiles cleanly with `CL_HPP_TARGET_OPENCL_VERSION=120` by commenting out the `#define` to 200 (design doc constraint verification).
- [ ] No `output.bmp` is produced (numeric tool — console table is the artifact).
- [ ] `kernels/svm_kernel.cl` is present in `$<TARGET_FILE_DIR:svm_demo>/kernels/` after build (post-build copy verified by `ls build/kernels/`).
- [ ] Device selection respects `GPU` env var — `GPU=INTEL ./build/svm_demo` selects Intel device without error (or prints "no matching device" if absent).

---

## Execution Report
<!-- Filled by @coder after implementation. -->

- **Status:** PENDING
- **Session:** —

### Validation
```
[output here]
```

### Changed Files
| File | Change |
|------|--------|
| `99_Toolbox/SVM/CMakeLists.txt` | Created |
| `99_Toolbox/SVM/main.cpp` | Created |
| `99_Toolbox/SVM/kernels/svm_kernel.cl` | Created |

### Remaining
- [ ] Implementation by @coder
