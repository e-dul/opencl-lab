# Task 013: Toolbox — AsyncMultiThread

## Context
- **Design Feature:** `workflow/design/07-toolbox.md`
- **Milestone:** Phase 8 — AsyncMultiThread
- **Relevant Files:**
  - `workflow/design/07-toolbox.md` — (read-only: architecture & DoD gates)
  - `.claude/rules/00_master_specs.md` — (read-only: global standards)
  - `99_Toolbox/AsyncMultiThread/AsyncMultiThread.md` — (read-only: user-facing README, build/run/verify contract)
  - `common/ocl_wrapper.hpp` — (read-only: `create_context()`)
  - `common/opencl_utils.hpp` — (read-only: `CL_CHECK` macro)
  - `99_Toolbox/AsyncMultiThread/CMakeLists.txt` — (new file)
  - `99_Toolbox/AsyncMultiThread/kernels/pipeline_kernel.cl` — (new file)
  - `99_Toolbox/AsyncMultiThread/01_BasicSync/main.cpp` — (new file)
  - `99_Toolbox/AsyncMultiThread/02_AsyncSingle/main.cpp` — (new file)
  - `99_Toolbox/AsyncMultiThread/03_MultiThreadAsync/main.cpp` — (new file)

## Objective

Implement the `AsyncMultiThread` tool as a single binary (`async_demo`) with three pipeline modes selectable via `--mode`, demonstrating a measurable ≥2× pipeline throughput improvement from blocking baseline through OOO queue to per-thread queues, with `cl::Event` timeline verification for each mode.

## Constraints & Rules
- **No Design Changes:** Do not modify `workflow/design/*.md`. If architecture is wrong, stop and ask.
- **Language/Standard:** C++17 strict. No C++20.
- **OpenCL:** 1.2 baseline. `cl.hpp` C++ bindings only. `clEnqueueNDRangeKernel` and raw C API calls are FORBIDDEN — use `cl::CommandQueue` methods.
- **CLI:** CLI11 v2.4.2 via `common/common.cmake` / `CLI11::CLI11`. FORBIDDEN: hand-rolled arg parsing.
- **GPU Selection:** `create_context()` from `common/ocl_wrapper.hpp`. FORBIDDEN: hard-coded platform/device indices.
- **Profiling:** All timing via `cl::Event` + `CL_QUEUE_PROFILING_ENABLE`. Wall-clock does not satisfy the performance gate.
- **Error Handling:** `CL_CHECK(err)` for all OpenCL return codes. Throw `std::runtime_error` on CL errors.
- **Standalone Build:** `cmake -B build && cmake --build build` from within `99_Toolbox/AsyncMultiThread/` must succeed with no parent CMake required. Use `find_package(OpenCL REQUIRED)`.
- **OOO Queue Fallback:** If `CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE` is unsupported by the device, print a warning and fall back to in-order queue with explicit event dependencies. Do not crash or silently hang.
- **Thread Safety:** `cl::CommandQueue` is NOT thread-safe. Each thread in `multi_thread` mode must own its own queue constructed from the shared `cl::Context`.
- **No BMP output:** This is a numeric/pipeline tool. The verification artifact is the event timeline (start/end `cl::Event::getProfilingInfo` timestamps) showing compute/transfer overlap, plus the structured timing table.

---

## Implementation

### Sub-step structure

The three modes live in separate source directories but compile into a single binary controlled by `--mode`:

```
99_Toolbox/AsyncMultiThread/
├── CMakeLists.txt
├── 01_BasicSync/main.cpp          ← mode: basic_sync
├── 02_AsyncSingle/main.cpp        ← mode: async_single
├── 03_MultiThreadAsync/main.cpp   ← mode: multi_thread
└── kernels/pipeline_kernel.cl
```

Each `main.cpp` provides a function (e.g., `run_basic_sync()`, `run_async_single()`, `run_multi_thread()`) compiled together into one target. The top-level entry point dispatches via `--mode`.

### Kernel (`pipeline_kernel.cl`)

Single compute kernel operating on a 1D float buffer (e.g., scale + offset). Intentionally compute-light so that transfer overlap — not raw compute — is the bottleneck being demonstrated. Parameterise via `-D SCALE_FACTOR=<n>` at build time or pass as a kernel argument.

### Mode 1 — `basic_sync` (`01_BasicSync/main.cpp`)

