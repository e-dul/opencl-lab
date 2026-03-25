# A.1 — OpenCV Interop: Kill the Copy

**Goal**: Measure how much time the naive `cv::Mat` copy costs, then replace it with `UMat` zero-copy interop.

## Prerequisites (delta from module index)

No additional requirements beyond the module index prerequisites.

## Build & Run

```bash
cd 01_OpenCV_Interop
cmake -B build
cmake --build build
./build/opencv_interop --input assets/sample.bmp
# GPU=NVIDIA ./build/opencv_interop --input assets/sample.bmp
```

## Verify

Console prints two transfer times. Expected output varies by hardware:

**Discrete GPU:**
```
[COPY]     cv::Mat → clEnqueueWriteBuffer:  8.4 ms
[ZERO-COPY] UMat → cl::Buffer (map):        0.1 ms
```

**iGPU (Intel/ARM Mali — UMA):**
```
[COPY]     cv::Mat → clEnqueueWriteBuffer:  0.2 ms
[ZERO-COPY] UMat → cl::Buffer (map):        0.1 ms
```

On iGPU, GPU buffer shares physical memory with system RAM — both paths converge to near zero. This is expected, not a bug. The technique matters on discrete GPU or large buffers.

## Key Concepts

- **Zero-copy interop**: `UMat` wraps the same GPU memory the OpenCL runtime allocated. No `memcpy` across the PCIe bus.
- **When it matters**: on discrete GPU, copy overhead is linear in buffer size. At 4K (≈24 MB) it easily exceeds 5 ms. On UMA (iGPU, ARM Mali), physical memory is shared — both paths converge.
- **Toolbox links**: [Toolbox: Zero-Copy](../../05_Toolbox/15_Zero_Copy/ZeroCopy.md) · [Toolbox: SVM](../../05_Toolbox/10_SVM/SVM.md) (hardware explanation of UMA zero-copy).

## Mini-Challenge

Change the input image to `4096×4096` and re-run. At what resolution does the copy time exceed 1 ms? 5 ms? This is your "copy budget" for later work.

---

[Path A: Multimedia & AI](../Multimedia.md)
