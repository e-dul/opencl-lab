# Task 018: SyncAtomics Toolbox Tool

## Context
- **Design Feature:** `workflow/design/07-toolbox.md`
- **Milestone:** Phase 13 — SyncAtomics
- **Relevant Files:**
  - `workflow/design/07-toolbox.md` — Phase 13 spec, performance gate, directory structure (read-only)
  - `99_Toolbox/SyncAtomics/SyncAtomics.md` — user-facing README, already written (read-only)
  - `99_Toolbox/FastMath/CMakeLists.txt` — CMake style reference (read-only)
  - `99_Toolbox/FastMath/main.cpp` — host-code structure reference (read-only)
  - `common/ocl_wrapper.hpp` — `create_context()`, `OclContext` (read-only)
  - `common/opencl_utils.hpp` — `CL_CHECK`, `load_kernel_source`, `duration_ms` (read-only)
  - `99_Toolbox/SyncAtomics/CMakeLists.txt` — new file
  - `99_Toolbox/SyncAtomics/main.cpp` — new file
  - `99_Toolbox/SyncAtomics/kernels/histogram_kernel.cl` — new file

## Objective
Implement the SyncAtomics toolbox tool: a standalone C++17 binary that runs a 1M-element histogram in three kernel variants (unsafe non-atomic, global `atomic_add`, local-accumulate-then-merge), verifies correctness on host, and prints a structured timing table.

## Constraints & Rules
- Standard constraints from `.claude/rules/00_master_specs.md` apply (C++17, `cl.hpp`, CLI11, `create_context()`, `cl::Event` profiling, `CL_CHECK`, standalone CMake, kernel copy rule).
- **No BMP output.** SyncAtomics is a numeric tool. Structured console table is the sole required artifact.
- **Histogram parameters:** 256 bins, input element type `uchar` (values 0–255 map directly to bin index), default size 1,048,576 elements (1M). `--size` CLI flag controls the element count.
- **OpenCL atomics scope:** OpenCL 1.2 `atomic_add` operates on `__global int*` and `__local int*`. Do NOT use OpenCL 2.0 `atomic_fetch_add` with explicit memory order — that requires `cl_khr_int64_base_atomics` or OpenCL 2.0. Use only OpenCL 1.2 built-ins.
- **Unsafe variant intent:** The unsafe kernel intentionally omits atomics to demonstrate the data race. It will produce a bin sum less than the element count on real GPU hardware. The binary must not abort on this; it must print the mismatch and continue.
- **Local work-group size for local variant:** Use 256 as the local work size. This ensures each work-group's `local_hist` tile covers all 256 bins.
- **Performance profiling:** All three variant timings must come from `cl::Event` profiling. The unsafe run's event time is still valid for timing purposes even though its output is incorrect.

---

## Implementation

### 1. `99_Toolbox/SyncAtomics/CMakeLists.txt`

Mirror `99_Toolbox/FastMath/CMakeLists.txt` exactly, substituting:
- `project(sync_atomics CXX)`
- `add_executable(sync_atomics main.cpp)`
- Target name `sync_atomics` throughout (`opencl_lab_target`, `copy_kernels`, `target_include_directories`).
- Comment: `# common.cmake is two levels up from 99_Toolbox/SyncAtomics/`

### 2. `99_Toolbox/SyncAtomics/kernels/histogram_kernel.cl`

Three kernels in a single `.cl` file.

**Kernel A — `histogram_unsafe`**
- Parameters: `__global const uchar* data`, `__global int* histogram`, `int size`
- Each work-item reads `data[gid]` and writes `histogram[data[gid]]++` as a plain (non-atomic) read-modify-write.
- `gid` must be `size_t`; guard `if (gid < (size_t)size)`.
- WHY comment: explain that the naked `++` is a read-modify-write hazard, intentionally broken.

**Kernel B — `histogram_global_atomic`**
- Parameters: `__global const uchar* data`, `__global int* histogram`, `int size`
- Same structure but use `atomic_add(&histogram[data[gid]], 1)`.
- WHY comment: explain global atomic serialises all work-items touching the same bin across the entire device.