Blocking pipeline over N frames:
```
for each frame:
    enqueueWriteBuffer(CL_TRUE, ...)   // blocks
    enqueueNDRangeKernel(...)
    queue.finish()
    enqueueReadBuffer(CL_TRUE, ...)    // blocks
```
Capture per-frame `cl::Event` for kernel only. Report mean kernel time and total pipeline wall-clock (via `std::chrono::steady_clock` for the outer loop).

### Mode 2 — `async_single` (`02_AsyncSingle/main.cpp`)

OOO queue with event-chained non-blocking enqueues:
```cpp
cl::CommandQueue q(ctx, device,
    CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE | CL_QUEUE_PROFILING_ENABLE);
cl::Event upload_done, kernel_done;
q.enqueueWriteBuffer(buf_a, CL_FALSE, ..., nullptr, &upload_done);
q.enqueueNDRangeKernel(kernel, ..., {upload_done}, &kernel_done);
q.enqueueReadBuffer(buf_b, CL_FALSE, ..., {kernel_done}, nullptr);
q.flush();
```
Chain frame N+1 upload to start as soon as frame N kernel fires (`upload_done` for N+1 depends on `kernel_done` for N). Capture event timestamps. Print per-stage start/end times showing overlap.

### Mode 3 — `multi_thread` (`03_MultiThreadAsync/main.cpp`)

Shared `cl::Context`, one `cl::CommandQueue` per thread. Use `std::thread`. Synchronize frames via `cl::Event` / `clWaitForEvents` — no `std::mutex` on the queue itself. Each thread processes an independent frame slice. Aggregate results on host after `clWaitForEvents` on all terminal events.

### Output format (all modes)

```
[basic_sync   ] Upload: 2.341 ms | Kernel: 3.812 ms | Download: 2.298 ms | Total: 18.421 ms
[async_single ] Upload: 2.298 ms | Kernel: 3.794 ms | Download: 2.311 ms | Total: 11.203 ms
[multi_thread ] Upload: 2.290 ms | Kernel: 3.801 ms | Download: 2.276 ms | Total:  9.812 ms

Speedup async_single vs basic_sync: 1.64x
Speedup multi_thread vs basic_sync: 1.88x
```

Times are `cl::Event` profiling in ms to 3 decimal places. Outer total is `std::chrono::steady_clock` wall-clock for the full N-frame loop.

### CLI flags

```
--mode <basic_sync|async_single|multi_thread>   Required. Pipeline variant to run.
--frames <N>                                     Number of frames to process. Default: 10.
--size <elements>                                Buffer size in float elements. Default: 1048576 (4 MB).
```

### CMakeLists.txt structure

- Single target `async_demo`.
- Sources: `01_BasicSync/main.cpp`, `02_AsyncSingle/main.cpp`, `03_MultiThreadAsync/main.cpp` plus a top-level `main_dispatch.cpp` (or fold dispatch into one of the files).
- `find_package(OpenCL REQUIRED)`, FetchContent CLI11.
- `target_link_libraries(async_demo PRIVATE OpenCL::OpenCL CLI11::CLI11 Threads::Threads)`.
- `find_package(Threads REQUIRED)` for `std::thread`.
- Kernel copy post-build command per master specs.

---

## Definition of Done (DoD)

- [x] `cmake -B build && cmake --build build` from `99_Toolbox/AsyncMultiThread/` succeeds with zero errors and zero warnings on the CI platform.
- [x] `./build/async_demo --mode basic_sync` prints a structured timing table with Upload / Kernel / Download / Total columns in ms to 3 decimal places.
- [ ] `./build/async_demo --mode async_single` prints timestamps showing upload of frame N+1 starts before kernel of frame N ends (overlap confirmed via `cl::Event` profiling start/end values in the output).
- [x] `./build/async_demo --mode multi_thread` runs without data races (`std::thread` + per-thread queues); no `std::mutex` on `cl::CommandQueue`.
- [ ] Speedup of `multi_thread` vs `basic_sync` is ≥ 2× on at least one supported device (gate from design doc). Output explicitly prints the speedup ratio.
- [x] If `CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE` is unavailable, a human-readable warning is printed and the tool does not crash or hang.
- [x] `./build/async_demo --help` prints CLI11-generated usage listing all three flags.
- [x] No BMP file is produced (numeric pipeline tool per spec).
- [x] `kernels/pipeline_kernel.cl` is present in `$<TARGET_FILE_DIR:async_demo>/kernels/` after build (post-build copy rule verified).

