# 4.6 — FFmpeg Pipeline: Offline Video Processing

**When to use**: you want to apply the blur/filter kernel from Track A to a video file — hardware decode directly into GPU memory, no CPU copies.

## Prerequisites
See [main README](../../README.md) for base requirements (OpenCL 1.2+, CMake 3.18+).

- **Required**: [Track A](../../02_Projects/A_Multimedia/Multimedia.md) completed
- FFmpeg with hardware acceleration: `sudo apt install ffmpeg libavcodec-dev libavformat-dev libavutil-dev`
- Verify hardware decode support: `ffmpeg -hwaccels` — must list `vaapi` (Intel/AMD) or `cuda` (Nvidia)

## Build & Run
```bash
cd 04_Addons/4_6_FFmpeg_Pipeline
cmake -B build && cmake --build build
./build/ffmpeg_opencl_transcoder --input ../../../assets/sample.mp4 \
    --output filtered.mp4 --effect bokeh
# GPU=NVIDIA ./build/ffmpeg_opencl_transcoder --input ../../../assets/sample.mp4 \
#     --output filtered.mp4 --effect bokeh
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

Add a second effect (`--effect sepia`) using the [GenericKernelTemplates](../../99_Toolbox/GenericKernelTemplates/README.md) pattern: both `bokeh` and `sepia` should share one `.cl` source file, built with `-D EFFECT=BOKEH` and `-D EFFECT=SEPIA` respectively. No duplicate kernel code.

## Troubleshooting

- **`clCreateFromVA_APIMediaSurfaceINTEL` not found**: requires `cl_intel_va_api_media_sharing` extension. Check: `clinfo | grep va_api`. Not available on Nvidia — use EGL interop (`cl_khr_egl_image`) instead.
- **Hardware decode fails, falls back to software**: verify `ffmpeg -hwaccels` shows your backend and that the codec is supported (H.264/H.265 are most widely accelerated).
- **Output video has green frame at start**: the first decoded frame may be a reference frame with no pixel data. Skip frames where `hw_frame->pict_type == AV_PICTURE_TYPE_NONE`.

---

[Back to Add-ons](../Addons.md)
