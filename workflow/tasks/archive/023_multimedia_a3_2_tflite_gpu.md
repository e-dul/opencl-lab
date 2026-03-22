# Task 023: A3_2 — TFLite GPU Delegate + Bokeh Blur

## Context
- **Design Feature:** `workflow/design/04-multimedia-projects.md`
- **Milestone:** Phase 4 — A3_2 TFLite GPU Delegate: Explicit `clEnqueueMapBuffer` buffer handoff.
- **Relevant Files:**
  - `workflow/design/04-multimedia-projects.md` — (read-only: architecture, data flow A3_2, Key Decision §5 & §7, Known Risks)
  - `.claude/rules/00_master_specs.md` — (read-only: global constraints)
  - `common/ocl_wrapper.hpp` — (read-only: `create_context()`)
  - `common/image_utils.hpp` — (read-only: image IO utilities)
  - `common/common.cmake` — (read-only: CLI11 integration)
  - `02_Projects/A_Multimedia/A3_1_OpenCV_DNN/kernels/bokeh_blur.cl` — (read-only: reuse/copy kernel)
  - `assets/selfie_segmentation.tflite` — (read-only: inference model)
  - `02_Projects/A_Multimedia/A3_2_TFLite_GPU/` — (new directory: all files to create)

## Objective

Implement `A3_2_TFLite_GPU`: load an image into a `cl::Buffer` with `CL_MEM_ALLOC_HOST_PTR`, bind it to a TFLite GPU delegate input tensor via `clEnqueueMapBuffer`, run selfie segmentation inference on the GPU delegate, extract the output tensor as a `cl::Buffer` via a second `clEnqueueMapBuffer`, then apply the bokeh blur OpenCL kernel conditioned on that mask — producing `output_mask.bmp` and `output_blurred.bmp` with inference and blur times printed separately.

## Constraints & Rules

- **TFLite GPU delegate acquisition**: Use `FetchContent` to download the pre-built x86_64 TFLite GPU delegate `.so` from official TFLite release artifacts. `find_library` fallback is permitted but must be documented in a CMake comment. Hard-coded paths to system libraries are forbidden.
- **Buffer flag**: Input and output `cl::Buffer` objects must use `CL_MEM_ALLOC_HOST_PTR`. `CL_MEM_COPY_HOST_PTR` alone is not mappable on all drivers. Add a `WHY` comment explaining the flag combination per master_specs §7.5.
- **Delegate init**: Use `TfLiteGpuDelegateV2` with `TFLITE_GPU_EXPERIMENTAL_FLAGS_CL_COMMAND_QUEUE_IMPORT`. The existing OpenCL command queue (from `create_context()`) must be imported into the delegate to share the same context.
- **No host copy of mask data**: The output mask must flow from the TFLite output tensor binding directly to the blur kernel as a `cl::Buffer` — no `clEnqueueReadBuffer`/`enqueueWriteBuffer` on mask data.
- **Graceful fallback**: If `TfLiteGpuDelegateV2Create` returns null or delegate application fails, print a descriptive message and throw `std::runtime_error`. Do NOT crash silently.
- **Bokeh kernel naive branch**: Reuse or copy `bokeh_blur.cl` from A3_1. The `if (mask[id] == BACKGROUND)` branch must remain naive — do NOT pre-optimize with `select()`.
- **Profiling**: Inference time measured via `std::chrono::steady_clock` (TFLite does not expose `cl::Event`). Blur kernel timed via `cl::Event`. Both printed to console.
- **CLI args**: `--input` (path to BMP/PNG), `--model` (path to `.tflite`, default `assets/selfie_segmentation.tflite`), `--threshold` (float, default 0.5). `GPU` env var for device selection.
- **Integer overflow safety**: When computing buffer sizes from width × height, promote the first operand to `size_t` before multiplying (master_specs §7.1).
- **OpenCL dependency**: `find_package(OpenCL REQUIRED)` via `common/common.cmake`. No OpenCV dependency in A3_2.
- **`CMAKE_CXX_EXTENSIONS OFF`** must be set alongside `CMAKE_CXX_STANDARD 17`.

---

## Implementation

1. **CMake scaffold** — Create `02_Projects/A_Multimedia/A3_2_TFLite_GPU/CMakeLists.txt`.
   - Include `../../../common/common.cmake`.
   - Use `FetchContent` to acquire the TFLite GPU delegate `.so` and headers for x86_64 Linux from official TFLite release artifacts. Document the URL and version in a comment.
   - Link: `OpenCL::OpenCL`, `CLI11::CLI11`, the TFLite GPU delegate shared library.
   - Add `POST_BUILD` kernel copy command per master_specs §1.

2. **Bokeh blur kernel** — Copy `A3_1_OpenCV_DNN/kernels/bokeh_blur.cl` into `A3_2_TFLite_GPU/kernels/bokeh_blur.cl` unchanged.

