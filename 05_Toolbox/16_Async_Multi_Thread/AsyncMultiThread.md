# Async Pipelines & Multi-Thread

**Symptom**: CPU thread blocks between kernel stages. Timeline shows gaps: kernel runs, then silence (CPU dispatching next kernel), then next kernel runs.

> **Hardware note**: true transfer/compute overlap is driver-dependent. Many consumer GPUs (NVIDIA CUDA, AMD rusticl) serialize commands on a single device even with OOO queues or dual queues. The code is correct — whether overlap is observable depends on your driver exposing separate DMA and compute engines.

## Prerequisites
Prerequisites: OpenCL 1.2+, CMake 3.18+, `clinfo` installed. See [main README](../../README.md) for base requirements.

## Contents
```
01_BasicSync/       Blocking calls baseline
02_AsyncSingle/     Dual in-order queues (DMA + compute), event-chained non-blocking enqueues
03_MultiThreadAsync/ Shared context, per-thread queues, event barriers
```

## Build & Run

Run all commands from `05_Toolbox/16_Async_Multi_Thread/`.

```bash
cd 05_Toolbox/16_Async_Multi_Thread
cmake -B build && cmake --build build
./build/async_multi_thread --mode basic_sync    # baseline
./build/async_multi_thread --mode async_single  # dual-queue pipeline
./build/async_multi_thread --mode multi_thread  # per-thread queues
./build/async_multi_thread --mode all           # all three + speedup ratios
```

`--iters N` (default: 8192) scales FMA compute load per work item. Increase it to make the kernel dominate; decrease it (`--iters 256`) to make transfer dominate.

## Verify

> **Note:** "No overlap detected" is the typical result on NVIDIA CUDA and AMD rusticl — this is expected behavior, not a code defect.

```
[basic_sync   ] Upload: X ms | Kernel: X ms | Download: X ms | Total: X ms
[async_single ] Upload: X ms | Kernel: X ms | Download: X ms | Total: X ms
[async_single ] Overlap confirmed: ... (or "No overlap detected" on many drivers)
[multi_thread ] Upload: X ms | Kernel: X ms | Download: X ms | Total: X ms
Speedup async_single vs basic_sync: X.XXx
Speedup multi_thread vs basic_sync:  X.XXx
```

Run all three modes and compare the timelines. Speedup is hardware-dependent — do not expect fixed numbers. Read the concept sections after you see your own output.

## Concept

**Step 1 — Baseline (blocking)**:
```cpp
queue.enqueueWriteBuffer(buf, CL_TRUE, ...);   // blocks
queue.enqueueNDRangeKernel(kernel, ...);        // blocks
queue.enqueueReadBuffer(buf, CL_TRUE, ...);     // blocks
```

**Step 2 — Dual-Queue Pipeline (overlap compute and transfer)**:

Two in-order queues — one for DMA, one for compute — let the driver schedule transfers and kernels on independent hardware engines. A single `CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE` queue is less reliable because most drivers still route all commands through one command processor.

```cpp
// WHY two queues: gives the driver a chance to assign DMA and compute to
// separate hardware engines. A single OOO queue serializes on most drivers.
cl::CommandQueue dma_queue    (ctx, device, CL_QUEUE_PROFILING_ENABLE);
cl::CommandQueue compute_queue(ctx, device, CL_QUEUE_PROFILING_ENABLE);

cl::Event upload_done, kernel_done;
dma_queue.enqueueWriteBuffer(buf_a, CL_FALSE, 0, size, data, nullptr, &upload_done);
// kernel starts as soon as upload_done fires — event crosses queue boundaries
compute_queue.enqueueNDRangeKernel(kernel, cl::NullRange, global, local,
                                    {upload_done}, &kernel_done);
dma_queue.enqueueReadBuffer(buf_b, CL_FALSE, 0, size, result, {kernel_done}, nullptr);
dma_queue.flush();
compute_queue.flush();  // submit both without waiting — CPU is free
// NOTE: flush() submits without blocking; finish() blocks until all commands complete.
```

**Step 3 — Multi-thread** (per-thread queues, shared context):
- `cl::Context` is thread-safe; `cl::CommandQueue` is not — one queue per thread.
- Synchronize between threads via `cl::Event` (`clWaitForEvents`), not mutexes.
- Use event callbacks (`clSetEventCallback`) to trigger downstream work without blocking a thread.

## Mini-Challenge

Modify `async_multi_thread` to pipeline three frames: while frame N is on the kernel, upload frame N+1 and download frame N-1 simultaneously. Measure the per-frame throughput improvement vs `basic_sync`.

## Troubleshooting

- **"No overlap detected" printed**: This is expected on most consumer GPUs (NVIDIA CUDA, AMD rusticl). The code is correct; the driver serializes DMA and compute on the same engine internally. True overlap requires PCIe copy-engine and compute-engine to be separately addressable, which OpenCL 1.2 does not mandate.
- **multi_thread is slower than basic_sync**: With a heavy kernel (`--iters 8192`), all threads queue behind each other on the single GPU — parallelism is not visible at the device level. Use `--iters 256` (transfer-dominated) to observe thread-level parallelism from concurrent downloads instead.
- **Out-of-order queue hangs on some devices**: Not all drivers implement `CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE` safely. Use two in-order queues with event dependencies as the more portable approach.
- **Per-thread queue crashes**: Sharing a `cl::CommandQueue` across threads is undefined behavior. Verify each thread constructs its own queue from the shared context.

## Used In
- [Track C — 03_Perception_Node](../../04_Robotics/03_Perception_Node/PerceptionNode.md) (double-buffer pipeline)

---

[Back to Toolbox](../Toolbox.md)