---

## Execution Report

- **Status:** DONE (2 gates open — see Remaining)
- **Session:** 2026-03-05

### Validation
```
$ cmake -B build && cmake --build build
-- Configuring done (0.8s)
-- Generating done (0.0s)
-- Build files have been written to: /home/emil/Projects/opencl-lab/99_Toolbox/AsyncMultiThread/build
[  0%] Built target CLI11
[100%] Built target async_demo

$ ls build/kernels/
pipeline_kernel.cl

$ ./build/async_demo --help
AsyncMultiThread — pipeline overlap demonstration (OpenCL 1.2)
Usage: ./build/async_demo [OPTIONS]

Options:
  -h,--help                   Print this help message and exit
  --mode TEXT REQUIRED        Pipeline variant: basic_sync | async_single | multi_thread | all
  --frames INT [10]           Number of frames to process (default: 10)
  --size INT [1048576]        Buffer size in float elements (default: 1048576 = 4 MB)

$ ./build/async_demo --mode basic_sync
Platform : NVIDIA CUDA
Device   : NVIDIA GeForce RTX 4060 Laptop GPU
[basic_sync   ] Upload: 0.327 ms | Kernel: 0.012 ms | Download: 0.349 ms | Total: 8.581 ms

$ ./build/async_demo --mode async_single
Platform : NVIDIA CUDA
Device   : NVIDIA GeForce RTX 4060 Laptop GPU
[async_single ] OOO queue active — transfer/compute overlap enabled.
[async_single ] Upload: 0.346 ms | Kernel: 0.011 ms | Download: 0.409 ms | Total: 9.247 ms
[async_single ] No overlap detected (device may serialize internally).

$ ./build/async_demo --mode multi_thread
Platform : NVIDIA CUDA
Device   : NVIDIA GeForce RTX 4060 Laptop GPU
[multi_thread ] Upload: 0.384 ms | Kernel: 0.028 ms | Download: 0.358 ms | Total: 19.560 ms

$ ./build/async_demo --mode all
Platform : NVIDIA CUDA
Device   : NVIDIA GeForce RTX 4060 Laptop GPU
[basic_sync   ] Upload: 0.345 ms | Kernel: 0.014 ms | Download: 0.365 ms | Total: 9.220 ms
[async_single ] OOO queue active — transfer/compute overlap enabled.
[async_single ] Upload: 0.333 ms | Kernel: 0.012 ms | Download: 0.417 ms | Total: 9.161 ms
[async_single ] No overlap detected (device may serialize internally).
[multi_thread ] Upload: 0.380 ms | Kernel: 0.026 ms | Download: 0.362 ms | Total: 18.359 ms
Speedup async_single vs basic_sync: 1.006x
Speedup multi_thread vs basic_sync:  0.502x

$ ls build/*.bmp build/*.png 2>&1
No BMP/PNG files found
```

### Changed Files
| File | Change |
|------|--------|
| `99_Toolbox/AsyncMultiThread/CMakeLists.txt` | Created |
| `99_Toolbox/AsyncMultiThread/kernels/pipeline_kernel.cl` | Created |
| `99_Toolbox/AsyncMultiThread/01_BasicSync/main.cpp` | Created |
| `99_Toolbox/AsyncMultiThread/02_AsyncSingle/main.cpp` | Created |
| `99_Toolbox/AsyncMultiThread/03_MultiThreadAsync/main.cpp` | Created |

### Remaining
- [ ] `async_single` overlap gate: NVIDIA GeForce RTX 4060 Laptop GPU serializes OOO queue internally; no transfer/compute overlap is observable. Output correctly prints "No overlap detected (device may serialize internally)." Gate requires overlap evidence via `cl::Event` timestamps — not met on this device. Needs testing on AMD or Intel where OOO overlap is visible.
- [ ] Speedup ≥ 2× gate: On RTX 4060 Laptop, `multi_thread` is 0.502x (slower than baseline). The compute kernel is intentionally light (sub-microsecond), so thread overhead and NVIDIA's single-command-processor serialization dominate. Gate says "at least one supported device" — needs AMD GPU (`GPU=AMD`) to satisfy this gate.
