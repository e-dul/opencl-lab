# Assets Catalogue

| File | Format | Resolution | Size | Description |
|------|--------|------------|------|-------------|
| `sample.bmp` | BMP (24-bit RGB) | 256 × 256 | 192 KB | Reference colour image. Used as input for modules that expect packed RGB (e.g. A1 OpenCV interop). |
| `sample_nv12.yuv` | Raw NV12 (YUV 4:2:0) | 256 × 256 | 96 KB | Luma-plane-first, interleaved UV semi-planar layout. Derived from `sample.bmp`. Used by A2 YUV pipeline and any module that exercises YUV↔RGB conversion. |
| `sample_yuyv.yuv` | Raw YUYV (YUV 4:2:2 packed) | 256 × 256 | 128 KB | Packed macropixel layout `[Y0, U, Y1, V]` per 2-pixel pair. Converted from `sample.bmp` via ffmpeg (`-pix_fmt yuyv422`). Used by A2b YUYV Extension. Pass `--width 256 --height 256` at runtime. |
| `sample_1080p.bmp` | BMP (24-bit RGB) | 1920 × 1080 | 6.0 MB | Full-HD synthetic test pattern (`testsrc` via ffmpeg). Source for 1080p YUV assets. |
| `sample_nv12_1080p.yuv` | Raw NV12 (YUV 4:2:0) | 1920 × 1080 | 3.0 MB | Derived from `sample_1080p.bmp` via ffmpeg (`-pix_fmt nv12`). Use with A2: `--width 1920 --height 1080`. |
| `sample_yuyv_1080p.yuv` | Raw YUYV (YUV 4:2:2 packed) | 1920 × 1080 | 4.0 MB | Derived from `sample_1080p.bmp` via ffmpeg (`-pix_fmt yuyv422`). Use with A2b: `--width 1920 --height 1080`. |
| `face.png` | PNG (24-bit RGB) | 498 × 498 | 325 KB | Sample portrait used as `--input` for A3_1 (OpenCV DNN) and A3_2 (TFLite GPU). |
| `selfie_segmentation.onnx` | ONNX model | — | 452 KB | Official ONNX export of the MediaPipe Selfie Segmentation model (Apache-2.0). Input: `1×3×256×256` float; output: `1×1×256×256` float alpha mask. Required by A3_1. [Source](https://huggingface.co/onnx-community/mediapipe_selfie_segmentation) |


## Format Notes

### NV12 Memory Layout
_Example: 256 × 256 image_
```
┌──────────────────────┐  offset 0
│   Y plane (W × H)   │  65 536 bytes  (1 byte/pixel)
├──────────────────────┤  offset W×H
│  UV plane (W × H/2) │  32 768 bytes  (2 bytes/pixel pair, interleaved U,V)
└──────────────────────┘  total = W × H × 3/2
```
Dimensions must be passed explicitly at runtime (e.g. `--width 256 --height 256`);
the file carries no embedded header.

### YUYV Memory Layout
_Example: 256 × 256 image_
```
┌──────────────────────────────────────────┐  offset 0
│  Macropixel 0: [Y0, U, Y1, V] (4 bytes) │  encodes pixels 0 and 1
│  Macropixel 1: [Y2, U, Y3, V] (4 bytes) │  encodes pixels 2 and 3
│  …                                       │
└──────────────────────────────────────────┘  total = W × H × 2
```
U and V are shared per horizontal pair (4:2:2 subsampling — full luma, halved chroma width).
Dimensions must be passed explicitly at runtime; the file carries no embedded header.
