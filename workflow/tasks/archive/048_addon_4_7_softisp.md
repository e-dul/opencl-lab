# Task 048: Add-on 4.7 SoftISP — Bilinear vs LDS-Tiled Debayer

## Context
- **Design Feature:** `workflow/design/08-addons.md`
- **Milestone:** Phase 7 — 4.7 SoftISP
- **Relevant Files:**
  - `workflow/design/08-addons.md` — (read-only: reference)
  - `.claude/rules/00_master_specs.md` — (read-only: constraints)
  - `common/common.cmake` — (read-only: CLI11 + OpenCL integration)
  - `common/ocl_wrapper.hpp` — (read-only: `create_context()`)
  - `common/image_utils.hpp` — (read-only: BMP I/O utilities)
  - `04_Addons/4_7_SoftISP/` — (new directory: all files)

## Objective

Implement a standalone `softisp` binary that reads a raw RGGB Bayer file, runs two debayer kernels (V1: naive bilinear; V2: LDS-tiled), writes pixel-identical BMP outputs for both, asserts byte-exact identity in-binary, and reports V1 ms, V2 ms, speedup, and effective FPS.

## Constraints & Rules

- All standard constraints from `.claude/rules/00_master_specs.md` apply (C++17, `cl.hpp`, CLI11, `create_context()`, `cl::Event` profiling, `CL_CHECK`, standalone CMake, kernel copy rule).
- **4.7-specific**:
  - V1 (`debayer_naive.cl`) and V2 (`debayer_lds.cl`) outputs must be byte-exact. Any difference terminates run with `std::runtime_error`.
  - Pixel-identical assertion is **in-binary** — no shell script comparison.
  - Performance gate: V2 < 10 ms at 3840×2160; speedup ≥ 3× reported in console.
  - Hardware waiver: gates verified via `cl::Event` only. Report actual hardware results; waive if physically impossible on device.
  - Input: raw RGGB Bayer (uchar, `--width × --height`). Default: `assets/raw_bayer_4k.raw` / 3840×2160.
  - Output: `output_rgb_v1.bmp`, `output_rgb_v2.bmp` (RGBA, 4 ch) in binary directory.
  - LDS tile halo: work-items at image edges must clamp indices to `[0, width-1] × [0, height-1]`.
  - CLI: `--input`, `--width`, `--height` (CLI11 via `common/common.cmake`).
  - No hardcoded asset paths. CMake emits `message(WARNING)` if `assets/raw_bayer_4k.raw` is absent (not a build error). Binary fails gracefully at runtime with clear `std::runtime_error`.

---

## Implementation

### A — Directory Scaffold

Create `04_Addons/4_7_SoftISP/` with:
- `CMakeLists.txt`
- `main.cpp`
- `kernels/debayer_naive.cl`
- `kernels/debayer_lds.cl`

### B — CMakeLists.txt

Mirror the pattern from `04_Addons/4_4_SVM_Theory/CMakeLists.txt` — `opencl_lab_target()` handles OpenCL + CLI11 + include dirs; `copy_kernels()` handles the POST_BUILD kernel copy.

```cmake
cmake_minimum_required(VERSION 3.18)
project(softisp CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../common/common.cmake)

add_executable(softisp main.cpp)
opencl_lab_target(softisp)
copy_kernels(softisp)

# Warn if asset missing — not a build error
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../assets/raw_bayer_4k.raw")
  message(WARNING "assets/raw_bayer_4k.raw not found. Binary will fail at runtime if --input is not overridden.")
endif()
```

### C — `debayer_naive.cl` (V1)

Naive bilinear debayer for RGGB Bayer pattern:
- Each work-item processes one output pixel at `(gx, gy)`.
- Pattern: `(gx%2, gy%2)` → R=`(0,0)`, Gr=`(1,0)`, Gb=`(0,1)`, B=`(1,1)`.
- Clamp all neighbor indices to `[0, width-1] × [0, height-1]`.
- Output: RGBA (`uchar4`), A=255.

```c
__kernel void debayer_naive(
    __global const uchar* bayer,
    __global uchar4*       rgba,
    int width, int height)
{
    size_t gx = get_global_id(0);
    size_t gy = get_global_id(1);
    if (gx >= (size_t)width || gy >= (size_t)height) return;

    // helper macro: clamp & read
    // ...bilinear interpolation per channel...
}
```

### D — `debayer_lds.cl` (V2)

LDS-tiled debayer — same algorithm, but load a `(TILE_W+2) × (TILE_H+2)` halo block into `__local` memory before interpolation:
- Tile size: `TILE_W=16`, `TILE_H=16` (compile-time defines, passed via `-DTILE_W=16 -DTILE_H=16`).
- Each work-item loads its pixel + participates in halo load via strided loop.
- `barrier(CLK_LOCAL_MEM_FENCE)` after load.
- Same RGGB bilinear interpolation, reading from `__local` array.
- Clamp halo boundary reads to `[0, width-1] × [0, height-1]`.

### E — `main.cpp`

