# A.2b — YUYV Extension: Packed 4:2:2 Format

**Goal**: Extend the NV12 pipeline to handle YUYV (4:2:2 packed) — the format most USB webcams output — using the same BT.601 coefficients with different index arithmetic.

## Prerequisites (delta from module index)

- [A.2 — YUV Pipeline](../02_YUV_Pipeline/YUVPipeline.md) completed.

## Build & Run

```bash
cd 03_YUYV_Extension
cmake -B build
cmake --build build
./build/yuyv_extension --input assets/sample_yuyv_1080p.yuv --width 1920 --height 1080
```

## Verify

- `output_yuyv_rgba.bmp` — correctly colored image from YUYV input
- Console prints single-pass vs two-pass timing comparison:
  ```
  Single-pass yuyv_to_rgba:    X.X ms
  Two-pass (Y extract + RGBA): X.X ms   ← expect ~2x
  ```

## Key Concepts

### YUYV Packed Format

Byte stream: `Y0 U0 Y1 V0 Y2 U1 Y3 V1 ...` — 4 bytes encode 2 pixels. Each pair of pixels shares one U and one V sample. Index arithmetic for pixel `x`:

- `Y = buf[x * 2]`
- `U = buf[(x & ~1) * 2 + 1]`  (even column's U, shared with odd neighbour)
- `V = buf[(x & ~1) * 2 + 3]`

**Two-pass vs single-pass**: the two-pass path reads the YUYV buffer twice and writes an intermediate Y buffer — doubling memory traffic. On hardware with large GPU L2 cache the gap may be smaller than 2x, but the extra write always costs something. Profile with `cl::Event` to measure your hardware.

## Mini-Challenge

Modify the single-pass kernel to output NV12 instead of RGBA — extracting the same Y, U, V values but writing a planar Y plane followed by an interleaved UV plane. Compare its throughput against the A.2 NV12 kernel. What does the difference reveal about the cost of the extra index arithmetic in the packed-format path?

---

[Path A: Multimedia & AI](../Multimedia.md)
