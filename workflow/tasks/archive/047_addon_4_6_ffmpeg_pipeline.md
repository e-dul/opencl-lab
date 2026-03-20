# Task 047: 4.6 FFmpeg Pipeline — Hardware Decode → OpenCL → Re-encode

## Context

- **Design Feature:** `workflow/design/08-addons.md` — Phase 6
- **Module README:** `04_Addons/4_6_FFmpeg_Pipeline/FFmpegPipeline.md`
- **Milestone:** Phase 6: 4.6 FFmpeg Pipeline
- **Relevant Files:**
  - `workflow/design/08-addons.md` — (read-only: authoritative specs)
  - `04_Addons/4_6_FFmpeg_Pipeline/FFmpegPipeline.md` — (read-only: user-facing README)
  - `common/ocl_wrapper.hpp` — (read-only: `create_context()`)
  - `common/common.cmake` — (read-only: CLI11 integration)
  - `04_Addons/4_6_FFmpeg_Pipeline/CMakeLists.txt`
  - `04_Addons/4_6_FFmpeg_Pipeline/main.cpp`
  - `04_Addons/4_6_FFmpeg_Pipeline/kernels/filter.cl`
  - `04_Addons/4_6_FFmpeg_Pipeline/kernels/nv12_to_rgba.cl`
  - `04_Addons/4_6_FFmpeg_Pipeline/kernels/rgba_to_nv12.cl`

## Objective

Implement a standalone binary `ffmpeg_opencl_transcoder` that hardware-decodes an `.mp4` via VAAPI/NVDEC, maps each frame into OpenCL (zero-copy when hardware interop is available, software fallback otherwise), applies a filter kernel, and re-encodes — printing per-frame stage timings.

## System Prerequisites (Ubuntu 24.04)

FFmpeg + VA-API dev libs (all platforms):

```bash
sudo apt install ffmpeg libavcodec-dev libavformat-dev libavutil-dev libswscale-dev libva-dev
```

| GPU    | HW Decode                                                | Zero-copy OpenCL interop                                                                     |
|--------|----------------------------------------------------------|----------------------------------------------------------------------------------------------|
| NVIDIA | `sudo apt install libva2 libva-drm2 nvidia-vaapi-driver` | Not available (no `cl_intel_va_api_media_sharing` on CUDA stack)                             |
| AMD    | `sudo apt install mesa-va-drivers vainfo`                | `sudo apt install rocm-opencl-runtime` (rusticl lacks VA interop)                            |
| Intel  | `sudo apt install intel-media-va-driver-non-free vainfo` | `sudo apt install intel-opencl-icd` — NEO exposes `cl_intel_va_api_media_sharing` natively   |

**Input asset format**: must be H.264 High Profile (yuv420p). NVDEC and VAAPI do not support High 4:4:4 Predictive. Generate via:

```bash
ffmpeg -f lavfi -i testsrc=duration=3:size=1920x1080:rate=25 \
  -vf format=yuv420p -c:v libx264 -profile:v high -level:v 4.0 assets/sample.mp4
```

## Constraints & Rules

- All standard constraints from `.claude/rules/00_master_specs.md` apply (C++17, `cl.hpp`, `CLI11`, `create_context()`, `cl::Event` profiling, `CL_CHECK`, standalone CMake, kernel copy rule).
- **Hardware interop**: check for `cl_intel_va_api_media_sharing` at runtime via extension string. Software fallback mandatory.
- **VAAPI extension functions** (`clCreateFromVA_APIMediaSurfaceINTEL`, acquire, release) are NOT in the ICD dispatch table. Load via `clGetExtensionFunctionAddressForPlatform`. Suppress extern declarations with `#define CL_NO_NON_ICD_DISPATCH_EXTENSION_PROTOTYPES` before including `<CL/cl_va_api_media_sharing_intel.h>`.
- **VAAPI decoder**: use standard `avcodec_find_decoder(codec_id)` + `hw_device_ctx` + `get_format` callback. **Do NOT** use `h264_vaapi` as a decoder name — it does not exist for decoding.
- **VAAPI encoder**: `h264_vaapi` with `hw_frames_ctx` (NV12-backed surface pool). Falls back to `libx264`.
- **CL context**: must be created with `CL_CONTEXT_VA_API_DISPLAY_INTEL` property at construction time — cannot be added post-creation.
- **`cl_dst` flags**: must be `CL_MEM_READ_WRITE` so the encode-side RGBA→NV12 kernel can read it.
- **Software fallback mandatory**: binary must complete without error on any hardware.
- **Per-frame timing columns**: decode, map, filter, encode, total. Use `cl::Event` for GPU stages. Use `std::chrono::steady_clock` for FFmpeg decode/encode stages.
- **Filter kernel** (`kernels/filter.cl`): compiled with `-D EFFECT_BLUR` or `-D EFFECT_SEPIA`. No duplicate kernel code.
- **CLI**: `--input` (required), `--output` (default: `filtered.mp4`), `--effect` (enum: `blur`, `sepia`; default: `blur`).
- **CMake** must use `find_package(PkgConfig REQUIRED)` → `pkg_check_modules(FFMPEG REQUIRED libavcodec libavformat libavutil libswscale)`. Also link `va`.
- **Performance gate**: decode + map + filter + encode < 10 ms per frame @ 1080p. Hardware waiver if HW decoder absent.

