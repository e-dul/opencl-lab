# 4.1 — vkFFT: GPU FFT Without Writing a Kernel

**When to use**: you need a GPU FFT and don't want to spend two weeks implementing butterfly stages, twiddle factors, and radix-4 decomposition.

## Prerequisites
See [main README](../../README.md) for base requirements (OpenCL 1.2+, CMake 3.18+).

- Any Module 2 track.

## Build & Run
```bash
cd 06_Bonus/03_VkFFT_Audio
cmake -B build && cmake --build build
./build/vkfft_audio --input assets/test_440hz.wav
# GPU=NVIDIA ./build/vkfft_audio --input assets/test_440hz.wav
```

## Verify
- `output_spectrogram.bmp` — frequency-vs-time heatmap (low frequency = bottom, high = top)
- Console prints:
  ```
  FFT batch (1024 frames x 2048 bins): 0.8 ms
  CPU reference (FFTW):               18.4 ms   Speedup: 23x
  ```

## Concept

A naive DFT is O(N²). FFT is O(N log N) but the butterfly access pattern requires careful shared memory use, bit-reversal permutation, and multi-stage twiddle factor application. Implementing a performant FFT kernel from scratch is a multi-week project. vkFFT handles all of this and auto-tunes per device.

**The rule**: reach for a library when the algorithm is well-defined and implementation complexity exceeds two days. Write custom kernels when the access pattern or data layout is non-standard.

vkFFT uses OpenCL (and Vulkan/CUDA/HIP) under the hood. Integration is a few lines:

```cpp
VkFFTConfiguration cfg = {};
cfg.FFTdim = 1;
cfg.size[0] = 2048;          // FFT length
cfg.numberBatches = 1024;    // frames per batch
cfg.buffer = &cl_buffer;
cfg.bufferSize = &buf_size;

VkFFTApplication app = {};
initializeVkFFT(&app, cfg);
VkFFTLaunchParams params = { .commandQueue = &queue };
VkFFTAppend(&app, -1, &params);  // -1 = forward FFT
```

## Mini-Challenge

Process a 10-second `.wav` file in real time using a sliding window. At what window overlap (0%, 50%, 75%) does the GPU start keeping up with the audio stream? Profile the FFT time vs the audio callback period.

## Troubleshooting

- **vkFFT not found**: fetched via CMake FetchContent — requires internet at configure time. Offline: set `-DCMAKE_PREFIX_PATH=/path/to/vkfft`.
- **Wrong spectrogram orientation**: vkFFT output is interleaved complex (real, imag, real, imag...). Compute magnitude `sqrt(re² + im²)` before writing to BMP.

---

[Back to Bonus.md](../Bonus.md)
