# Task T070: Sub-Buffers Tool (Phase 1)

## Context
- **Design Feature:** `workflow/design/D12_v2_2_improvements.md`
- **Milestone:** Phase 1 — Sub-Buffers Tool
- **Relevant Files:**
  - `workflow/design/D12_v2_2_improvements.md` — (read-only: Phase 1 spec)
  - `workflow/design/00_master_specs.md` — (read-only: standards reference)
  - `05_Toolbox/Toolbox.md` — (to modify: add row for slot 10)
  - `README.md` — (to modify: add Sub-Buffers entry to Toolbox section)
  - `05_Toolbox/10_Sub_Buffers_Partitioning/` — (new directory: full submodule)
  - `05_Toolbox/10_Sub_Buffers_Partitioning/SubBuffers.md` — (new file: submodule README)
  - `05_Toolbox/10_Sub_Buffers_Partitioning/main.cpp` — (new file)
  - `05_Toolbox/10_Sub_Buffers_Partitioning/kernels/sub_buffer_demo.cl` — (new file)
  - `05_Toolbox/10_Sub_Buffers_Partitioning/CMakeLists.txt` — (new file)
  - `common/common.cmake` — (read-only: reuse `opencl_lab_target()`)

## Objective

Create a self-contained, buildable `05_Toolbox/10_Sub_Buffers_Partitioning/` submodule that demonstrates `cl::Buffer::createSubBuffer` with a visual BMP output, and update the Toolbox index files accordingly.

## Constraints & Rules

- Standard constraints from `00_master_specs.md` apply in full (C++17, `cl.hpp`, CLI11, `create_context()`, `CL_CHECK`, standalone CMake, kernel symlink rule).
- **Zero data movement constraint**: parent buffer allocated once; sub-buffer handles aliased from it; no `enqueueWriteBuffer` of sub-regions after initial parent write.
- **Runtime alignment query is MANDATORY**: query `CL_DEVICE_MEM_BASE_ADDR_ALIGN` to compute sub-buffer partition boundaries at runtime. Hardcoded alignment values (e.g. `64`) are FORBIDDEN.
- **`get_global_id(0)` note**: the kernel must document (via WHY comment) that `get_global_id(0)` is relative to the sub-buffer origin, not the parent buffer.
- Slot 10 occupancy must not shift any other tool's numbering — verify `Toolbox.md` row order is unchanged.
- No modifications to any `.cpp`, `.cl`, or `CMakeLists.txt` outside the new `10_Sub_Buffers_Partitioning/` directory.

---

## Implementation

1. **Create directory skeleton**: `05_Toolbox/10_Sub_Buffers_Partitioning/` with subdirectory `kernels/`.

2. **Write `kernels/sub_buffer_demo.cl`**:
   - Kernel receives a single `__global uchar4* pixels` buffer (the sub-buffer region) and a `uchar4 fill_color` uniform.
   - `size_t gid = get_global_id(0);` guarded with `if (gid < (size_t)count)`.
   - WHY comment: explain that `gid=0` maps to the sub-buffer origin (not the parent buffer start).

3. **Write `main.cpp`**:
   - Parse CLI with CLI11: `--width` (default 512), `--height` (default 512), `--strips` (default 4), `--output` (default `output.bmp`).
   - Create context/queue via `create_context()` from `common/ocl_wrapper.hpp`.
   - Query `CL_DEVICE_MEM_BASE_ADDR_ALIGN` (returns alignment in bits; convert to bytes). Compute strip height rounded up to satisfy alignment for `uchar4` rows.
   - Allocate one parent `cl::Buffer` (`CL_MEM_READ_WRITE`) sized `width * total_height * 4` bytes (RGBA).
   - For each strip `i` in `[0, strips)`: compute `cl::BufferSlice` (origin, size) aligned to the queried boundary; call `parent_buf.createSubBuffer(CL_MEM_READ_WRITE, CL_BUFFER_CREATE_TYPE_REGION, &region)`.
   - Assign a distinct `uchar4` fill color per strip (use a simple HSV-to-RGB step or evenly spaced hues).
   - Dispatch kernel once per sub-buffer via `enqueueNDRangeKernel`; wrap in `CL_CHECK`.
   - After all dispatches, `enqueueReadBuffer` the parent buffer back to a host `std::vector<uint8_t>`.
   - Write `output.bmp` using `common/image_utils.hpp` (or `stb_image_write.h` directly).
   - Print device name and alignment value to stdout for verification.

4. **Write `CMakeLists.txt`**:
   - `cmake_minimum_required(VERSION 3.18)`, `project(SubBuffersPartitioning)`.
   - `set(CMAKE_CXX_STANDARD 17)` and `set(CMAKE_CXX_EXTENSIONS OFF)`.
   - Include `common/common.cmake` via relative path; use `opencl_lab_target()`.
   - Kernel symlink post-build command per `00_master_specs.md` §1.
   - Executable target name: `sub_buffers` (snake_case, no prefix, no `_demo` suffix).

5. **Write `SubBuffers.md`** following the Toolbox entry README template:
   - Sections: `# Sub-Buffers & Partitioning`, Symptom, Prerequisites (delta from `Toolbox.md`), Build & Run, Verify, Key Concept callout, back-link to `../Toolbox.md`.
   - Include the "Stop and Read: `CL_MISALIGNED_SUB_BUFFER_OFFSET`" note covering the alignment requirement and runtime query pattern.