---

## Implementation

1. **CMakeLists.txt** — standalone build with `OpenCL::OpenCL`, `CLI11::CLI11`, `${FFMPEG_LIBRARIES}`, `va`. Kernel copy post-build command.

2. **kernels/filter.cl** — `apply_filter` with `#ifdef EFFECT_BLUR` (5×5 box blur) and `#ifdef EFFECT_SEPIA`.

3. **kernels/nv12_to_rgba.cl** — `nv12_to_rgba` kernel. BT.601 limited-range NV12 → RGBA. Y plane (CL_R, W×H) + UV plane (CL_RG, W/2×H/2) imported from VAAPI surface.

4. **kernels/rgba_to_nv12.cl** — two kernels:
   - `rgba_to_nv12_y`: dispatched W×H, writes Y plane.
   - `rgba_to_nv12_uv`: dispatched W/2×H/2, averages 2×2 block per UV pair, writes UV plane.

5. **main.cpp** — pipeline host:

   **5a. CLI setup** (CLI11): `--input` (required), `--output`, `--effect`.

   **5b. OpenCL context** via `create_context()`. Rebuild with `CL_CONTEXT_VA_API_DISPLAY_INTEL` when VAAPI device available and `cl_intel_va_api_media_sharing` extension present.

   **5c. FFmpeg open**:

   - VAAPI decode: `avcodec_find_decoder(codec_id)` + `dec_ctx->hw_device_ctx = vaapi_dev_ctx` + `get_format` callback selecting `AV_PIX_FMT_VAAPI`.
   - NVDEC: `avcodec_find_decoder_by_name("h264_cuvid")` + CUDA device context.
   - SW retry: if HW open fails or produces 0 frames, retry with `avcodec_find_decoder(codec_id)`.

   **5d. Per-frame loop** (zero-copy path when `hw_interop && enc_using_vaapi`):

   - Map: `clCreateFromVA_surf` Y+UV → acquire → `nv12_to_rgba` kernel → release → `cl_src`.
   - Filter: `apply_filter` kernel (`cl_src` → `cl_dst`). `cl::Event` timing.
   - Encode: `av_hwframe_get_buffer` → `clCreateFromVA_surf` (write-only, planes 0+1) → acquire → `rgba_to_nv12_y` + `rgba_to_nv12_uv` → release → `avcodec_send_frame(hw_frame)`.

   SW fallback: `av_hwframe_transfer_data` + `sws_scale` → `enqueueWriteImage` (map); `enqueueReadImage` + `sws_scale` + `av_hwframe_transfer_data` (encode).

   **5e. Teardown**: flush encoder, write trailer, free all AVFrame/AVPacket/SwsContext, `av_buffer_unref(&vaapi_dev_ctx)`.

---

## Definition of Done (DoD)

Standard items from `.claude/rules/00_master_specs.md §8` apply:

- [x] `cmake -B build && cmake --build build` from `04_Addons/4_6_FFmpeg_Pipeline/` succeeds with zero errors and zero warnings.
- [x] `./build/ffmpeg_opencl_transcoder --help` prints CLI11 usage including `--input`, `--output`, `--effect`.
- [x] `GPU=<vendor> ./build/ffmpeg_opencl_transcoder ...` selects the correct device without crashing.

Task-specific:

- [x] Run with `--input .../assets/sample.mp4 --effect blur` completes without error.
- [x] Console prints per-frame table with columns: decode, map, filter, encode, total.
- [x] Console prints average FPS at end.
- [x] When hardware interop is unavailable, software copy path message is printed and binary still completes successfully.
- [x] Performance gate met: ~4 ms/frame on Intel Iris Xe with full zero-copy pipeline (227 FPS, well under 10 ms).
- [x] MANUAL: Play `filtered.mp4`; confirm blur effect is visibly applied across frames.
- [x] MANUAL: Re-run with `--effect sepia`; confirm sepia tint is visibly applied.

---

## Execution Report

- **Status:** COMPLETE — performance gate met; awaiting MANUAL visual sign-off
- **Session:** 2026-03-20

### NVIDIA RTX 4060 (initial validation)

- **Hardware:** NVIDIA GeForce RTX 4060 Laptop GPU (OpenCL 3.0)
- **Note:** `h264_cuvid` opens but fails at decode time (CUDA_ERROR_NOT_SUPPORTED). Runtime SW-fallback kicks in. Hardware timing waiver applies.

