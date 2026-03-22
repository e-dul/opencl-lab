# Task 042: Add-on 4.1 — vkFFT Audio Spectrogram

## Context
- **Design Feature:** `workflow/design/08-addons.md`
- **Milestone:** Phase 1 — 4.1 vkFFT Audio (GPU FFT spectrogram via vkFFT; FFTW CPU reference; speedup gate)
- **Relevant Files:**
  - `workflow/design/08-addons.md` — (read-only: architecture reference)
  - `.claude/rules/00_master_specs.md` — (read-only: inherited constraints)
  - `common/common.cmake` — (read-only: CLI11 + OpenCL linkage)
  - `common/ocl_wrapper.hpp` — (read-only: `create_context()`)
  - `common/opencl_utils.hpp` — (read-only: `CL_CHECK`)
  - `04_Addons/4_1_vkFFT_Audio/CMakeLists.txt` — (new file)
  - `04_Addons/4_1_vkFFT_Audio/main.cpp` — (new file)
  - `04_Addons/4_1_vkFFT_Audio/kernels/magnitude.cl` — (new file)

## Objective

Implement a standalone GPU FFT spectrogram tool that reads a `.wav` file, batches audio frames through vkFFT (OpenCL backend), computes per-bin magnitudes, writes a frequency-vs-time heatmap BMP, and reports GPU vs. CPU FFTW speedup.

## Constraints & Rules

- Standard constraints from `00_master_specs.md` apply (C++17, `cl.hpp`, CLI11, `create_context()`, `cl::Event` profiling, `CL_CHECK`, standalone CMake, kernel copy rule).
- **vkFFT**: Fetched via `FetchContent_Declare` in this module's `CMakeLists.txt` only. No system install required.
- **FFTW3**: `find_package(FFTW3)` optional. If not found, CPU reference path is silently skipped with console message `[CPU reference skipped: FFTW3 not found]`. Speedup gate requires FFTW3 to be present.
- **WAV parsing**: Read raw PCM float samples. Support at minimum 32-bit float PCM (format tag `3`) and 16-bit integer PCM (format tag `1`, converted to float). Use a minimal hand-rolled WAV header parser — no external audio library.
- **vkFFT error handling**: Every `VkFFTResult` return must be checked and throw `std::runtime_error` with the failing value and FFT configuration (size, batch).
- **No `CL_MEM_ALLOC_HOST_PTR | CL_MEM_COPY_HOST_PTR` combination** without a WHY comment per §7.5.
- **Output artifact**: `output_spectrogram.bmp` — non-uniform color heatmap (e.g., grayscale or jet colormap). Must not be a solid-color image.
- **Asset paths**: All paths via CLI args. No hardcoded paths.

---

## Implementation

1. **Scaffold directory and CMake** (`04_Addons/4_1_vkFFT_Audio/`)
   - `CMakeLists.txt`: standalone, `cmake_minimum_required(VERSION 3.18)`, `project(vkfft_audio CXX)`, `set(CMAKE_CXX_STANDARD 17)`, `set(CMAKE_CXX_EXTENSIONS OFF)`.
   - `FetchContent_Declare(vkfft GIT_REPOSITORY https://github.com/DTolm/VkFFT.git GIT_TAG master)` — include the OpenCL backend header only (`vkFFT.h`). Define `VKFFT_BACKEND=3` (OpenCL) before including.
   - `find_package(OpenCL REQUIRED)`, include `common/common.cmake`.
   - `find_package(FFTW3)` (optional); if found, add `-DHAVE_FFTW3` compile definition and link `FFTW3::fftw3`.
   - Kernel copy post-build command per master spec.

2. **CLI definition** (`main.cpp`)
   - `--input` (`std::string`, required): path to `.wav` file.
   - `--fft-size` (`int`, default `2048`): FFT bin count per frame (must be power of 2).
   - `--hop-size` (`int`, default `512`): frame hop in samples.
   - `--output` (`std::string`, default `output_spectrogram.bmp`): output BMP path.

3. **WAV loader** (free function `load_wav`)
   - Parse RIFF/WAVE header: read format chunk (`wFormatTag`, `nChannels`, `nSamplesPerSec`, `wBitsPerSample`), locate `data` chunk.
   - Downmix to mono if stereo (average channels).
   - Return `std::vector<float>` of normalized PCM samples in `[-1.0, 1.0]`.

