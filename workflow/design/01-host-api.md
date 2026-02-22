# Module 1: Host API & Feedback Loop

## Goal
Establish foundational skills for C++ OpenCL host-side programming with a profiling-first mindset. Students learn to wrap the OpenCL C API in modern C++, capture performance metrics from day one, and understand memory layout impact on CPU↔GPU data transfer.

## Non-goals
- Deep kernel optimization (covered in Module 2+)
- Multi-device orchestration or load balancing
- Production-grade error handling (simplified for learning clarity)
- Advanced memory patterns (sub-buffers, SVM, pipes)

## Roadmap / Status
- [x] Phase 1: Visual Kernel — "Hello World" basic image filter (MAD operation).
  - *Context*: See `HostAPI.md` section "01_VisualKernel — Visual 'Hello World'".
- [ ] Phase 2: Visual Kernel Events — Add event-based profiling to measure upload/kernel/download times.
  - *Context*: See `HostAPI.md` section "02_VisualKernel_Events — Measure Everything".
- [ ] Phase 3: Buffers Layout — Memory management experiments (CL_MEM_USE_HOST_PTR vs COPY).
  - *Context*: See `HostAPI.md` section "03_Buffers_Layout — Memory Matters".

## Specifications
> **Inherits**: `design/00_master_specs.md`

## Architecture (high-level)

### Components
- **HostAPI Wrapper**: Thin C++ layer over `cl::Context`, `cl::CommandQueue`, `cl::Kernel`.
  - Encapsulates platform/device selection.
  - RAII-based resource management (via `cl.hpp`).
- **Event Profiler**: Timing measurement subsystem.
  - Captures `CL_PROFILING_COMMAND_{QUEUED,SUBMIT,START,END}` timestamps.
  - Calculates upload, execution, and download latency.
- **BufferManager**: Memory allocation patterns.
  - Demonstrates `CL_MEM_USE_HOST_PTR` vs `CL_MEM_COPY_HOST_PTR`.

### Data Flow
1. **Host**: Allocates input data (image buffer).
2. **Transfer**: `clEnqueueWriteBuffer` → Device global memory (event tracking enabled).
3. **Execute**: Kernel execution (MAD op: `pixel * contrast + brightness`).
4. **Readback**: `clEnqueueReadBuffer` ← Device to host (event tracking enabled).
5. **Verify**: Profiler extracts timing; Host saves output buffer to BMP.

## Key Decisions (and Rationale)
1. **Visual Verification (Brightness/Contrast) over Synthetic Gradient**
   - **Why**: Processing real images (loading/saving BMP) is more engaging and practical than generating gradients.
   - **Refinement**: Phase 1 implements a Multiply-Add (MAD) kernel controlled by CLI args (`--contrast`, `--brightness`).
2. **C++ `cl.hpp` Wrapper over Raw C API**
   - **Why**: RAII prevents resource leaks; allows focus on logic rather than cleanup code.
3. **Events from Day 1**
   - **Why**: Prevent the "write code first, optimize later" anti-pattern. Timing must be a first-class citizen.

## Known Issues / Risks
- **Platform-specific event timing**: Intel CPU runtimes may report zero for QUEUED→SUBMIT deltas.
- **Driver quirks**: NVIDIA requires `CL_QUEUE_PROFILING_ENABLE` at queue creation.
- **Student profiling avoidance**: Tasks must strictly enforce timing output in DoD.

## Performance Gate (Module Completion)
- **Kernel Launch Overhead**: < 1ms (empty kernel baseline).
- **Profiling Coverage**: Events capture all 3 stages: Upload, Execute, Download.
- **Artifact**: Phase 1 must produce a valid `output.bmp` showing brightness adjustment.

## Specifications & Standards
*Essential implementation details that must persist across all tasks.*
- **Directory Structure**:
  - `01_VisualKernel/` (Phase 1)
  - `02_VisualKernel_Events/` (Phase 2)
  - `03_Buffers_Layout/` (Phase 3)
- **Verification Standard**: Must output visual artifact (`output.bmp`) and match CLI args (`--contrast 1.2`).
- **Tooling**:
  - Use `stb_image` / `stb_image_write` for BMP IO.
  - Use `cl.hpp` (C++ bindings) for OpenCL 1.2.
- **CLI Arguments**: Required support for `--contrast` and `--brightness`.

## Prerequisites
See [main README](../README.md) for base requirements (OpenCL 1.2+, CMake 3.16+).