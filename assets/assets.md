# Assets Catalogue

| File | Format | Resolution | Size | Description |
|------|--------|------------|------|-------------|
| `sample.bmp` | BMP (24-bit RGB) | 256 × 256 | 192 KB | Reference colour image. Used as input for modules that expect packed RGB (e.g. A1 OpenCV interop). |
| `sample_nv12.yuv` | Raw NV12 (YUV 4:2:0) | 256 × 256 | 96 KB | Luma-plane-first, interleaved UV semi-planar layout. Derived from `sample.bmp`. Used by A2 YUV pipeline and any module that exercises YUV↔RGB conversion. |
| `sample_yuyv.yuv` | Raw YUYV (YUV 4:2:2 packed) | 256 × 256 | 128 KB | Packed macropixel layout `[Y0, U, Y1, V]` per 2-pixel pair. Converted from `sample.bmp` via ffmpeg (`-pix_fmt yuyv422`). Used by A2b YUYV Extension. Pass `--width 256 --height 256` at runtime. |

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
