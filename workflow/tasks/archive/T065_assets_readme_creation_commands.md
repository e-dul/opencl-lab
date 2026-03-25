# Task T065: Extend Assets README with Creation Commands

## Context
- **Design Feature:** `workflow/design/D10_v2_improvements.md`
- **Milestone:** Phase 8 — Extend assets README with creation commands
- **Relevant Files:**
  - `workflow/design/D10_v2_improvements.md` — (read-only: Phase 8 spec, §Phase 8 Assets Scope)
  - `assets/assets.md` — (to modify: add `## Regenerating Assets` section)
  - `02_Multimedia/08_FFmpeg_Pipeline/FFmpegPipeline.md` — (to modify: replace embedded `sample.mp4` creation command with link)

## Objective

Add a canonical `## Regenerating Assets` section to `assets/assets.md` documenting all synthetic asset creation commands, and replace the one embedded creation command in `FFmpegPipeline.md` with a link to that section.

## Constraints & Rules

- Do NOT document downloaded assets (`*.onnx`, `*.obj`) — link to their existing sources already in the catalogue table. They are explicitly out of scope.
- Do NOT modify any `.cpp`, `.cl`, or `CMakeLists.txt` file.
- The new section must be appended at the end of `assets/assets.md`, after the existing `## Format Notes` section.
- Per-module README edits are limited to replacing the embedded creation command with a prose link pointing to `assets/assets.md#regenerating-assets`. Do not rewrite surrounding context.
- `VisualKernelEvents.md` generates a local scratch file (`test_4k.bmp`), not a canonical asset — leave it unchanged.
- All `ffmpeg` commands must be copy-pasteable from the repository root (no assumed `cd`).

---

## Implementation

### A — Create helper scripts in `scripts/`

**Problem:** The Bayer asset requires pixel-level remapping logic that cannot be expressed with ffmpeg.

**Decision:** One script in `scripts/`, consistent with `scripts/gen_pgm.py`. WAV files are handled by ffmpeg (see step B).

**Action:** Create the following script:

**`scripts/gen_bayer.py`** — reads `assets/rgb_4k.bmp`, writes `assets/raw_bayer_4k.raw` (RGGB, 8-bit per pixel).

```python
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
```

---

### B — Add `## Regenerating Assets` section to `assets/assets.md`  *(depends on A)*

**Problem:** Creation commands for all synthetic assets are either absent from the catalogue entirely or embedded only in individual module READMEs. Students who delete or need to regenerate an asset have no single authoritative source.

**Decision:** Append a `## Regenerating Assets` section at the end of `assets/assets.md`. Each entry is a named sub-heading with the exact shell command(s) needed to recreate that file from scratch.

**Action:**
1. Read `assets/assets.md` in full to find the correct append point (after the last `## Format Notes` sub-section).
2. Append the following section. Use the commands exactly as specified below — do not paraphrase or shorten:

```markdown
## Regenerating Assets

All synthetic assets can be regenerated from the repository root using the commands below.
Downloaded assets (`*.onnx`, `*.obj`) are not listed here — see the table above for their source URLs.

### `sample.bmp` (256 × 256 RGB)
```bash
ffmpeg -y -f lavfi -i "testsrc=size=256x256:rate=1" -vframes 1 assets/sample.bmp
```

### `sample_1080p.bmp` (1920 × 1080 RGB)
```bash
ffmpeg -y -f lavfi -i "testsrc=size=1920x1080:rate=1" -vframes 1 assets/sample_1080p.bmp
```

### `rgb_4k.bmp` (3840 × 2160 RGB)
```bash
ffmpeg -y -f lavfi -i "testsrc2=size=3840x2160:rate=1" -vframes 1 assets/rgb_4k.bmp
```

### `sample_nv12.yuv` (256 × 256 NV12)
```bash
ffmpeg -y -i assets/sample.bmp -pix_fmt nv12 -f rawvideo assets/sample_nv12.yuv
```

### `sample_nv12_1080p.yuv` (1920 × 1080 NV12)
```bash
ffmpeg -y -i assets/sample_1080p.bmp -pix_fmt nv12 -f rawvideo assets/sample_nv12_1080p.yuv
```

### `sample_yuyv.yuv` (256 × 256 YUYV)
```bash
ffmpeg -y -i assets/sample.bmp -pix_fmt yuyv422 -f rawvideo assets/sample_yuyv.yuv
```

### `sample_yuyv_1080p.yuv` (1920 × 1080 YUYV)
```bash
ffmpeg -y -i assets/sample_1080p.bmp -pix_fmt yuyv422 -f rawvideo assets/sample_yuyv_1080p.yuv
```

### `sample.mp4` (1920 × 1080 H.264, 3 s)
```bash
ffmpeg -y -f lavfi -i "testsrc=duration=3:size=1920x1080:rate=25" \
  -c:v libx264 -preset fast -crf 23 assets/sample.mp4
```

### `raw_bayer_4k.raw` (3840 × 2160 RGGB Bayer)
```bash
# Requires rgb_4k.bmp to exist first (see above).
python3 scripts/gen_bayer.py
```

### `warehouse.pgm` and `warehouse_2k.pgm`
```bash
python3 scripts/gen_pgm.py
```

### `test_440hz.wav` (440 + 880 Hz sine mix, 2 s, float32 PCM)
```bash
ffmpeg -y -f lavfi -i "sine=frequency=440:duration=2" \
          -f lavfi -i "sine=frequency=880:duration=2" \
       -filter_complex "amix=inputs=2" \
       -ar 44100 -c:a pcm_f32le assets/test_440hz.wav
