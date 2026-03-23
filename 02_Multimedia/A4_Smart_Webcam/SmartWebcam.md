# A.4 — Smart Webcam: Flagship Project

**Goal**: Build a live webcam pipeline running at ≥ 30 FPS @ 1080p: capture → AI segmentation → OpenCL Bokeh blur → display, with the neural network output feeding directly into the kernel.

## Prerequisites (delta from module index)

- [A.3.2 — OpenVINO GPU](../A3_2_OpenVINO_GPU/OpenVINOGPU.md) completed — A4 uses the RemoteTensor path internally.
- Webcam device (or use `--input --loop` for offline test).
- GLFW for live window (optional): `sudo apt install libglfw3-dev`.

## Build & Run

```bash
cd A4_Smart_Webcam
cmake -B build
cmake --build build
./build/smart_webcam --device 0
# Offline test (no webcam): ./build/smart_webcam --input assets/face.png --loop
# GPU=NVIDIA ./build/smart_webcam --device 0
```

A4 uses the RemoteTensor path from A3_2 internally. Frame 1 shows a JIT warm-up spike (~80–160 ms Inference) — expected. Frames 2+ stabilize to ~4–8 ms inference (f32, 256×256 model, Intel Xe).

## Verify

Live preview window shows:
- Subject in sharp focus, background blurred
- Console prints per-frame breakdown:
  ```
  Frame 2+ (stable):
    Capture:    2.1 ms
    Inference:  5.4 ms
    Kernel:     3.2 ms
    Display:    1.1 ms
    Total:     11.8 ms  ← must be < 33 ms to pass
  ```

When measuring FPS, skip frame 1 — its `Inference` time includes one-time GPU driver JIT compilation.

**Performance gate**: Total frame time < 33 ms @ 1080p (30 FPS).

## Key Concepts

### Zero-Copy Pipeline Architecture

Three decisions make this zero-copy:

1. **Shared OpenCL context.** OpenVINO is initialized with `ClContext(core, ctx.get())` — the same `cl_context` your preprocessing kernel uses. Without this, the GPU plugin creates its own internal context and there is no shared address space to import buffers across.

2. **Output tensor pre-allocated before `infer()`.** `req.set_output_tensor()` is called with a pre-allocated `cl::Buffer` wrapped as a `ClBufferTensor` *before* the first `req.infer()`. Without this, `get_output_tensor()` after inference returns a plain host `Tensor`.

3. **`queue.finish()` gates the frame loop.** The blur kernel operates on a `cl_mem` owned by the `InferRequest`. `CL_CHECK(queue.finish())` ensures the blur kernel completes before the loop restarts or the request is destroyed.

**Why `CL_MEM_READ_WRITE` for both buffers**: the OpenVINO GPU plugin rejects `CL_MEM_READ_ONLY`/`CL_MEM_WRITE_ONLY` on imported buffers — it performs in-place layout transformations and requires read-write access.

## Stretch Challenge

Replace the segmentation model with a face detector (YuNet) that outputs a bounding box, then launch the blur kernel only over that ROI using `global_work_offset` and `global_work_size` in `enqueueNDRangeKernel`. Profile Full-Frame vs ROI with `cl::Event`.

**Performance gate**: < 20 ms/frame @ 1080p (single face).

## Mini-Challenge

Inside the Bokeh kernel, `if (mask[id] == BACKGROUND)` causes thread divergence. Replace it with `select()` (branchless) and measure the kernel time difference. See [Toolbox: Thread Divergence](../../05_Toolbox/ThreadDivergence/ThreadDivergence.md).

## Troubleshooting

- **First frame ~100–160 ms inference time**: JIT warm-up — expected. Frame 2+ stabilizes. Do not measure FPS using frame 1.
- **INT8 ONNX slower than f32 on Intel Xe**: QDQ-format INT8 is not fused by the GPU plugin. Use the f32 ONNX model.
- **Pipeline hangs or output corrupted after frame N**: missing `queue.finish()` before `InferRequest` is destroyed or re-invoked. Add `CL_CHECK(queue.finish())` after `enqueueNDRangeKernel`.
- **Webcam gives wrong resolution**: add `--width 1920 --height 1080`.
- **Wrong GPU**: `GPU=NVIDIA ./build/smart_webcam`.

---

[Path A: Multimedia & AI](../Multimedia.md)
