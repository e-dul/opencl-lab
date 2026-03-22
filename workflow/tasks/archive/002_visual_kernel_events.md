# Task 002: Visual Kernel Events — Add Event-Based Profiling

## Context
- **Design Feature:** `workflow/design/01-host-api.md`
- **Milestone:** Phase 2 — Visual Kernel Events ("Measure Everything")
- **Relevant Files:**
  - `01_Host_API/01_Visual_Kernel/src/main.cpp` (Phase 1 base — read before coding)
  - `01_Host_API/01_Visual_Kernel/CMakeLists.txt` (template to copy)
  - `01_Host_API/01_Visual_Kernel/kernels/mad.cl` (copy verbatim)
  - `common/ocl_wrapper.hpp` (`create_context()`, `OclContext` struct)
  - `01_Host_API/HostAPI.md` (user-facing verification section)

## Objective
Copy Phase 1 into `02_Visual_Kernel_Events/` and add `cl_event` profiling to measure Upload → Kernel → Download latency, printing a timing breakdown to stdout.

## Constraints & Rules
- **No Design Changes:** Do not modify `workflow/design/*.md`.
- **Language/Standard:** C++17.
- **OpenCL:** 1.2 via `cl.hpp` — no raw `clCreate*` / `clRelease*`.
- **Timing:** Report ms to 3 decimal places (`std::fixed << std::setprecision(3)`).
- **Queue:** Must create a new `cl::CommandQueue` with `CL_QUEUE_PROFILING_ENABLE`
  (cannot use `ocl.queue` from `create_context()` — it has no profiling flag).
- **Upload:** Change buf_src to `CL_MEM_READ_ONLY` (no `CL_MEM_COPY_HOST_PTR`);
  upload via explicit `enqueueWriteBuffer` so the event can be captured.

## Implementation Steps

1. **Scaffold directory**
   ```
   01_Host_API/02_Visual_Kernel_Events/
   ├── CMakeLists.txt
   ├── src/main.cpp
   └── kernels/mad.cl
   ```

2. **CMakeLists.txt** — copy from Phase 1, change:
   - `project(VisualKernelEvents ...)`
   - `add_executable(visual_kernel_events src/main.cpp)`
   - Message string updated accordingly

3. **kernels/mad.cl** — copy verbatim from `01_Visual_Kernel/kernels/mad.cl`.

4. **src/main.cpp** — copy Phase 1, then apply these diffs:

   a. Add `#include <iomanip>` for `std::setprecision`.

   b. After `create_context()`, create profiling queue:
      ```cpp
      // CL_QUEUE_PROFILING_ENABLE: required to call getProfilingInfo() on events
      cl::CommandQueue queue(ocl.context, ocl.device, CL_QUEUE_PROFILING_ENABLE);
      ```

   c. Change buf_src creation to `CL_MEM_READ_ONLY` (remove `CL_MEM_COPY_HOST_PTR`):
      ```cpp
      cl::Buffer buf_src(ocl.context, CL_MEM_READ_ONLY,  total_bytes);
      cl::Buffer buf_dst(ocl.context, CL_MEM_WRITE_ONLY, total_bytes);
      ```

   d. Before dispatching, declare events and allocate dst:
      ```cpp
      std::vector<uint8_t> dst_data(total_bytes);
      cl::Event write_event, kernel_event, read_event;
      ```

   e. Replace the existing enqueue calls with event-capturing versions:
      ```cpp
      queue.enqueueWriteBuffer(buf_src, CL_FALSE, 0, total_bytes,
                               src_data.data(), nullptr, &write_event);

      queue.enqueueNDRangeKernel(kernel, cl::NullRange,
                                  cl::NDRange(work_size), cl::NullRange,
                                  nullptr, &kernel_event);

      queue.enqueueReadBuffer(buf_dst, CL_FALSE, 0, total_bytes,
                              dst_data.data(), nullptr, &read_event);

      queue.finish(); // flush + wait; profiling counters are now valid
      ```

   f. Extract and print timing after `queue.finish()`:
      ```cpp
      // WHY / 1e6: getProfilingInfo returns nanoseconds; divide to get ms.
      auto duration_ms = [](const cl::Event& e) -> double {
          return (e.getProfilingInfo<CL_PROFILING_COMMAND_END>() -
                  e.getProfilingInfo<CL_PROFILING_COMMAND_START>()) / 1e6;
      };

      const double t_upload   = duration_ms(write_event);
      const double t_kernel   = duration_ms(kernel_event);
      const double t_download = duration_ms(read_event);

      std::cout << std::fixed << std::setprecision(3)
                << "\n--- Profiling ---\n"
                << "Upload to GPU:       " << t_upload   << " ms\n"
                << "Kernel execution:    " << t_kernel   << " ms\n"
                << "Download from GPU:   " << t_download << " ms\n"
                << "Total pipeline:      " << (t_upload + t_kernel + t_download) << " ms\n";
      ```

   g. Remove the old `ocl.queue.enqueueReadBuffer(...)` (blocking read after finish) — it's now part of the event-tracked enqueue chain above.

   h. Keep header comment updated: "Phase 2: Visual Kernel Events"

5. **Build & verify** (see DoD).

## Definition of Done (DoD)
- [ ] **Build:** `cmake -B build && cmake --build build` — zero warnings.
- [ ] **Profiling output:** All 3 stages printed with ms values:
  ```
  Upload to GPU:       X.XXX ms
  Kernel execution:    X.XXX ms
  Download from GPU:   X.XXX ms
  Total pipeline:      X.XXX ms
  ```
- [ ] **Artifact:** `output.bmp` produced and visually correct.
- [ ] **Mini-challenge:** Test 256×256 vs 1920×1080 — note when GPU wins.

## Execution Report (Filled by Agent)
- **Status:** COMPLETED
- **Validation:**
  ```
  Platform: NVIDIA GeForce RTX 4060 Laptop GPU

  === 256×256 ===
  Upload to GPU:       0.031 ms
  Kernel execution:    0.004 ms
  Download from GPU:   0.030 ms
  Total pipeline:      0.065 ms

  === 1920×1080 ===
  Upload to GPU:       1.163 ms
  Kernel execution:    0.055 ms
  Download from GPU:   1.086 ms
  Total pipeline:      2.304 ms

  === 4096×4096 ===
  Upload to GPU:       7.936 ms
  Kernel execution:    0.762 ms
  Download from GPU:   7.819 ms
  Total pipeline:      16.517 ms

  Key insight: Transfer time (upload + download) dominates at every scale.
  Kernel execution is ~5–10% of total time — bandwidth-bound, not compute-bound.
  ```
- **Changed Files:**
  - `01_Host_API/02_Visual_Kernel_Events/CMakeLists.txt` (new)
  - `01_Host_API/02_Visual_Kernel_Events/src/main.cpp` (new)
  - `01_Host_API/02_Visual_Kernel_Events/kernels/mad.cl` (new)
