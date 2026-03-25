# Task 003: Phase 3 — Buffers Layout

## Context
- **Design Feature:** `workflow/design/01-host-api.md`
- **Milestone:** Phase 3 — Buffers Layout (Memory Matters)
- **Relevant Files:**
  - `01_Host_API/03_Buffers_Layout/` (skeleton, empty)
  - `01_Host_API/02_Visual_Kernel_Events/src/main.cpp` (reference implementation)
  - `01_Host_API/02_Visual_Kernel_Events/kernels/mad.cl` (reuse verbatim)
  - `01_Host_API/02_Visual_Kernel_Events/CMakeLists.txt` (base for new CMake)
  - `common/ocl_wrapper.hpp` (device selection)
  - `common/opencl_utils.hpp` (CL_CHECK, load_kernel_source)

## Objective
Build `buffers_layout_demo`: run the same MAD kernel with three buffer strategies and print a comparative profiling table so users can observe how transfer overhead differs per strategy.

## Constraints & Rules
- **No Design Changes:** Do not modify `workflow/design/*.md`.
- **Language/Standard:** C++17.
- **Dependencies:** `cl.hpp` (OpenCL 1.2), `stb_image` / `stb_image_write` via FetchContent, `common/` interface.
- **Error Handling:** `CL_CHECK(err)` for return codes; throw `std::runtime_error` on fatal errors.
- **Forbidden:** Raw `clCreateBuffer` / `clReleaseMemObject` — use `cl::Buffer` only.
- **Executable name:** `buffers_layout_demo` (per `HostAPI.md` build instructions).

## Implementation Steps

### 1. Scaffold directory
```
01_Host_API/03_Buffers_Layout/
├── CMakeLists.txt
├── src/
│   └── main.cpp
└── kernels/
    └── mad.cl
```

### 2. CMakeLists.txt
Copy `02_Visual_Kernel_Events/CMakeLists.txt`. Changes:
- `project(BuffersLayout)`
- Executable name: `buffers_layout_demo`
- Add install/copy for `kernels/` directory

### 3. kernels/mad.cl
Copy verbatim from `02_Visual_Kernel_Events/kernels/mad.cl`. No changes needed — use scalar `mad_kernel` only.

### 4. src/main.cpp — Architecture

```
main()
 ├── generate 256×256 RGB synthetic image (reuse Phase 2 pattern)
 ├── create profiling queue (CL_QUEUE_PROFILING_ENABLE always on)
 ├── build program from mad.cl
 ├── run_strategy("Explicit Write",   CL_MEM_READ_ONLY,    ...)
 ├── run_strategy("Copy on Create",   CL_MEM_COPY_HOST_PTR, ...)
 ├── run_strategy("Use Host Ptr",     CL_MEM_USE_HOST_PTR,  ...)
 ├── print comparison table
 └── save output.bmp (from last strategy run)
```

#### `run_strategy()` signature
```cpp
struct TimingResult {
    double upload_ms;
    double kernel_ms;
    double download_ms;
};

TimingResult run_strategy(
    const std::string& name,
    cl_mem_flags       src_flags,
    const cl::Context& ctx,
    const cl::CommandQueue& queue,
    const cl::Kernel&  kernel,
    const std::vector<uchar>& host_src,
    std::vector<uchar>& host_dst,
    int width, int height
);
```

#### Strategy-specific buffer creation inside `run_strategy()`
```cpp
// Strategy 1 — Explicit Write (CL_MEM_READ_ONLY, no host ptr at creation)
cl::Buffer buf_src(ctx, CL_MEM_READ_ONLY, size);
cl::Event ev_write;
queue.enqueueWriteBuffer(buf_src, CL_FALSE, 0, size, host_src.data(), nullptr, &ev_write);

// Strategy 2 — Copy on Create (transfer happens inside constructor)
cl::Buffer buf_src(ctx, CL_MEM_COPY_HOST_PTR | CL_MEM_READ_ONLY, size, host_src.data());
// ev_write: use a marker or set upload_ms = 0 with comment explaining transfer is hidden

// Strategy 3 — Use Host Ptr (zero-copy hint; driver may still copy on discrete GPU)
cl::Buffer buf_src(ctx, CL_MEM_USE_HOST_PTR | CL_MEM_READ_ONLY, size, host_src.data());
// ev_write: same as Strategy 2 — transfer time is inside constructor or not applicable
```