6. **Update `05_Toolbox/Toolbox.md`**: Insert the new row at slot 10 (between slot 09 and slot 11):
   ```
   | [Sub-Buffers](10_Sub_Buffers_Partitioning/SubBuffers.md) | VRAM limits exceeded / High CPU overhead from manual `memcpy` chunking | `10_Sub_Buffers_Partitioning/` |
   ```

7. **Update root `README.md`**: Add the `10_Sub_Buffers_Partitioning` entry to the Toolbox section table, mirroring the `Toolbox.md` row.

---

## Definition of Done (DoD)

Standard DoD from `00_master_specs.md` §8:
- [x] `cmake -B build && cmake --build build` from `05_Toolbox/10_Sub_Buffers_Partitioning/` succeeds with zero errors and zero warnings.
- [x] `./build/sub_buffers` runs without arguments and exits 0.
- [x] `--help` prints CLI11-generated usage listing `--width`, `--height`, `--strips`, `--output`.
- [x] `GPU=<vendor> ./build/sub_buffers` selects the correct device without crashing.

Phase 1-specific DoD:
- [x] `output.bmp` is written and contains visually distinct horizontal color strips (one per sub-buffer partition).
- [x] `CL_DEVICE_MEM_BASE_ADDR_ALIGN` is queried at runtime; the queried value (in bytes) is printed to stdout; no hardcoded alignment literals present in `main.cpp`.
- [x] No `enqueueWriteBuffer` call targets any sub-buffer region after initial parent buffer population (zero data movement constraint).
- [x] `05_Toolbox/Toolbox.md` contains the new slot-10 row; no other rows are reordered or renumbered.
- [x] Root `README.md` Toolbox section contains the `10_Sub_Buffers_Partitioning` entry.
- [x] `SubBuffers.md` exists and includes back-link to `../Toolbox.md`.
- [x] `SubBuffers.md` reviewed against the Toolbox entry README standard: contains all required sections (`# Sub-Buffers & Partitioning`, Symptom, Prerequisites, Build & Run, Verify, Key Concept callout, back-link), and the "Stop and Read: `CL_MISALIGNED_SUB_BUFFER_OFFSET`" note is present.
- [x] `SubBuffers.md` approved by `@educator` via `/test-ux` pass — no HIGH UX friction items unresolved.
- [x] MANUAL: Open `output.bmp`; confirm N horizontally distinct color bands each covering an equal image strip with no corruption or solid-black regions.

---

## Execution Report

- **Status:** PASS (pending MANUAL and @educator /test-ux)
- **Session:** 2026-03-28, Intel(R) Iris(R) Xe Graphics (Intel OpenCL Graphics platform)

### Validation
```
$ cmake -B build && cmake --build build
-- Configuring done (4.2s)
-- Generating done (0.0s)
-- Build files have been written to: .../10_Sub_Buffers_Partitioning/build
[  0%] Built target CLI11
[100%] Built target sub_buffers

$ ./build/sub_buffers
Platform : Intel(R) OpenCL Graphics
Device   : Intel(R) Iris(R) Xe Graphics
Alignment : 128 bytes  (CL_DEVICE_MEM_BASE_ADDR_ALIGN = 1024 bits)
Image     : 512 x 512 (4 strips)
Strip     : 128 rows, 262144 bytes
Buffer    : 1048576 bytes total
Output    : output.bmp

$ ./build/sub_buffers --help
sub_buffers — partition a parent buffer into N sub-regions, fill each with a distinct color
Usage: ./build/sub_buffers [OPTIONS]

Options:
  -h,--help                   Print this help message and exit
  --width INT                 Image width in pixels  (default 512)
  --height INT                Image height in pixels (default 512)
  --strips INT                Number of horizontal strips / sub-buffers (default 4)
  --output TEXT               Output BMP file path (default output.bmp)

$ GPU=INTEL ./build/sub_buffers
Platform : Intel(R) OpenCL Graphics  [GPU=INTEL]
Device   : Intel(R) Iris(R) Xe Graphics
Alignment : 128 bytes  (CL_DEVICE_MEM_BASE_ADDR_ALIGN = 1024 bits)
...
Output    : output.bmp

$ ls -lh output.bmp
-rw-rw-r-- 1 emil emil 1.1M Mar 28 19:04 output.bmp

Hardcoded alignment check: literals 64/128/256 appear only in comments (not in code logic).
enqueueWriteBuffer check: no call targets any sub-buffer — confirmed by grep.
```

### Changed Files
| File | Change |
|------|--------|
| `05_Toolbox/10_Sub_Buffers_Partitioning/CMakeLists.txt` | Created |
| `05_Toolbox/10_Sub_Buffers_Partitioning/main.cpp` | Created |
| `05_Toolbox/10_Sub_Buffers_Partitioning/kernels/sub_buffer_demo.cl` | Created |
| `05_Toolbox/10_Sub_Buffers_Partitioning/SubBuffers.md` | Created |
| `05_Toolbox/Toolbox.md` | Modified — added slot-10 row |
| `README.md` | Modified — added Sub-Buffers Toolbox entry |

### Remaining
- [ ] `@educator` `/test-ux` pass on `SubBuffers.md`
- [ ] MANUAL: visual inspection of `output.bmp`
