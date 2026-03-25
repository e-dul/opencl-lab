# 1.2 — Visual Kernel Events

**Goal**: Add `cl::Event` profiling to the visual kernel and measure upload, kernel, and download times separately — build the habit of measuring before optimizing.

## Prerequisites (delta from module index)

- [1.1 — Visual Kernel](../01_Visual_Kernel/VisualKernel.md) completed.
- Key new concept: the queue must be created with `CL_QUEUE_PROFILING_ENABLE` — without it, `getProfilingInfo` returns zero timestamps.

## Build & Run

```bash
cd 02_Visual_Kernel_Events
cmake -B build
cmake --build build
./build/visual_kernel_events -p
# GPU=NVIDIA ./build/visual_kernel_events -p
```

The `-p` flag enables event profiling output. Without it the binary runs but prints no timings.

To test with a custom image:

```bash
./build/visual_kernel_events -p -i /path/to/image.bmp
```

## Verify

Console output (with `-p`) shows three non-zero timings:

```
Upload:   X.XXX ms
Kernel:   Y.YYY ms
Download: Z.ZZZ ms
```

Two files are written: `gradient_input.bmp` (before) and `output.bmp` (after). All three timing values must be non-zero to pass the Performance Gate.

## Key Concepts

### cl::Event Profiling

`cl::Event` is a handle that records timestamps for any enqueued command. To measure elapsed GPU time:

```cpp
cl::Event event;
queue.enqueueNDRangeKernel(kernel, cl::NullRange, global, local, nullptr, &event);
queue.finish();

cl_ulong start, end;
event.getProfilingInfo(CL_PROFILING_COMMAND_START, &start);
event.getProfilingInfo(CL_PROFILING_COMMAND_END,   &end);
double ms = (end - start) / 1e6;  // ns → ms
```

Profiling requires the queue to be created with `CL_QUEUE_PROFILING_ENABLE`.

### The GPU Timing Diagram

Understanding which phase each timing measures prevents misdiagnosis:

```
Host thread              Command Queue             GPU
    │                         │                     │
    ├─ enqueueWriteBuffer ────►│                     │
    ├─ enqueueNDRangeKernel ──►│── upload ──────────►│
    ├─ enqueueReadBuffer ─────►│── kernel ──────────►│
    ├─ queue.finish() ─────────│── download ─────────│
    │  (blocks here)           │                     │
    ◄─ returns ───────────────────────────────────────┘
```

What each timing reveals:

- **Upload** — PCIe bandwidth host → device (or host memory latency on iGPU)
- **Kernel** — compute cost on the GPU (this is what you optimize)
- **Download** — PCIe bandwidth device → host

If upload + download dominates kernel time, the workload is too small to benefit from GPU acceleration. This is the first insight the feedback loop gives you.

### Why "GPU Slower Than CPU"?

Common reasons — and none of them are failures:

- **Memory transfers dominate compute time** — the workload is too small to amortize PCIe overhead
- **Workload too small** — GPU parallelism only pays off above a threshold problem size
- **Poor memory access patterns** — scattered reads thrash the cache; coalesced access is required

Measurement tells you *when* GPU makes sense. This is the point of the feedback loop.

## Mini-Challenge

Generate test images at increasing sizes and find the crossover point where kernel time exceeds upload time:

```bash
# Requires: sudo apt install ffmpeg
ffmpeg -y -f lavfi -i "color=c=gray:s=4096x4096" -vframes 1 -f image2 -vcodec bmp test_4k.bmp
./build/visual_kernel_events -p -i test_4k.bmp
```

Run with 256×256, 1920×1080, and 4096×4096 inputs. At what image size does kernel time exceed upload time on your hardware? The answer differs between a discrete GPU and an integrated GPU — use `GPU=NVIDIA` / `GPU=AMD` / `GPU=INTEL` to pin the target.

---

[Module 1: Host API](../HostAPI.md)