> **Note for @coder:** Strategies 2 & 3 do not expose a writable `cl_event` for the transfer (it occurs in the constructor). Report `upload_ms = 0.0` for these and add a comment: `// Transfer timing not available; cost is inside cl::Buffer() constructor`.

#### Kernel & download events (same for all strategies)
```cpp
cl::Event ev_kernel, ev_read;
queue.enqueueNDRangeKernel(kernel, cl::NullRange, cl::NDRange(n_pixels), cl::NullRange, nullptr, &ev_kernel);
queue.enqueueReadBuffer(buf_dst, CL_FALSE, 0, size, host_dst.data(), nullptr, &ev_read);
queue.finish();
```

#### Timing extraction
Reuse `duration_ms` lambda from Phase 2:
```cpp
auto duration_ms = [](const cl::Event& e) -> double {
    return (e.getProfilingInfo<CL_PROFILING_COMMAND_END>() -
            e.getProfilingInfo<CL_PROFILING_COMMAND_START>()) / 1e6;
};
```

### 5. Output table format
```
Buffer Strategy Comparison (256x256 RGB, contrast=1.2, brightness=10)
--------------------------------------------------------------------
Strategy         | Upload (ms) | Kernel (ms) | Download (ms) | Total (ms)
-----------------|-------------|-------------|---------------|----------
Explicit Write   |       X.XXX |       Y.YYY |         Z.ZZZ |     T.TTT
Copy on Create   |       0.000*|       Y.YYY |         Z.ZZZ |     T.TTT
Use Host Ptr     |       0.000*|       Y.YYY |         Z.ZZZ |     T.TTT

* Transfer timing hidden inside cl::Buffer() constructor.
```

### 6. Educational comments
Add `// WHY` comments at:
- `CL_MEM_COPY_HOST_PTR`: explain transfer is synchronous inside constructor; no upload event available
- `CL_MEM_USE_HOST_PTR`: explain zero-copy hint on integrated GPU; driver may still copy on discrete GPU
- Table: note that "Total" for strategies 2/3 excludes hidden transfer cost

## Definition of Done (DoD)
- [x] **Build:** `cmake -B build && cmake --build build` succeeds without warnings; executable at `build/buffers_layout_demo`.
- [x] **Run:** `./build/buffers_layout_demo` prints 3-row timing table to stdout.
- [x] **Visual:** `output.bmp` produced in working directory; visually matches MAD output from Phase 2.
- [x] **Profiling:** Kernel and download events captured for all 3 strategies (non-zero values expected).
- [x] **Annotations:** Key WHY comments present for each buffer strategy.

## Execution Report (Filled by Agent)
- **Status:** COMPLETED
- **Validation:**
  ```
  Buffer Strategy Comparison — 256×256 RGB  (contrast=1.2, brightness=10)
  Device: NVIDIA GeForce RTX 4060 Laptop GPU
  Strategy              Upload(ms)    Kernel(ms)  Download(ms)     Total(ms)
  Explicit Write           0.031           0.005         0.030         0.066
  Copy on Create           0.000 *         0.004         0.030       0.034 *
  Use Host Ptr             0.000 *         0.005         0.030       0.035 *
  ```
  output.bmp: 193K, visually correct.
- **Changed Files:**
  - `01_Host_API/03_Buffers_Layout/CMakeLists.txt` (created)
  - `01_Host_API/03_Buffers_Layout/src/main.cpp` (created)
  - `01_Host_API/03_Buffers_Layout/kernels/mad.cl` (copied from Phase 2)
