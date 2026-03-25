#!/usr/bin/env python3
"""Generate assets/raw_bayer_4k.raw from assets/rgb_4k.bmp (RGGB pattern, 8-bit)."""
import struct, pathlib

W, H = 3840, 2160
data = pathlib.Path("assets/rgb_4k.bmp").read_bytes()
offset = struct.unpack_from('<I', data, 10)[0]
pixels = data[offset:]  # BGR triplets, bottom row first
out = bytearray(W * H)
for row in range(H):
    src_row = H - 1 - row          # BMP stores rows bottom-up
    for col in range(W):
        idx = (src_row * W + col) * 3
        b, g, r = pixels[idx], pixels[idx + 1], pixels[idx + 2]
        # RGGB: R at even col+row, B at odd col+row, G elsewhere
        if row % 2 == 0 and col % 2 == 0:
            out[row * W + col] = r
        elif row % 2 == 1 and col % 2 == 1:
            out[row * W + col] = b
        else:
            out[row * W + col] = g
pathlib.Path("assets/raw_bayer_4k.raw").write_bytes(out)
print(f"raw_bayer_4k.raw written: {len(out)} bytes")
