# A.5 — FFmpeg Pipeline: Hardware Decode to OpenCL

**When to use**: you want to apply the blur/filter kernel from Track A to a video file — hardware decode directly into GPU memory, no CPU copies.

## Prerequisites
See [main README](../../README.md) for base requirements (OpenCL 1.2+, CMake 3.18+).

- **Required**: [Path A: Multimedia & AI](../Multimedia.md) completed

### System packages (Ubuntu 24.04)

**All platforms — FFmpeg dev libs (required):**
```bash
sudo apt install ffmpeg libavcodec-dev libavformat-dev libavutil-dev libswscale-dev libva-dev
```

**NVIDIA (cuvid / NVDEC + optional VAAPI):**
```bash
sudo apt install libva2 libva-drm2 nvidia-vaapi-driver
# Verify NVDEC: ffmpeg -hwaccels | grep cuda
# Verify VAAPI: LIBVA_DRIVER_NAME=nvidia vainfo | grep H264
```

**AMD iGPU / dGPU (VAAPI decode, Ubuntu 24.04 Mesa stack):**
```bash
# Mesa VAAPI driver is included in Ubuntu 24.04 desktop installs.
# If missing:
sudo apt install mesa-va-drivers vainfo
# Verify: vainfo shows VAProfileH264High VAEntrypointVLD
```

**AMD OpenCL zero-copy interop (`cl_intel_va_api_media_sharing`):**
```bash
# rusticl (default Mesa OpenCL) does NOT support VA interop.
# ROCm OpenCL does — install via AMD's installer or:
sudo apt install rocm-opencl-runtime   # Ubuntu 24.04 repo if available
# Verify: clinfo | grep va_api_media_sharing
```

**Intel iGPU (simplest zero-copy path):**
```bash
sudo apt install intel-opencl-icd intel-media-va-driver-non-free vainfo
# Verify: clinfo | grep va_api_media_sharing   (must appear)
#         vainfo                                (must show H.264 decode)
```

**Input asset format requirement:**
The input `.mp4` must use H.264 High Profile (yuv420p) — not High 4:4:4 Predictive.
NVDEC and VAAPI do not support the 4:4:4 profile.
Generate a compatible test clip:
```bash
ffmpeg -f lavfi -i testsrc=duration=3:size=1920x1080:rate=25 \
  -vf format=yuv420p -c:v libx264 -profile:v high -level:v 4.0 \
  assets/sample.mp4
```

## Build & Run
```bash
cd 08_FFmpeg_Pipeline
cmake -B build && cmake --build build
./build/ffmpeg_pipeline --input assets/sample.mp4 \
    --output filtered.mp4 --effect blur
# GPU=NVIDIA ./build/ffmpeg_pipeline --input assets/sample.mp4 \
#     --output filtered.mp4 --effect blur
```

## Verify
- `filtered.mp4` plays correctly with the effect applied
- Console prints per-frame breakdown:
  ```
  Frame  | Decode    | Map       | Filter    | Encode    | Total
  -------|-----------|-----------|-----------|-----------|----------
  0      |   1.20 ms |   0.10 ms |   3.40 ms |   1.80 ms |   6.50 ms
  1      |   1.15 ms |   0.09 ms |   3.38 ms |   1.77 ms |   6.39 ms
  ...

  Average FPS: 153.1  (over 75 frames)
  Output: filtered.mp4
  ```

## Concept: Hardware Decode → OpenCL Zero-Copy

Software decode produces a `AVFrame` in system RAM — then you call `clEnqueueWriteBuffer` to upload it. That's the copy to eliminate.

Hardware decode outputs a frame already in GPU memory as an `AVHWFramesContext`. Map it directly to an OpenCL image.

The `clCreateFromVA_APIMediaSurfaceINTEL` function is **not** in the standard ICD dispatch table. It must be loaded at runtime via `clGetExtensionFunctionAddressForPlatform`:

```cpp
// WHY runtime load: these symbols are absent from libOpenCL.so.
// clGetExtensionFunctionAddressForPlatform resolves them through
// the platform-specific ICD at runtime.
auto clCreateFromVA_surf =
    (clCreateFromVA_APIMediaSurfaceINTEL_fn)
    clGetExtensionFunctionAddressForPlatform(
        platform, "clCreateFromVA_APIMediaSurfaceINTEL");

// Import the VAAPI surface as a CL image (zero-copy — same GPU memory)
cl_mem y_img = clCreateFromVA_surf(
    context, CL_MEM_READ_ONLY, &va_surf_id, 0 /* Y plane */, &err);

// Acquire for exclusive OpenCL access before dispatching the kernel
clEnqueueAcquireVA_APIMediaSurfacesINTEL(queue, 1, &y_img, 0, NULL, NULL);

// Run your filter kernel — same kernel as 06_Smart_Webcam, unchanged
kernel.setArg(0, cl::Image2D(y_img, true));  // NOTE: pseudocode — cl::Image2D does not have this constructor in cl.hpp 1.2
queue.enqueueNDRangeKernel(kernel, ...);

clEnqueueReleaseVA_APIMediaSurfacesINTEL(queue, 1, &y_img, 0, NULL, NULL);
```

The filter kernel from `06_Smart_Webcam` runs unchanged — only the buffer source differs.

## Mini-Challenge

Add a second effect (`--effect sepia`) using the [GenericKernelTemplates](../../05_Toolbox/06_Generic_Kernel_Templates/GenericKernelTemplates.md) pattern: both `blur` and `sepia` should share one `.cl` source file, built with `-D EFFECT_BLUR` and `-D EFFECT_SEPIA` respectively. No duplicate kernel code.

## Build & Run Notes

> **On NVIDIA:** the binary falls back to a GPU-assisted software path if the VA interop extension (`cl_intel_va_api_media_sharing`) is absent. NV12 planes are uploaded to OpenCL, colour-converted on the GPU, and encoded via libx264.

## Troubleshooting

- **`clCreateFromVA_APIMediaSurfaceINTEL` not found**: requires `cl_intel_va_api_media_sharing` extension. Check: `clinfo | grep va_api`. Not available on Nvidia — the pipeline falls back to the GPU-assisted software path: NV12 planes (~3 MB) uploaded to OpenCL, colour-converted on the GPU, and encoded via VAAPI or libx264.
- **Hardware decode fails, falls back to software**: verify `ffmpeg -hwaccels` shows your backend and that the codec is supported (H.264/H.265 are most widely accelerated).
- **Output video has green frame at start**: the first decoded frame may be a reference frame with no pixel data. Skip frames where `hw_frame->pict_type == AV_PICTURE_TYPE_NONE`.

---

[Path A: Multimedia & AI](../Multimedia.md)
