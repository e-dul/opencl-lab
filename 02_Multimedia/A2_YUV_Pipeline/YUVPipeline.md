# A.2 — YUV Pipeline: One-Pass NV12 to RGBA on the GPU

**Goal**: Write a single-pass OpenCL kernel that converts a raw NV12 camera frame to RGBA — faster than the equivalent CPU `cv::cvtColor` path — and understand why raw layout knowledge makes this possible.

## Prerequisites (delta from module index)

No additional requirements beyond the module index prerequisites.

## Build & Run

```bash
cd A2_YUV_Pipeline
cmake -B build
cmake --build build
./build/yuv_pipeline_demo --input assets/sample_nv12.yuv --width 1920 --height 1080
```

## Verify

Two output files appear:
- `output_rgba.bmp` — correctly colored 1920×1080 image (green-pink = conversion bug)
  - Green-pink means the Y/UV byte offsets are wrong — check that the UV plane starts at byte `width * height`, not at `width`.
- `output_y_channel.bmp` — grayscale image (the Y luminance plane extracted without a copy)

Console prints a three-row comparison:
```
OpenCV CPU (cvtColor):   XX.X ms
OpenCL kernel:            X.X ms
Speedup:                  X.Xx
```

**Performance gate**: kernel time (via `cl::Event`) must be **< 2 ms** at 1920×1080.

## Key Concepts

### NV12 Layout and the Single-Pass Advantage

Real cameras output YUV — luminance (Y) separate from chrominance (U, V). Chroma subsampling (4:2:0) stores one U/V sample per 2×2 pixel block, cutting bandwidth roughly in half with near-zero perceptual loss.

**NV12 memory layout:**
```text
Y plane:   YYYYYYYY   ← full resolution, 1 byte/pixel
UV plane:  UVUVUVUV   ← half resolution, interleaved, 2 bytes per 2×2 block
```

Your kernel receives a flat byte buffer. Stride (pitch) can be wider than width — always use `pitch` for row offsets, never `width`.

**Why a single-pass kernel wins**: `cv::cvtColor` reads the Y plane, reads the UV plane, and writes the result with intermediate buffers. The OpenCL kernel reads the NV12 buffer once per pixel, computes conversion inline, and writes once. One pass, no intermediate copies, all pixels in parallel.

## Mini-Challenge: YUYV Port

Webcams often output YUYV instead of NV12. The format is packed — no separate UV plane. Write a `yuyv_to_rgba` kernel using the same BT.601 coefficients. Then split it into two passes and compare timing using `cl::Event`. See inline comments in `A2_YUV_Pipeline/` for index arithmetic.

---

[Path A: Multimedia & AI](../Multimedia.md)