```text
Platform : NVIDIA CUDA
Device   : NVIDIA GeForce RTX 4060 Laptop GPU
[INFO] Hardware interop unavailable; using software copy path.
[INFO] HW decoder produced no frames; retrying with SW decoder: h264

Frame  | Decode    | Map       | Filter    | Encode    | Total
-------|-----------|-----------|-----------|-----------|----------
0      |   3.22 ms  |  10.13 ms  |   0.19 ms  |   6.00 ms  |  19.55 ms
...
Average FPS: 49.6  (over 73 frames)
```

### Intel Iris Xe — SW path (intermediate)

- **Hardware:** Intel Iris Xe Graphics (OpenCL 3.0, `cl_intel_va_api_media_sharing` present)
- **Note:** Extension detected → interop available; VAAPI HW decoder not yet wired → SW `h264` fallback still active.

```text
Platform : Intel(R) OpenCL Graphics  [GPU=INTEL]
Device   : Intel(R) Iris(R) Xe Graphics
[INFO] HW decoder open failed; fell back to: h264
[INFO] Decoder: h264 (software)

Frame  | Decode    | Map       | Filter    | Encode    | Total
-------|-----------|-----------|-----------|-----------|----------
0      |   2.40 ms  |   6.27 ms  |   2.32 ms  |   7.43 ms  |  18.41 ms
...
Average FPS: 88.6  (over 73 frames)
```

### Intel Iris Xe — Full Zero-Copy Pipeline (final)

- **Hardware:** Intel Iris Xe Graphics
- **Decoder:** `h264` (software name) + `hw_device_ctx` + `get_format` → `AV_PIX_FMT_VAAPI`
- **Encoder:** `h264_vaapi` with `hw_frames_ctx`
- **Interop:** `cl_intel_va_api_media_sharing` — both decode and encode surfaces imported as CL images

```text
Platform : Intel(R) OpenCL Graphics  [GPU=INTEL]
Device   : Intel(R) Iris(R) Xe Graphics
[INFO] CL-VAAPI interop context: zero-copy enabled.
[INFO] Decoder: h264 (hardware)
[INFO] Encoder: h264_vaapi (hardware/vaapi)

Frame  | Decode    | Map       | Filter    | Encode    | Total
-------|-----------|-----------|-----------|-----------|----------
...
27     |   0.16 ms  |   1.36 ms  |   1.26 ms  |   0.99 ms  |   3.77 ms
...
Average FPS: 227.1  (over 73 frames)
```

### Performance Summary

| Stage  | SW path  | Zero-copy | Speedup              |
|--------|----------|-----------|----------------------|
| Decode | ~3 ms    | ~0.1 ms   | 30×                  |
| Map    | ~10 ms   | ~2 ms     | 5×                   |
| Filter | ~0.2 ms  | ~1.2 ms   | — (larger kernel on Xe) |
| Encode | ~6 ms    | ~0.9 ms   | 7×                   |
| **Total** | ~19 ms | **~4 ms** | **~5×**             |

### Files Changed

| File | Change |
|------|--------|
| `04_Addons/4_6_FFmpeg_Pipeline/CMakeLists.txt` | Created; added `va` to link libs |
| `04_Addons/4_6_FFmpeg_Pipeline/main.cpp` | Created; VAAPI decode/encode, interop context, zero-copy map+encode |
| `04_Addons/4_6_FFmpeg_Pipeline/kernels/filter.cl` | Created |
| `04_Addons/4_6_FFmpeg_Pipeline/kernels/nv12_to_rgba.cl` | Created; BT.601 NV12→RGBA |
| `04_Addons/4_6_FFmpeg_Pipeline/kernels/rgba_to_nv12.cl` | Created; BT.601 RGBA→NV12 (Y + UV kernels) |
| `assets/sample.mp4` | Created (3s 1080p H.264 test clip) |

### Key Implementation Notes

- VAAPI extension functions not in ICD dispatch table — loaded via `clGetExtensionFunctionAddressForPlatform`; `CL_NO_NON_ICD_DISPATCH_EXTENSION_PROTOTYPES` suppresses linker-breaking extern declarations.
- CL context rebuilt with `CL_CONTEXT_VA_API_DISPLAY_INTEL` after VAAPI device init (cannot add property post-creation).
- `cl_dst` changed to `CL_MEM_READ_WRITE` — write-only blocked the encode-side read in the RGBA→NV12 kernel.
- NVIDIA path unaffected: zero-copy branch gates on `hw_interop && enc_using_vaapi`.
- `prof_queue` created after interop context rebuild — `CL_QUEUE_PROFILING_ENABLE` must be set at queue creation time.

### Remaining (MANUAL)

- [x] Play `filtered.mp4` — confirm blur visible across frames with no colour corruption
- [x] Play `sepia.mp4` — confirm sepia tint visible across frames