Structure:
1. CLI11: `--input` (default `assets/raw_bayer_4k.raw`), `--width` (default 3840), `--height` (default 2160).
2. Read raw file → host `std::vector<uchar>` of size `width * height`.
3. `create_context()` → `cl::CommandQueue` with `CL_QUEUE_PROFILING_ENABLE`.
4. Build both kernels (same source file or separate `.cl` files); pass `-DTILE_W=16 -DTILE_H=16` for V2.
5. `cl::Buffer bayer_buf` (`CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR`).
6. `cl::Buffer rgba_v1`, `cl::Buffer rgba_v2` (`CL_MEM_WRITE_ONLY`), each `width * height * 4` bytes.
7. **V1 dispatch**: global `{width, height}`, local `{16, 16}` → `cl::Event ev1`. `queue.finish()`.
8. **V2 dispatch**: global `{ceil(width/16)*16, ceil(height/16)*16}`, local `{16, 16}` → `cl::Event ev2`. `queue.finish()`.
9. Read back both buffers.
10. **Byte-exact assertion**: compare `rgba_v1` and `rgba_v2` vectors element-by-element. On mismatch: throw `std::runtime_error("V1/V2 pixel mismatch at index N: v1=X v2=Y")`.
11. Write `output_rgb_v1.bmp` and `output_rgb_v2.bmp` via `common/image_utils.hpp` (or stb_image_write).
12. Compute and print timing table:
    ```
    V1 (naive bilinear):  XX.XXX ms  (YYY FPS)
    V2 (LDS tiled):       XX.XXX ms  (YYY FPS)
    Speedup:              X.XXx
    ```
13. Exit 0.

**Integer arithmetic safety** (§7.1): use `static_cast<size_t>(width) * height * 4` for buffer sizes.

**Error handling**: wrap all `setArg`, `enqueueNDRangeKernel`, `enqueueReadBuffer`, `finish` in `CL_CHECK`.

---

## Definition of Done (DoD)

Standard items from `.claude/rules/00_master_specs.md` §8:
- [x] `cmake -B build && cmake --build build` succeeds with zero errors and zero warnings from `04_Addons/4_7_SoftISP/`.
- [x] Binary runs without arguments (using defaults) and completes without error (asset present).
- [x] `--help` prints CLI11-generated usage including `--input`, `--width`, `--height`.
- [x] `GPU=<vendor> ./build/softisp` selects the correct device without crashing.

Task-specific:
- [x] `output_rgb_v1.bmp` and `output_rgb_v2.bmp` are written to the binary directory.
- [x] Console prints V1 ms, V2 ms, speedup (≥ 3× on dGPU — hardware waiver applies on iGPU/UMA).
- [x] V2 < 10 ms at 3840×2160 on discrete GPU (hardware waiver applies if physically impossible).
- [x] In-binary byte-exact assertion passes (no mismatch error) when V1 and V2 are correct.
- [x] Binary exits with `std::runtime_error` (non-zero) if `--input` file is missing.
- [x] MANUAL: Open `output_rgb_v1.bmp` and `output_rgb_v2.bmp`; verify the image is a visually correct colour-demosaiced photograph (no green/red cast, no grid artefacts, smooth gradients).

---

## Execution Report

- **Status:** DONE
- **Session:** 2026-03-21

### Validation
```
Platform : Intel(R) OpenCL Graphics  [GPU=INTEL]
Device   : Intel(R) Iris(R) Xe Graphics

V1 (naive bilinear):     8.606 ms  (116.2 FPS)
V2 (LDS tiled):          1.891 ms  (528.8 FPS)
Speedup:                  4.55x

Outputs written: output_rgb_v1.bmp, output_rgb_v2.bmp
```

### Visual Comparison vs `assets/rgb_4k.bmp`

Compared debayered output against the reference using pixel-level diff (amplified ×5):

- **95.6% of pixels are exact** (diff = 0). PSNR: **31.39 dB**.
- Errors are confined to two structures inherent to bilinear demosaicing:
  1. **Diagonal edges** — `testsrc2` contains angled lines; bilinear cannot distinguish axis-aligned neighbors from diagonal transitions, causing sub-pixel colour fringing.
  2. **Hard colour boundaries** — at saturated colour block edges (e.g. cyan/magenta), bilinear averaging pulls in the wrong channel from the opposite side ("zipper" artefact).
- No systematic channel bias (R mean: +0.007, G: +0.011, B: −0.028).
- These are **expected artefacts of the algorithm**, not implementation bugs. A direction-adaptive algorithm (Malvar-He-Cutler, AHD) would be needed to eliminate them.

### Changed Files
| File | Change |
|------|--------|
| `04_Addons/4_7_SoftISP/CMakeLists.txt` | Created |
| `04_Addons/4_7_SoftISP/main.cpp` | Created |
| `04_Addons/4_7_SoftISP/kernels/debayer_naive.cl` | Created |
| `04_Addons/4_7_SoftISP/kernels/debayer_lds.cl` | Created |