**Kernel C — `histogram_local`**
- Parameters: `__global const uchar* data`, `__global int* global_hist`, `__local int* local_hist`, `int size`
- Phase 1 (zero local): `for (int b = lid; b < 256; b += get_local_size(0)) local_hist[b] = 0;` then `barrier(CLK_LOCAL_MEM_FENCE)`.
- Phase 2 (accumulate locally): `if (gid < (size_t)size) atomic_add(&local_hist[data[gid]], 1);` then `barrier(CLK_LOCAL_MEM_FENCE)`.
- Phase 3 (merge to global): `for (int b = lid; b < 256; b += get_local_size(0)) atomic_add(&global_hist[b], local_hist[b]);`
- WHY comment on barrier placement: explain both barriers are mandatory — the first prevents reads of uninitialised local memory, the second prevents the merge from reading partial accumulation.

### 3. `99_Toolbox/SyncAtomics/main.cpp`

Follow the structural pattern of `99_Toolbox/FastMath/main.cpp`.

**CLI (CLI11):**
- `--size` → `int n_elements` (default 1,048,576). Throw `std::runtime_error` if `n_elements <= 0`.
- `--local-size` → `int local_size` (default 256). Used as the NDRange local work size for kernel C. Must divide `global_work_size`; round `n_elements` up to the next multiple of `local_size` when forming the global work size.

**Data generation:**
- Allocate `std::vector<cl_uchar>` of `n_elements` elements.
- Fill with `std::mt19937` + `std::uniform_int_distribution<int>(0, 255)`, seeded with 42.
- Compute a host reference histogram (`std::array<int, 256>`) from the same data — used for correctness.

**Buffer setup:**
- One `cl::Buffer` for input data: `CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR`.
- One `cl::Buffer` for the histogram output (256 `int` values): `CL_MEM_READ_WRITE`. Zero-filled before each variant dispatch (`enqueueWriteBuffer` with a `std::vector<int>(256, 0)`).

**Per-variant helper function `run_histogram_variant`:**
- Signature: returns `double` (kernel time in ms).
- Parameters: `cl::CommandQueue&`, `cl::Kernel&`, kernel-specific args, `cl::Buffer& hist_buf`, `int n_elements`, `int local_size`.
- Zero the histogram buffer before dispatch.
- For kernel C (`histogram_local`): set arg 2 as `cl::Local(256 * sizeof(int))` (the `__local` allocation), arg 3 as `cl_int n_elements`.
- For kernels A and B: two args (`buf_data`, `buf_hist`, `cl_int n_elements`).
- Enqueue with `cl::Event ev`; call `queue.finish()`; return `duration_ms(ev)`.

**Correctness check helper `check_histogram`:**
- Parameters: `cl::CommandQueue&`, `cl::Buffer& hist_buf`, `const std::array<int,256>& reference`, `int n_elements`.
- Read back the 256-int histogram with `enqueueReadBuffer`.
- Compute `int bin_sum = std::accumulate(gpu_hist.begin(), gpu_hist.end(), 0)`.
- Return a struct or tuple: `{bin_sum, bool correct}` where `correct = (bin_sum == n_elements)`.
- Do NOT throw on mismatch — unsafe variant is expected to fail.

**Main flow:**
1. Parse CLI.
2. `OclContext ocl = create_context();`
3. Recreate `ocl.queue` with `CL_QUEUE_PROFILING_ENABLE`. WHY comment: required for `cl::Event` timestamp queries.
4. Generate data, compute reference histogram.
5. Upload data buffer.
6. Build single `cl::Program` from `kernels/histogram_kernel.cl` (no variant flags needed — all three kernels are in one file). Surface build log on error.
7. Create three `cl::Kernel` objects: `histogram_unsafe`, `histogram_global_atomic`, `histogram_local`.
8. Run each variant in order: unsafe → global → local. Between each, zero the histogram buffer.
9. After each variant: call `check_histogram`, store result.
10. Print structured table (see format below).
11. Print correctness summary block.

**Output format:**

```
=== SyncAtomics: 1M-element histogram (256 bins) ===

Variant              Elements     Kernel (ms)     Speedup vs Global
-----------------------------------------------------------------
unsafe               1048576          X.XXX        Y.YYx  [CORRUPT — bins sum to N ≠ 1048576]
global_atomic        1048576          X.XXX        1.00x  [PASS — bins sum to 1048576]
local_reduce         1048576          X.XXX        Y.YYx  [PASS — bins sum to 1048576]

Speedup (local_reduce vs global_atomic): Y.YYx
```

