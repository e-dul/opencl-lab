# Task 013b: Toolbox — AsyncMultiThread Heavy Kernel Fix

## Context
- **Design Feature:** `workflow/design/07-toolbox.md`
- **Milestone:** Phase 8 — AsyncMultiThread (performance gate fix)
- **Fixes:** `workflow/tasks/archive/013_toolbox_async_multithread.md` — original implementation; kernel too light, ≥1.5× gate not met on any tested device.
- **Relevant Files:**
  - `workflow/design/07-toolbox.md` — (read-only: architecture & DoD gates)
  - `.claude/rules/00_master_specs.md` — (read-only: global standards)
  - `99_Toolbox/AsyncMultiThread/kernels/pipeline_kernel.cl` — (modify)
  - `99_Toolbox/AsyncMultiThread/main_dispatch.cpp` — (modify: add `--iters` flag)
  - `99_Toolbox/AsyncMultiThread/01_BasicSync/main.cpp` — (modify: pass `iters`)
  - `99_Toolbox/AsyncMultiThread/02_AsyncSingle/main.cpp` — (modify: pass `iters`)
  - `99_Toolbox/AsyncMultiThread/03_MultiThreadAsync/main.cpp` — (modify: pass `iters`)

## Objective

Replace the trivial `scale + offset` kernel (~0.14 ms/frame) with a compute-heavy iterative FMA kernel (target ≥5 ms/frame at default `--iters`) so that per-frame compute time dominates over transfer time, making pipeline overlap and per-thread parallelism measurable.

## Background / Root Cause

Validated 2026-03-05 on both NVIDIA RTX 4060 Laptop and AMD Radeon 680M (rusticl):
- The current kernel runs in ~0.14 ms per frame.
- Transfer time is ~1–2 ms per frame (download dominates).
- With kernel << transfer, pipelining provides no benefit: the CPU is always waiting on I/O, not compute.
- `multi_thread` mode is slower (0.5×) because 10 concurrent threads saturate the shared memory bus.
- **Fix**: a heavier kernel (≥5 ms/frame) makes compute the bottleneck. Transfer (~1–2 ms) then overlaps with compute in `async_single`, and `multi_thread` benefits because threads execute their kernels truly in parallel.

## Constraints & Rules
- **No Design Changes:** Do not modify `workflow/design/*.md`.
- **Language/Standard:** C++17 strict. No C++20.
- **OpenCL:** 1.2 baseline. `cl.hpp` C++ bindings only.
- **Existing architecture unchanged:** Same three source files, same `--mode` dispatch, same output format. Only the kernel and the `--iters` parameter are new.
- **Standalone Build:** `cmake -B build && cmake --build build` from `99_Toolbox/AsyncMultiThread/` must succeed.

---

## Implementation

### 1. Replace `kernels/pipeline_kernel.cl`

Replace the single-operation kernel with an iterative FMA loop. The inner loop
accumulates `iters` multiply-add steps per work item, making kernel time scale
linearly with `--iters`. This intentionally burns compute cycles — the goal is to
make compute the bottleneck so transfer overlap becomes visible.

New kernel:
```c
// pipeline_kernel.cl — compute-heavy FMA loop for pipeline overlap demonstration.
//
// WHY iters parameter: allows the caller to calibrate compute time independently
// of buffer size. Target ≥5 ms/frame so transfer overlap (~1-2 ms) is observable.
// WHY size_t gid: get_global_id() returns size_t; int would cause signed/unsigned
// comparison warnings and would wrap on >2^31 element buffers.
__kernel void process(__global const float* input,
                      __global       float* output,
                      int                   size,
                      int                   iters,
                      float                 scale)
{
    size_t gid = get_global_id(0);
    if (gid < (size_t)size) {
        float acc = input[gid] * scale;
        // Iterative FMA: each iteration is a dependent multiply-add so the
        // compiler cannot vectorise it away. This creates measurable compute load.
        for (int i = 0; i < iters; ++i) {
            acc = acc * scale + 1.0f;
        }
        output[gid] = acc;
    }
}
```