```

### `test_1024frames.wav` (4-tone mix, 12 s, float32 PCM)
```bash
# 12 s yields >=1024 FFT frames at default hop size
ffmpeg -y -f lavfi -i "sine=frequency=220:duration=12" \
          -f lavfi -i "sine=frequency=440:duration=12" \
          -f lavfi -i "sine=frequency=880:duration=12" \
          -f lavfi -i "sine=frequency=1760:duration=12" \
       -filter_complex "amix=inputs=4" \
       -ar 44100 -c:a pcm_f32le assets/test_1024frames.wav
```
```

3. Verify the appended section renders correctly (no unclosed fences, no broken headings).

---

### C — Replace embedded creation command in `FFmpegPipeline.md`

**Problem:** `02_Multimedia/08_FFmpeg_Pipeline/FFmpegPipeline.md` embeds the `sample.mp4` generation command inline. Now that `assets/assets.md` is the canonical source, the inline copy is redundant and will drift.

**Decision:** Replace the embedded ffmpeg block that creates `sample.mp4` with a single prose sentence linking to `assets/assets.md#regenerating-assets`.

**Action:**
1. Read `02_Multimedia/08_FFmpeg_Pipeline/FFmpegPipeline.md` in full to locate the exact block (around line 52 per earlier grep).
2. Identify the block: the `ffmpeg -f lavfi -i testsrc=duration=3...` command that creates `assets/sample.mp4`.
3. Replace that block with:

```markdown
> The `assets/sample.mp4` test clip is a synthetic H.264 file.
> To regenerate it, see [Regenerating Assets](../../assets/assets.md#regenerating-assets).
```

4. Do not alter any surrounding prose, headings, or the `--input assets/sample.mp4` usage example that follows.

---

## Definition of Done (DoD)

- [x] `scripts/gen_bayer.py` exists and is executable (`python3 scripts/gen_bayer.py` runs without error when `assets/rgb_4k.bmp` is present).
- [x] `assets/assets.md` contains a `## Regenerating Assets` section at the end of the file with named sub-headings for all twelve assets (`sample.bmp`, `sample_1080p.bmp`, `rgb_4k.bmp`, `sample_nv12.yuv`, `sample_nv12_1080p.yuv`, `sample_yuyv.yuv`, `sample_yuyv_1080p.yuv`, `sample.mp4`, `raw_bayer_4k.raw`, `warehouse.pgm`, `test_440hz.wav`, `test_1024frames.wav`).
- [x] Every sub-heading in `## Regenerating Assets` contains at least one copy-pasteable shell command.
- [x] The `raw_bayer_4k.raw` entry invokes `python3 scripts/gen_bayer.py` (no heredoc).
- [x] The WAV entries use `ffmpeg` with `sine` lavfi source and `pcm_f32le` codec (no Python script).
- [x] The `warehouse.pgm` entry invokes `python3 scripts/gen_pgm.py`.
- [x] `02_Multimedia/08_FFmpeg_Pipeline/FFmpegPipeline.md` no longer contains the inline `ffmpeg ... testsrc=duration=3 ... sample.mp4` creation block; a prose link to `assets/assets.md#regenerating-assets` replaces it.
- [x] `grep -n "testsrc=duration=3" 02_Multimedia/08_FFmpeg_Pipeline/FFmpegPipeline.md` returns no matches.
- [x] `grep -c "## Regenerating Assets" assets/assets.md` returns `1`.
- [x] No `.cpp`, `.cl`, or `CMakeLists.txt` files appear in `git diff --name-only`.
- [x] `VisualKernelEvents.md` is unchanged (confirmed by `git diff 01_Host_API/02_Visual_Kernel_Events/VisualKernelEvents.md` showing no diff).

---

## Execution Report
<!-- Filled by @coder after implementation. -->

- **Status:** DONE
- **Session:** 2026-03-25

### Completed
| Item | Action |
| --- | --- |
| A — Create `scripts/gen_bayer.py` | Created; executable bit set (-rwxrwxr-x) |
| B — Add `## Regenerating Assets` to `assets/assets.md` | Appended section with all 12 sub-headings |
| C — Replace FFmpegPipeline.md inline command | Replaced with prose link at line 52 |

### Validation
```
# 1. grep -n "testsrc=duration=3" 02_Multimedia/08_FFmpeg_Pipeline/FFmpegPipeline.md
(no output — PASS)

# 2. grep -c "## Regenerating Assets" assets/assets.md
1  — PASS

# 3. ls -la scripts/gen_bayer.py
-rwxrwxr-x 1 emil emil 922 mar 25 09:20 scripts/gen_bayer.py  — PASS

# 4. git diff --name-only | grep -E '\.(cpp|cl)$|CMakeLists\.txt'
(no output — PASS: only .md files changed)

# 5. git diff 01_Host_API/02_Visual_Kernel_Events/VisualKernelEvents.md
(no output — PASS: file unchanged)
```

### Changed Files
| File | Change |
| --- | --- |
| `scripts/gen_bayer.py` | Created: Bayer RGGB extractor from BMP |
| `assets/assets.md` | Appended `## Regenerating Assets` section |
| `02_Multimedia/08_FFmpeg_Pipeline/FFmpegPipeline.md` | Replaced inline ffmpeg command with prose link |

### Remaining
(none)