- Speedup column: `global_atomic_ms / variant_ms`. Unsafe and local_reduce both express speedup relative to global_atomic.
- Correctness tag: `[PASS — bins sum to N]` or `[CORRUPT — bins sum to M ≠ N]`.
- Print final speedup line at the bottom.

**Correctness assertion (hard):**
After the table, assert that `global_atomic` and `local_reduce` both pass (i.e., their bin sums equal `n_elements`). If either fails, throw `std::runtime_error("Atomic variant produced incorrect histogram — driver bug or kernel error")`. Unsafe failure is expected and must NOT trigger this assertion.

---

## Definition of Done (DoD)

Standard items from `.claude/rules/00_master_specs.md` §8:
- [x] `cmake -B build && cmake --build build` from `99_Toolbox/SyncAtomics/` succeeds with zero errors and zero warnings.
- [x] `./build/sync_atomics` (no arguments) runs without error and prints the timing table.
- [x] `--help` prints CLI11-generated usage including `--size` and `--local-size`.
- [x] `GPU=<vendor> ./build/sync_atomics` selects the correct device without crashing.

Task-specific outcomes:
- [x] Three kernel variants are present in `kernels/histogram_kernel.cl`: `histogram_unsafe`, `histogram_global_atomic`, `histogram_local`.
- [x] Unsafe variant produces a bin sum less than `n_elements` on at least one real GPU run (non-deterministic mismatch), printed as `[CORRUPT]`. If the unsafe variant accidentally passes on a given run (serialised by driver), the binary must still complete without error — the CORRUPT tag is driven by the sum check, not an assertion.
- [x] Global atomic and local-reduce variants both produce `bin_sum == n_elements` and are tagged `[PASS]`.
- [x] Hard assertion fires (throws) if a correct variant produces a wrong sum.
- [x] Performance gate: `local_reduce` is at least **3× faster** than `global_atomic` on a discrete GPU (`GPU=AMD` or `GPU=NVIDIA`). If gate is not met, document in `workflow/design/07-toolbox.md` Known Issues with hardware context (mirror existing waiver pattern from FastMath / LocalMemory entries).
- [x] All kernel timings are from `cl::Event` profiling — no wall-clock times in the output table.
- [x] `--size 65536` (small input) runs cleanly and still produces correct histograms for atomic variants.

---

## Execution Report

- **Status:** COMPLETE
- **Session:** 2026-03-07
- **Hardware:** NVIDIA GeForce RTX 4060 Laptop GPU (NVIDIA CUDA platform)

### Validation
```
# cmake -B build && cmake --build build
[100%] Built target sync_atomics  (zero errors, zero warnings)

# ./build/sync_atomics
Platform : NVIDIA CUDA
Device   : NVIDIA GeForce RTX 4060 Laptop GPU

=== SyncAtomics: 1048576-element histogram (256 bins) ===

Variant              Elements     Kernel (ms)     Speedup vs Global
-----------------------------------------------------------------
unsafe               1048576      0.056           4.58x  [CORRUPT — bins sum to 12259 ≠ 1048576]
global_atomic        1048576      0.258           1.00x  [PASS — bins sum to 1048576]
local_reduce         1048576      0.045           5.73x  [PASS — bins sum to 1048576]

Speedup (local_reduce vs global_atomic): 5.73x

# ./build/sync_atomics --help
  --size INT            Number of uchar elements (default 1048576)
  --local-size INT      Local work-group size for local_reduce variant (default 256)

# ./build/sync_atomics --size 65536
=== SyncAtomics: 65536-element histogram (256 bins) ===
unsafe               65536        0.015           1.33x  [CORRUPT — bins sum to 952 ≠ 65536]
global_atomic        65536        0.020           1.00x  [PASS — bins sum to 65536]
local_reduce         65536        0.007           2.86x  [PASS — bins sum to 65536]
```

### Changed Files
| File | Change |
|------|--------|
| `99_Toolbox/SyncAtomics/CMakeLists.txt` | Created |
| `99_Toolbox/SyncAtomics/main.cpp` | Created |
| `99_Toolbox/SyncAtomics/kernels/histogram_kernel.cl` | Created |

### Remaining
- Performance gate MET: local_reduce achieved **5.73×** vs global_atomic on NVIDIA RTX 4060 Laptop GPU (threshold: 3×). No waiver needed.