3. **Host: context and image load** (`main.cpp`).
   - Parse CLI args via CLI11.
   - Call `create_context()` to obtain `cl::Context`, `cl::Device`, `cl::CommandQueue`.
   - Load input image (RGBA, 4 channels) using `common/image_utils.hpp`. Record width/height.
   - Allocate `cl::Buffer input_buf` (`CL_MEM_READ_ONLY | CL_MEM_ALLOC_HOST_PTR`, size = `static_cast<size_t>(width) * height * 4`). Write pixel data via `enqueueWriteBuffer`. `CL_CHECK` the call.

4. **TFLite delegate init and tensor binding**.
   - Create `TfLiteGpuDelegateOptionsV2` with `TFLITE_GPU_EXPERIMENTAL_FLAGS_CL_COMMAND_QUEUE_IMPORT` and import the `cl_command_queue` handle.
   - Create model from flat buffer (`tflite::FlatBufferModel::BuildFromFile`), build interpreter, apply GPU delegate.
   - Map input `cl::Buffer` to the input tensor via `clEnqueueMapBuffer` (blocking, `CL_MAP_WRITE`). Copy pixel data. `clEnqueueUnmapMemObject`. `CL_CHECK` both calls.
   - Bind the mapped pointer to the input tensor.

5. **Inference**.
   - Record `std::chrono::steady_clock::now()` before and after `interpreter->Invoke()`.
   - Print inference wall-clock time in ms.

6. **Output tensor extraction**.
   - Map output tensor buffer via `clEnqueueMapBuffer` (blocking, `CL_MAP_READ`).
   - Construct a `cl::Buffer output_mask_buf` wrapping the mapped memory (or use `CL_MEM_USE_HOST_PTR`).
   - Unmap after the blur kernel completes.

7. **Bokeh blur kernel dispatch**.
   - Build `bokeh_blur.cl`, set args: input RGBA buffer, mask buffer, output RGBA buffer, width, height, threshold. `CL_CHECK` every `setArg`.
   - `enqueueNDRangeKernel` with 1D global size = `static_cast<size_t>(width) * height`. `CL_CHECK`. Profile via `cl::Event`.
   - `CL_CHECK(queue.finish())`.
   - Read output buffer back. `CL_CHECK`. Save `output_blurred.bmp`.

8. **Mask visualization**.
   - Read mask buffer back to host (or use mapped pointer). Apply threshold, write grayscale pixels. Save `output_mask.bmp` using `stb_image_write`.

9. **Console output** — Print a two-row table:
   ```
   [A3_2 TFLite GPU Delegate]
   Inference (wall-clock): XX.XXX ms
   Blur kernel (cl::Event): XX.XXX ms
   ```

## Definition of Done (DoD)

- [ ] `cmake -B build && cmake --build build` from `02_Projects/A_Multimedia/A3_2_TFLite_GPU/` succeeds with zero errors and zero warnings.
- [ ] Binary runs without arguments and prints CLI11 usage (not a crash).
- [ ] `--help` prints usage including `--input`, `--model`, `--threshold`.
- [ ] `GPU=<vendor> ./build/a3_2_tflite_gpu --input assets/sample_1080p.bmp` completes without error.
- [ ] `output_mask.bmp` is produced: bright pixels on person silhouette, dark background (visually correct segmentation mask).
- [ ] `output_blurred.bmp` is produced: background blurred, foreground sharp (bokeh effect visible).
- [ ] Console prints inference wall-clock time and blur kernel `cl::Event` time in ms to 3 decimal places.
- [ ] No `clEnqueueReadBuffer`/`enqueueWriteBuffer` on mask data between inference and blur kernel (no host copy of mask).
- [ ] Integer overflow safety: buffer sizes computed as `static_cast<size_t>(width) * height * channels` — no `int` multiplication before cast.
- [ ] All `setArg`, `enqueueNDRangeKernel`, `enqueueReadBuffer`, `enqueueWriteBuffer`, `finish`, `enqueueUnmapMemObject` calls wrapped in `CL_CHECK`.

---

## Execution Report
<!-- Filled by @coder after implementation. -->

- **Status:** CANCELLED - not feasible
- **Session:** 2026-03-10

### Validation
```
[output here]
```

### Changed Files
| File | Change |
|------|--------|
| `02_Projects/A_Multimedia/A3_2_TFLite_GPU/CMakeLists.txt` | Created |
| `02_Projects/A_Multimedia/A3_2_TFLite_GPU/main.cpp` | Created |
| `02_Projects/A_Multimedia/A3_2_TFLite_GPU/kernels/bokeh_blur.cl` | Created (copied from A3_1) |

### Remaining
- [ ] [Remaining item]
