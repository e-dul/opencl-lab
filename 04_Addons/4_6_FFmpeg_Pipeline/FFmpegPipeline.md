# 4.6 — FFmpeg Pipeline: Offline Video Processing

**When to use**: you want to apply the blur/filter kernel from Track A to a video file — hardware decode directly into GPU memory, no CPU copies.

## Prerequisites
See [main README](../../README.md) for base requirements (OpenCL 1.2+, CMake 3.18+).

- **Required**: [Track A](../../02_Projects/A_Multimedia/Multimedia.md) completed

### System packages (Ubuntu 24.04)

**All platforms — FFmpeg dev libs (required):**
```bash
sudo apt install ffmpeg libavcodec-dev libavformat-dev libavutil-dev libswscale-dev
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
cd 04_Addons/4_6_FFmpeg_Pipeline
cmake -B build && cmake --build build
./build/ffmpeg_opencl_transcoder --input ../../../assets/sample.mp4 \
    --output filtered.mp4 --effect blur
# GPU=NVIDIA ./build/ffmpeg_opencl_transcoder --input ../../../assets/sample.mp4 \
#     --output filtered.mp4 --effect blur
```

## Verify
- `filtered.mp4` plays correctly with the effect applied
- Console prints per-frame breakdown:
  ```
  Decode (NVDEC/VAAPI): 1.2 ms
  Map decoder → OpenCL: 0.1 ms   (zero-copy)
  OpenCL filter:        3.4 ms
  Encode (NVENC/VAAPI): 1.8 ms
  Total:                6.5 ms   (153 FPS @ 1080p)
  ```

## Concept: Hardware Decode → OpenCL Zero-Copy

Software decode produces a `AVFrame` in system RAM — then you call `clEnqueueWriteBuffer` to upload it. That's the copy to eliminate.

Hardware decode outputs a frame already in GPU memory as an `AVHWFramesContext`. Map it directly to an OpenCL image:

```cpp
// Retrieve hardware frame context after avcodec_receive_frame()
AVFrame* hw_frame = av_frame_alloc();
avcodec_receive_frame(codec_ctx, hw_frame);   // hw_frame->data[3] = VAAPI surface

// Map VAAPI surface to OpenCL (Intel/AMD path)
cl_mem cl_img = clCreateFromVA_APIMediaSurfaceINTEL(
    context, CL_MEM_READ_ONLY, (VASurfaceID*)hw_frame->data[3], 0, &err);

// Acquire for OpenCL use
clEnqueueAcquireVA_APIMediaSurfacesINTEL(queue, 1, &cl_img, 0, NULL, NULL);

// Run your filter kernel — same kernel as A4_Smart_Webcam, unchanged
kernel.setArg(0, cl::Buffer(cl_img));
queue.enqueueNDRangeKernel(kernel, ...);

clEnqueueReleaseVA_APIMediaSurfacesINTEL(queue, 1, &cl_img, 0, NULL, NULL);
```

The filter kernel from `A4_Smart_Webcam` runs unchanged — only the buffer source differs.

## Mini-Challenge

Add a second effect (`--effect sepia`) using the [GenericKernelTemplates](../../99_Toolbox/GenericKernelTemplates/GenericKernelTemplates.md) pattern: both `blur` and `sepia` should share one `.cl` source file, built with `-D EFFECT_BLUR` and `-D EFFECT_SEPIA` respectively. No duplicate kernel code.

## Troubleshooting

- **`clCreateFromVA_APIMediaSurfaceINTEL` not found**: requires `cl_intel_va_api_media_sharing` extension. Check: `clinfo | grep va_api`. Not available on Nvidia — use EGL interop (`cl_khr_egl_image`) instead.
- **Hardware decode fails, falls back to software**: verify `ffmpeg -hwaccels` shows your backend and that the codec is supported (H.264/H.265 are most widely accelerated).
- **Output video has green frame at start**: the first decoded frame may be a reference frame with no pixel data. Skip frames where `hw_frame->pict_type == AV_PICTURE_TYPE_NONE`.

---

[Back to Add-ons](../Addons.md)