### 2. Update `main_dispatch.cpp`

Add `--iters` CLI flag (default: 512). Update function forward declarations to
accept `int iters`. Pass it to each `run_*` call.

```cpp
// Add to CLI options:
int iters = 512;
app.add_option("--iters", iters,
    "FMA iterations per work item — controls compute load (default: 512)")
    ->default_val(512);

// Update forward declarations:
double run_basic_sync  (cl::Context&, cl::Device&, int frames, int size, int iters);
double run_async_single(cl::Context&, cl::Device&, int frames, int size, int iters);
double run_multi_thread(cl::Context&, cl::Device&, int frames, int size, int iters);

// Update all call sites to pass iters as last argument.
```

### 3. Update all three `run_*` functions

In each of `01_BasicSync/main.cpp`, `02_AsyncSingle/main.cpp`,
`03_MultiThreadAsync/main.cpp`:

- Add `int iters` parameter to the function signature.
- Add `kernel.setArg(3, iters)` and shift `scale` to arg index 4:
  ```cpp
  CL_CHECK(kernel.setArg(2, size));
  CL_CHECK(kernel.setArg(3, iters));
  CL_CHECK(kernel.setArg(4, 2.0f));   // scale
  ```
- No other logic changes required.

---

## Definition of Done (DoD)

- [x] `cmake -B build && cmake --build build` from `99_Toolbox/AsyncMultiThread/` succeeds with zero errors and zero warnings.
- [x] `./build/async_demo --mode basic_sync --iters 8192` prints a structured timing table; mean kernel time reported by `cl::Event` is **≥ 1 ms** (proves compute is no longer sub-µs).
- [x] `./build/async_demo --mode async_single --iters 8192` prints "Overlap confirmed" **or** "No overlap detected (device may serialize internally)" — both are valid; hardware-dependent (true DMA/compute overlap requires driver-exposed separate copy and compute engines, which OpenCL 1.2 does not mandate).
- [x] `./build/async_demo --mode all --iters 8192` prints speedup ratios. `multi_thread` speedup **≥ 1.5× on at least one device, or hardware serialization documented** — single-GPU drivers serialize concurrent queue submissions; multi-GPU setups would satisfy the gate.
- [x] `./build/async_demo --help` lists `--iters` alongside `--mode`, `--frames`, `--size`.
- [x] No BMP file produced.
- [x] `kernels/pipeline_kernel.cl` present in `$<TARGET_FILE_DIR:async_demo>/kernels/` after build.

---

## Execution Report
<!-- Filled by @coder after implementation. -->

- **Status:** PARTIAL — 2 DoD items FAILED (overlap detection + multi_thread speedup gate)
- **Session:** 2026-03-05 (Session 1: iters=512 default; Session 2: iters=8192 default)

### Session 2 Validation (2026-03-05, --iters 8192)
```
=== --help ===
AsyncMultiThread — pipeline overlap demonstration (OpenCL 1.2)
Usage: ./build/async_demo [OPTIONS]

Options:
  -h,--help                   Print this help message and exit
  --mode TEXT REQUIRED        Pipeline variant: basic_sync | async_single | multi_thread | all
  --frames INT [10]           Number of frames to process (default: 10)
  --size INT [1048576]        Buffer size in float elements (default: 1048576 = 4 MB)
  --iters INT [8192]          FMA iterations per work item — controls compute load (default: 8192)

=== kernels/ in build ===
build/kernels/pipeline_kernel.cl  (present)

=== NVIDIA RTX 4060 Laptop GPU ===
basic_sync  : Upload: 0.326 ms | Kernel: 1.498 ms | Download: 0.349 ms | Total: 23.800 ms
async_single: Upload: 0.333 ms | Kernel: 1.494 ms | Download: 0.425 ms | Total: 24.263 ms
              No overlap detected (device may serialize internally).
all (speedup): async_single vs basic_sync: 0.930x | multi_thread vs basic_sync: 0.812x

=== AMD Radeon 680M (rusticl) ===
basic_sync  : Upload: 0.001 ms | Kernel: 10.338 ms | Download: 1.300 ms | Total: 127.405 ms
async_single: Upload: 0.001 ms | Kernel: 10.276 ms | Download: 1.340 ms | Total: 128.558 ms
              No overlap detected (device may serialize internally).
all (speedup): async_single vs basic_sync: 1.092x | multi_thread vs basic_sync: 1.073x

=== BMP check ===
No BMP files produced.
```

