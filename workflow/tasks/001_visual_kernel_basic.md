# Task 001: Visual Kernel Basic

## Context
- **Design Feature:** `workflow/design/01-hostapi.md`
- **Milestone:** Phase 1: Visual Kernel — "Hello World" basic image filter (MAD operation).
- **Relevant Files:**
  - `01_VisualKernel/main.cpp`
  - `01_VisualKernel/kernels/mad.cl`
  - `01_VisualKernel/CMakeLists.txt`
  - `common/ocl_wrapper.hpp` (minimal wrapper)
  - `vendor/stb/stb_image.h`
  - `vendor/stb/stb_image_write.h`

## Objective
Implement the "Visual Hello World" application using a simple Multiply-Add (MAD) kernel. The application must load an image (or generate a synthetic one if no input is provided), apply contrast/brightness adjustment on the GPU, and save the result to `output.bmp`. This validates the toolchain, wrapper, and basic data transfer.

## Constraints & Rules
- **No Design Changes:** Do not modify `workflow/design/*.md`.
- **Language/Standard:** C++17, OpenCL 1.2+ (via cl.hpp).
- **Directory Structure:** `01_VisualKernel/` (as per Design Specs).
- **CLI Arguments:** Must support `--contrast <float>` and `--brightness <int>`.
- **Output:** Must verify by generating `output.bmp`.
- **Error Handling:** Throw `std::runtime_error` on OpenCL failure.

## Implementation Steps
1. **Project Setup:** Create CMake structure for `01_VisualKernel` linking OpenCL and `common/`.
2. **Wrapper Implementation:** Implement `common/ocl_wrapper.hpp` (Context, Device, Queue, Program build).
3. **Kernel:** Write `kernels/mad.cl`: `dst = src * contrast + brightness` (handle clamping).
4. **Host Logic:**
   - Parse CLI args (default: contrast=1.0, brightness=0).
   - Load image using `stb_image` (or create synthetic test pattern).
   - Allocate CL buffers.
   - Enqueue Write → Kernel → Read.
   - Save result using `stb_image_write`.
5. **Validation:** Ensure output image visually reflects the changes.

## Definition of Done (DoD)
- [ ] **Build:** `cmake -B build && cmake --build build` succeeds.
- [ ] **Test:** `./build/visual_kernel --contrast 1.5 --brightness 20` runs without errors.
- [ ] **Artifact:** `output.bmp` exists and is visibly brighter/higher contrast than input.
- [ ] **Platform:** Runs on at least one available OpenCL platform.

## Execution Report (Filled by Agent)
- **Status:** [PENDING]
- **Validation:**
- **Changed Files:**
