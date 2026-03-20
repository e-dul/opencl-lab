# Task 047: 4.6 FFmpeg Pipeline — Hardware Decode → OpenCL → Re-encode

## Context
- **Design Feature:** `workflow/design/08-addons.md` — Phase 6
- **Module README:** `04_Addons/4_6_FFmpeg_Pipeline/FFmpegPipeline.md`
- **Milestone:** Phase 6: 4.6 FFmpeg Pipeline
- **Relevant Files:**
  - `workflow/design/08-addons.md` — (read-only: authoritative specs)
  - `04_Addons/4_6_FFmpeg_Pipeline/FFmpegPipeline.md` — (read-only: user-facing README)
  - `02_Projects/A_Multimedia/A4_Smart_Webcam/kernels/bokeh_blur.cl` — (read-only: reference for box-blur logic)
  - `common/ocl_wrapper.hpp` — (read-only: `create_context()`)
  - `common/common.cmake` — (read-only: CLI11 integration)
  - `04_Addons/4_6_FFmpeg_Pipeline/CMakeLists.txt` — (new file)
  - `04_Addons/4_6_FFmpeg_Pipeline/main.cpp` — (new file)
  - `04_Addons/4_6_FFmpeg_Pipeline/kernels/filter.cl` — (new file)

## Objective

Implement a standalone binary `ffmpeg_opencl_transcoder` that hardware-decodes an `.mp4` via VAAPI/NVDEC, maps each frame into OpenCL (zero-copy when hardware interop is available, software fallback otherwise), applies a filter kernel, and re-encodes — printing per-frame stage timings.

## Constraints & Rules

- All standard constraints from `.claude/rules/00_master_specs.md` apply (C++17, `cl.hpp`, `CLI11`, `create_context()`, `cl::Event` profiling, `CL_CHECK`, standalone CMake, kernel copy rule).
- **Hardware interop**: check for `cl_intel_va_api_media_sharing` (Intel/AMD VAAPI) and `cl_khr_egl_image` (Nvidia EGL) at runtime via extension string. If neither is present, fall back to software path: `avcodec_receive_frame` → `clEnqueueWriteBuffer`.
- **Software fallback mandatory**: binary must complete without error on any hardware. Print `[INFO] Hardware interop unavailable; using software copy path.` when fallback is active.
- **Graceful asset error**: if `--input` file does not exist, throw `std::runtime_error` with clear message. Do not crash or assert.
- **No raw `clCreateBuffer` / `clReleaseMemObject`**: use `cl::Buffer` / `cl::Image2D` RAII wrappers.
- **SVM / Device Enqueue**: not used here. Do not include.
- **`CL_DEVICE_OPENCL_C_VERSION` gating is forbidden**: not applicable to this task, but do not add any such check.
- **Per-frame timing columns**: decode, map, filter, encode, total. Use `cl::Event` for GPU stages. Use `std::chrono::steady_clock` for FFmpeg decode/encode stages.
- **Filter kernel** (`kernels/filter.cl`): single source file, compiled with `-D EFFECT_BLUR` or `-D EFFECT_SEPIA` at build time depending on `--effect` flag. No duplicate kernel code.
- **CLI**: `--input` (required), `--output` (default: `filtered.mp4`), `--effect` (enum: `blur`, `sepia`; default: `blur`).
- **`--help`** must print CLI11-generated usage.
- **CMake** must use `find_package(PkgConfig REQUIRED)` → `pkg_check_modules(FFMPEG REQUIRED libavcodec libavformat libavutil libswscale)`.
- **Missing asset**: CMake emits `message(WARNING "assets/sample.mp4 not found — provide via --input at runtime")` if `${CMAKE_SOURCE_DIR}/../../assets/sample.mp4` is absent. Build must still succeed.
- **Performance gate**: decode + map + filter + encode < 10 ms per frame @ 1080p on capable hardware. Hardware waiver applies (gate measured via `cl::Event` + `std::chrono`; waived if hardware decoder is absent and software path is active).

---

## Implementation

1. **CMakeLists.txt** — standalone build:
   - `cmake_minimum_required(VERSION 3.18)`, `project(ffmpeg_opencl_transcoder CXX)`.
   - `set(CMAKE_CXX_STANDARD 17)` + `set(CMAKE_CXX_EXTENSIONS OFF)`.
   - `include(../../common/common.cmake)`.
   - `find_package(OpenCL REQUIRED)` (also pulled in by `common.cmake` — idempotent).
   - `find_package(PkgConfig REQUIRED)` → `pkg_check_modules(FFMPEG REQUIRED libavcodec libavformat libavutil libswscale)`.
   - Link: `OpenCL::OpenCL`, `CLI11::CLI11`, `${FFMPEG_LIBRARIES}`.
   - Include dirs: `${FFMPEG_INCLUDE_DIRS}`.
   - Kernel copy `add_custom_command` (POST_BUILD, copy `kernels/` to `$<TARGET_FILE_DIR:ffmpeg_opencl_transcoder>/kernels/`).
   - Optional asset warning via `if(NOT EXISTS ...)`.