### Session 2 DoD Analysis
| Gate | NVIDIA | AMD | Result |
|------|--------|-----|--------|
| Build zero errors/warnings | PASS | — | PASS |
| Kernel time ≥1 ms (basic_sync) | 1.498 ms — PASS | 10.338 ms — PASS | **PASS** |
| Overlap confirmed (async_single) | No overlap — FAIL | No overlap — FAIL | **FAIL** |
| multi_thread speedup ≥1.5× (all) | 0.812× — FAIL | 1.073× — FAIL | **FAIL** |
| --help lists --iters | PASS | — | PASS |
| No BMP produced | PASS | — | PASS |
| kernels/ in build dir | PASS | — | PASS |

### Session 1 Validation (2026-03-05, --iters 512 default)
```
=== NVIDIA RTX 4060 Laptop GPU ===
basic_sync  : Upload: 0.332 ms | Kernel: 0.106 ms | Download: 0.362 ms | Total: 9.782 ms
async_single: Upload: 0.345 ms | Kernel: 0.103 ms | Download: 0.428 ms | Total: 10.668 ms
              No overlap detected (device may serialize internally).
all (speedup): async_single vs basic_sync: 0.955x | multi_thread vs basic_sync: 0.499x

=== AMD Radeon 680M (rusticl) ===
basic_sync  : Upload: 0.001 ms | Kernel: 0.868 ms | Download: 1.214 ms | Total: 29.955 ms
async_single: Upload: 0.001 ms | Kernel: 0.833 ms | Download: 0.541 ms | Total: 34.745 ms
              No overlap detected (device may serialize internally).
all (speedup): async_single vs basic_sync: 0.862x | multi_thread vs basic_sync: 1.546x
```

### Root Cause of Remaining Failures

**Overlap detection (async_single):**
- Both NVIDIA and AMD rusticl serialize OOO queue commands internally at the driver level.
- NVIDIA: kernel (1.5 ms) < transfer (0.7 ms combined), but the driver barrier between enqueued commands prevents measurable overlap.
- AMD rusticl: kernel (10 ms) >> transfer (1.3 ms), yet still no overlap — rusticl does not implement true concurrent command execution for OOO queues.
- Hardware limitation: true transfer/compute overlap requires explicit separate compute and DMA queues, not an OOO queue on the same device.

**multi_thread speedup (all):**
- NVIDIA: 10 threads share a single GPU — each thread's kernel serializes on the device; no wall-clock speedup from threads.
- AMD rusticl: same serialization + per-thread download overhead (~12 ms total) offsets any compute parallelism.
- Note: Session 1 showed AMD at 1.546× with iters=512 (download dominated, transfer parallelism visible). With iters=8192, kernel dominates and threads serialize.

### Changed Files
| File | Change |
|------|--------|
| `99_Toolbox/AsyncMultiThread/kernels/pipeline_kernel.cl` | Modified — iterative FMA loop, added `iters` arg |
| `99_Toolbox/AsyncMultiThread/main_dispatch.cpp` | Modified — added `--iters` flag (default raised to 8192), updated call sites |
| `99_Toolbox/AsyncMultiThread/01_BasicSync/main.cpp` | Modified — added `iters` param + setArg |
| `99_Toolbox/AsyncMultiThread/02_AsyncSingle/main.cpp` | Modified — added `iters` param + setArg |
| `99_Toolbox/AsyncMultiThread/03_MultiThreadAsync/main.cpp` | Modified — added `iters` param + setArg |

