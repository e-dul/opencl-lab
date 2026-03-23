# A.3.2 — OpenVINO GPU: Low-Level Inference (RemoteTensor API)

**Goal**: Run the same segmentation model via OpenVINO GPU plugin, passing a `cl::Buffer` directly as an input tensor — the inference engine reads from and writes to your OpenCL-managed memory with no host round-trip. Intel iGPU required.

## Prerequisites (delta from module index)

- **Intel iGPU required**: `clinfo | grep -i intel` must show a GPU device.
- OpenVINO runtime: `source /opt/intel/openvino/setupvars.sh` — run once per shell session.
- Install: `sudo apt install libopenvino-dev`
- Verify GPU device visible: `python3 -c "from openvino import Core; print(Core().available_devices)"` — expect `GPU` in the list.
- Assets: `assets/face.png`, `assets/selfie_segmentation.onnx` — included in the repository.

## Build & Run

```bash
source /opt/intel/openvino/setupvars.sh   # once per shell session
cd A3_2_OpenVINO_GPU
cmake -B build
cmake --build build
./build/openvino_gpu_demo --input assets/face.png --model assets/selfie_segmentation.onnx
```

## Verify

- `output_mask.bmp` — binary mask (white = person, black = background)
- `output_blurred.bmp` — background blurred, subject sharp
- Console prints:

```text
[A3_2 OpenVINO GPU]
Inference  (wall-clock, 5 runs):
  run  1:   110.413 ms  <- JIT warm-up
  run  2:     8.202 ms
  run  3:     4.443 ms
  ...
  avg*:       5.241 ms  (* runs 2+)
Blur kernel  (cl::Event):       2.067 ms
```

Run 1 includes GPU driver JIT compilation — expected. The stable latency is `avg*` (runs 2+). Use `--runs N` to control iteration count.

## Key Concepts

### OpenVINO RemoteTensor API

OpenVINO's GPU plugin runs inference internally on OpenCL. The RemoteTensor API exposes that internal `cl_mem` boundary: you can import your own `cl::Buffer` as an input tensor and export the output tensor's `cl_mem` handle directly into your next kernel call. Nothing leaves the GPU.

```cpp
auto remote_ctx = model.get_context().as<ov::intel_gpu::ocl::ClContext>();
auto input_tensor = remote_ctx.create_tensor(
    model.input().get_element_type(),
    model.input().get_shape(),
    input_cl_buffer.get()    // raw cl_mem handle — no copy
);
```

**A3_1 vs A3_2 comparison:**

| | A3_1 OpenCV DNN | A3_2 OpenVINO GPU |
|:--|:--|:--|
| **Integration effort** | Low (T-API handles it) | Medium (explicit RemoteTensor wiring) |
| **Control over buffers** | Low (OpenCV owns buffers) | High (you own the `cl_mem`) |
| **Target platforms** | Desktop / server with OpenCV | Intel iGPU, production pipelines |
| **OpenCV dependency** | Required | None |

**When to use**: Intel iGPU in production pipelines where OpenCV is not in the stack, or where you need explicit control over tensor buffer lifetime without the T-API abstraction overhead.

## Known Issues — Intel Xe iGPU

- **f32 ONNX is the fastest format on Intel Xe.** INT8 ONNX (QDQ format) ran ~2.5× *slower* than f32. The GPU plugin does not fuse `QuantizeLinear`/`DequantizeLinear` nodes — they execute as separate ops. Genuine INT8 speedup requires NNCF-aware quantization.
- **First inference call measures JIT warm-up.** ~80–160 ms on first call; subsequent calls stabilize at ~4–8 ms. Always run at least one untimed warm-up before recording latency.

## Mini-Challenge

Compare `DNN_TARGET_OPENCL` (A3_1) vs OpenVINO GPU plugin (A3_2) inference latency at 224×224 and 1080p input. Run each 100 times and report the median. Which wins at each resolution, and why?

---

[Path A: Multimedia & AI](../Multimedia.md)