2. **kernels/filter.cl** — single-source effect kernel:
   - `__kernel void apply_filter(__read_only image2d_t src, __write_only image2d_t dst, int width, int height)`.
   - `#ifdef EFFECT_BLUR`: box-blur (3×3 or 5×5 kernel, reads via `read_imagef`).
   - `#ifdef EFFECT_SEPIA`: sepia matrix transform on RGBA pixel.
   - Default (neither defined): passthrough copy.
   - Use `size_t gid_x = get_global_id(0); size_t gid_y = get_global_id(1);` — not `int`.
   - Guard: `if (gid_x >= (size_t)width || gid_y >= (size_t)height) return;`.
   - WHY comment where `sampler_t` flags are combined.

3. **main.cpp** — pipeline host:

   **3a. CLI setup** (CLI11):
   ```
   --input   (required)
   --output  (default: "filtered.mp4")
   --effect  (enum string: "blur" | "sepia", default: "blur")
   ```

   **3b. OpenCL context** via `create_context()`. Build `filter.cl` with `-D EFFECT_BLUR` or `-D EFFECT_SEPIA` depending on `--effect`. Print selected device name.

   **3c. FFmpeg open**:
   - `avformat_open_input` → `avformat_find_stream_info` → find video stream index.
   - Attempt hardware decode: `avcodec_find_decoder_by_name("h264_vaapi")` (or `hevc_vaapi` / `h264_cuvid` / `hevc_cuvid`). On failure, fall back to software decoder (`avcodec_find_decoder`).
   - Allocate `AVCodecContext`, open codec.

   **3d. Per-frame loop**:
   - `av_read_frame` → `avcodec_send_packet` → `avcodec_receive_frame`.
   - Time decode via `std::chrono::steady_clock` (start before send, end after receive).
   - If hardware interop available: acquire surface → `cl::Event` map timing.
   - Else (software fallback): `sws_scale` to RGBA → `cl::Buffer` → `clEnqueueWriteBuffer`; time with `std::chrono`.
   - Dispatch kernel → `cl::Event` filter timing.
   - Release surface (hardware path) or read back (software path).
   - Re-encode via `avcodec_send_frame` / `avcodec_receive_packet` → write to output. Time encode via `std::chrono`.
   - Print one row per frame: `Frame N | decode X.Xms | map X.Xms | filter X.Xms | encode X.Xms | total X.Xms`.
   - After all frames: print average total FPS.

   **3e. Teardown**: flush encoder, write trailer, `avformat_close_input`, free AVFrame/AVPacket.

   **3f. Integer safety**: `static_cast<size_t>(width) * height` when computing buffer sizes.

4. **Verify sample.mp4 exists** at runtime (via `std::filesystem::exists`); throw `std::runtime_error` if not.

---

## Definition of Done (DoD)

Standard items from `.claude/rules/00_master_specs.md §8` apply:
- [ ] `cmake -B build && cmake --build build` from `04_Addons/4_6_FFmpeg_Pipeline/` succeeds with zero errors and zero warnings.
- [ ] `./build/ffmpeg_opencl_transcoder --help` prints CLI11 usage including `--input`, `--output`, `--effect`.
- [ ] `GPU=<vendor> ./build/ffmpeg_opencl_transcoder ...` selects the correct device without crashing.

Task-specific:
- [ ] `./build/ffmpeg_opencl_transcoder --input ../../../assets/sample.mp4 --output filtered.mp4 --effect blur` completes without error (or prints clear asset-missing error if asset absent).
- [ ] Console prints per-frame table with columns: decode, map, filter, encode, total.
- [ ] Console prints average FPS at end.
- [ ] When hardware interop is unavailable, `[INFO] Hardware interop unavailable; using software copy path.` is printed and binary still completes successfully.
- [ ] MANUAL: Play `filtered.mp4`; confirm blur effect is visibly applied across frames.
- [ ] MANUAL: Re-run with `--effect sepia`; confirm sepia tint is visibly applied.
- [ ] MANUAL: Confirm per-frame total < 10 ms on hardware with VAAPI/NVDEC decoder (or note hardware waiver in Execution Report if software fallback is active).

---

## Execution Report
<!-- Filled by @coder after implementation. -->

- **Status:** PENDING
- **Session:** —

### Validation
```
[output here]
```

### Changed Files
| File | Change |
|------|--------|
| `04_Addons/4_6_FFmpeg_Pipeline/CMakeLists.txt` | Created |
| `04_Addons/4_6_FFmpeg_Pipeline/main.cpp` | Created |
| `04_Addons/4_6_FFmpeg_Pipeline/kernels/filter.cl` | Created |

### Remaining
- [ ] Human validation of output video and timing gate