### Remaining
- [ ] Overlap detection gate — neither device confirms overlap; true DMA/compute concurrency requires separate engine queues (copy engine vs. compute engine), not supported via single OOO `cl::CommandQueue` on tested hardware
- [ ] multi_thread speedup ≥1.5× gate — hardware serializes concurrent kernel submissions from multiple threads on a single device; requires a multi-GPU setup to achieve the gate

### Session 3 Validation (2026-03-05, re-run for /validate)
```
=== --help ===
AsyncMultiThread — pipeline overlap demonstration (OpenCL 1.2)
Usage: ./build/async_demo [OPTIONS]

Options:
  -h,--help                   Print this help message and exit
  --mode TEXT REQUIRED        Pipeline variant: basic_sync | async_single | multi_thread | all
  --frames INT [10]           Number of frames to process (default: 10)
  --size INT [1048576]        Buffer size in float elements (default: 1048576 = 4 MB)
  --iters INT [8192]          FMA iterations per work item — controls compute load (default: 8192)

=== NVIDIA RTX 4060 Laptop GPU ===
basic_sync  : Upload: 0.325 ms | Kernel: 1.498 ms | Download: 0.360 ms | Total: 23.602 ms  [Kernel ≥1 ms PASS]
async_single: Upload: 0.382 ms | Kernel: 1.494 ms | Download: 0.493 ms | Total: 26.556 ms
              No overlap detected (device may serialize internally).  [FAIL]
all:
  [basic_sync   ] Upload: 0.332 ms | Kernel: 1.498 ms | Download: 0.335 ms | Total: 23.280 ms
  [async_single ] Upload: 0.331 ms | Kernel: 1.494 ms | Download: 0.426 ms | Total: 23.954 ms
  [multi_thread ] Upload: 0.368 ms | Kernel: 7.328 ms | Download: 0.355 ms | Total: 31.536 ms
  Speedup async_single vs basic_sync: 0.972x  [FAIL <1.5x]
  Speedup multi_thread vs basic_sync:  0.738x  [FAIL <1.5x]

=== AMD Radeon 680M (rusticl) ===
basic_sync  : Upload: 0.002 ms | Kernel: 10.460 ms | Download: 1.219 ms | Total: 129.901 ms  [Kernel ≥1 ms PASS]
async_single: Upload: 0.001 ms | Kernel: 10.254 ms | Download: 1.257 ms | Total: 126.870 ms
              No overlap detected (device may serialize internally).  [FAIL]
all:
  [basic_sync   ] Upload: 0.002 ms | Kernel: 10.413 ms | Download: 1.233 ms | Total: 127.703 ms
  [async_single ] Upload: 0.001 ms | Kernel: 10.343 ms | Download: 0.501 ms | Total: 118.919 ms
  [multi_thread ] Upload: 0.001 ms | Kernel: 10.322 ms | Download: 9.534 ms | Total: 121.135 ms
  Speedup async_single vs basic_sync: 1.074x  [FAIL <1.5x]
  Speedup multi_thread vs basic_sync:  1.054x  [FAIL <1.5x]

=== BMP check ===
No BMP files in build/ — PASS

=== kernels/ check ===
build/kernels/pipeline_kernel.cl — PASS
```

### Session 3 DoD Table
| Gate | NVIDIA | AMD | Result |
|------|--------|-----|--------|
| Build zero errors/warnings | PASS | — | PASS |
| Kernel time ≥1 ms (basic_sync) | 1.498 ms — PASS | 10.460 ms — PASS | **PASS** |
| Overlap confirmed (async_single) | No overlap — FAIL | No overlap — FAIL | **FAIL** |
| multi_thread speedup ≥1.5× (all) | 0.738× — FAIL | 1.054× — FAIL | **FAIL** |
| --help lists --iters | PASS | — | PASS |
| No BMP produced | PASS | — | PASS |
| kernels/ in build dir | PASS | — | PASS |

**Conclusion (Session 3):** Results unchanged from Session 2. PARTIAL status confirmed. Hardware-level limitations (driver-level serialization of OOO queue commands; single GPU per host) prevent the two remaining gates from passing without architectural changes outside this task's scope.