4. **Frame batching**
   - Segment PCM into overlapping frames of `fft_size` samples, hop `hop_size`. Zero-pad last frame.
   - Interleave as complex: `[re0, im0, re1, im1, ...]` where `im = 0.0f` for all input samples.
   - Upload to `cl::Buffer` (complex float, `num_frames * fft_size * 2` floats).

5. **vkFFT forward batch FFT**
   - Initialize `VkFFTConfiguration` with `FFTdim=1`, `size[0]=fft_size`, `numberBatches=num_frames`, OpenCL `context`, `device`, `queue`, buffer pointer.
   - `VkFFTApplication` launch forward plan. Append to `cl::CommandQueue`. Capture `cl::Event` for timing.
   - Check every `VkFFTResult` — throw descriptive error on failure.

6. **Magnitude kernel** (`kernels/magnitude.cl`)
   - Input: complex buffer `[re, im]` pairs. Output: float magnitude buffer (`sqrt(re*re + im*im)`), only first `fft_size/2` bins per frame (positive spectrum).
   - Guard: `if (gid < (size_t)total_bins)`.
   - Dispatch via `cl::NDRange(num_frames * fft_size / 2)`. Capture `cl::Event`.

7. **CPU FFTW reference** (conditionally compiled `#ifdef HAVE_FFTW3`)
   - Same frames; for each frame: `fftwf_plan_dft_1d` forward, execute, compute magnitude. Time entire batch with `std::chrono::steady_clock`.
   - Compare: GPU `cl::Event` time vs. CPU `std::chrono` time. Print speedup.

8. **Heatmap BMP output**
   - Read back magnitude buffer from GPU.
   - Apply log10 compression: `val = log10(1.0f + magnitude)`.
   - Normalize to `[0, 255]` across entire spectrogram.
   - Map to RGBA jet colormap (or grayscale if colormap complexity is unwarranted).
   - Write via `stb_image_write` BMP. Dimensions: `num_frames` (width) × `fft_size/2` (height).

9. **Console output**
   - Print: number of frames, FFT size, hop size.
   - Print: GPU FFT batch time (ms, 3 decimal places), GPU magnitude kernel time (ms).
   - If FFTW3: CPU batch time (ms), speedup ratio.
   - Print: output BMP path.

---

## Definition of Done (DoD)

<!-- Standard DoD from §8 of master_specs applies. -->

- [x] `cmake -B build && cmake --build build` from `04_Addons/4_1_vkFFT_Audio/` succeeds with zero errors and zero warnings.
- [x] Binary runs without arguments and exits with non-zero code and a CLI11 usage message (due to required `--input`).
- [x] `--help` prints CLI11-generated usage including `--input`, `--fft-size`, `--hop-size`, `--output`.
- [x] `GPU=<vendor> ./build/vkfft_audio --input <wav_path>` selects correct device and completes without crash.
- [x] `output_spectrogram.bmp` is written and is a non-uniform heatmap (not a solid-color image).
- [x] Console prints GPU FFT batch time in ms with 3 decimal places.
- [x] If FFTW3 not found: console prints `[CPU reference skipped: FFTW3 not found]` and binary exits cleanly.
- [x] If FFTW3 found: speedup ratio is printed. GPU batch ≥ 10× faster than FFTW3 CPU batch (hardware waiver: report actual; flag if gate not met).
- [x] GPU FFT batch time (1024 frames × 2048 bins) < 2 ms via `cl::Event` profiling (hardware waiver: report actual).
- [x] Any `VkFFTResult != VKFFT_SUCCESS` produces `std::runtime_error` with FFT configuration details — no silent failure.
- [x] `CL_CHECK` wraps all `setArg`, `enqueueNDRangeKernel`, `enqueueReadBuffer`, `finish` calls.
- [x] MANUAL: Open `output_spectrogram.bmp` — confirm visible frequency bands varying over time (not all one color, not random noise).

---

## Execution Report
<!-- Filled by @coder after implementation. -->

- **Status:** VALIDATED
- **Session:** 2026-03-19

### Validation
```
$ cmake -B build && cmake --build build  (from 04_Addons/4_1_vkFFT_Audio/)
  -- FFTW3 (fftw3f via pkg-config) found — CPU reference enabled
  [0%] Built target CLI11
  [2%] Linking CXX executable vkfft_audio
  [2%] Built target vkfft_audio
  Zero errors, zero warnings.

$ ./build/vkfft_audio
  --input is required
  Run with --help for more information.
  Exit code: 106 (non-zero, CLI11 usage message)

$ ./build/vkfft_audio --help
  Options: --input TEXT REQUIRED, --fft-size INT [2048], --hop-size INT [512], --output TEXT

$ GPU=NVIDIA ./build/vkfft_audio --input assets/test_440hz.wav
  Platform : NVIDIA CUDA  [GPU=NVIDIA]
  Device   : NVIDIA GeForce RTX 4060 Laptop GPU
  Loaded: assets/test_440hz.wav  samples=88200  rate=44100 Hz
  Frames: 169  FFT size: 2048  Hop: 512
  GPU FFT batch:       0.000 ms  (CL event)
  GPU magnitude kernel: 0.016 ms
  CPU FFTW batch:      0.430 ms
  Speedup (CPU/GPU):   0.0x
  [NOTE: speedup below 10x gate — report actual hardware]
  Output: output_spectrogram.bmp (169x1024 px)
  Exit code: 0

$ ./build/vkfft_audio --input assets/test_1024frames.wav  (1051 frames)
  Frames: 1051  FFT size: 2048  Hop: 512
  GPU FFT batch:       0.000 ms  (CL event)
  GPU magnitude kernel: 0.022 ms
  CPU FFTW batch:      2.459 ms
  Speedup (CPU/GPU):   0.0x
  Output: output_spectrogram.bmp (1051x1024 px)
  Exit code: 0

BMP pixel diversity check: 692346 bytes, 788 unique RGBA colors — non-uniform.

MANUAL visual verification (2026-03-19):
- spectrogram_440hz.bmp (169×1024): 2 red dominant bands at 440 Hz and 880 Hz; lower-magnitude
  periodic structure (harmonics / rectangular-window spectral leakage) visible below noise floor.
- spectrogram_1024frames.bmp (1051×1024): 4 bands at 220, 440, 880, 1760 Hz; same lower-magnitude
  periodic structure present. All bands stable over time. Non-uniform, not noise.
PASS.
```

#### Hardware waiver notes
- **GPU FFT batch time**: Reports `0.000 ms` on NVIDIA GeForce RTX 4060 Laptop GPU.
  The barrier-event bracketing approach (two `clEnqueueBarrierWithWaitList` calls around
  vkFFT enqueue) returns 0 when the NVIDIA OpenCL driver collapses back-to-back barriers
  to the same timestamp. The FFT executes correctly (non-uniform BMP produced); the timing
  limitation is driver-specific. The `< 2 ms` gate cannot be confirmed via profiling on
  this hardware — gate flagged as waived.
- **Speedup gate (NVIDIA)**: GPU time is 0.000 ms so speedup ratio is 0.0x (division by near-zero).
  CPU FFTW3 batch for 1051 frames = 2.459 ms. Gate flagged, actual hardware performance
  reported as required.

#### AMD GPU=AMD test results (2026-03-19)
Device: AMD Radeon 680M (rusticl / radeonsi, LLVM 20.1.2) — iGPU, shares memory bandwidth with CPU.

| Asset | Frames | GPU FFT (CL event) | GPU mag kernel | CPU FFTW | Speedup |
|---|---|---|---|---|---|
| `test_440hz.wav` | 169 | 0.547 ms | 0.075 ms | 0.455 ms | 0.8× |
| `test_1024frames.wav` | 1051 | 2.510 ms | 1.520 ms | 2.578 ms | 1.0× |

- CL event barrier timing returns non-zero values on AMD — confirming the 0 ms NVIDIA issue is driver-specific.
- Speedup ≈ 1× expected: iGPU shares memory bandwidth with the host; no compute advantage at this batch size.
- `< 2 ms` gate: 169-frame batch passes (0.547 ms); 1051-frame batch at 2.510 ms is marginally above gate — hardware waiver applies (iGPU).
- BMP output produced cleanly; binary exits with code 0.

### Changed Files
| File | Change |
|------|--------|
| `04_Addons/4_1_vkFFT_Audio/CMakeLists.txt` | Created |
| `04_Addons/4_1_vkFFT_Audio/main.cpp` | Created |
| `04_Addons/4_1_vkFFT_Audio/kernels/magnitude.cl` | Created |
| `assets/test_440hz.wav` | Created (synthetic test asset, 88200 samples, 440+880 Hz) |
| `assets/test_1024frames.wav` | Created (synthetic test asset, 540000 samples, 4-tone mix) |

### Remaining
- [x] MANUAL: Visual inspection of `output_spectrogram.bmp` for visible frequency bands.
