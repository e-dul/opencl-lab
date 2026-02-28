# Async Pipelines & Multi-Thread

**Symptom**: CPU thread blocks between kernel stages. Timeline shows gaps: kernel runs, then silence (CPU dispatching next kernel), then next kernel runs.

## Prerequisites
See [main README](../../README.md) for base requirements (OpenCL 1.2+, CMake 3.18+).

## Contents
```
01_BasicSync/       Blocking calls baseline
02_AsyncSingle/     OOO queue, non-blocking enqueues + event dependencies
03_MultiThreadAsync/ Shared context, per-thread queues, event barriers
```

## Build & Run
```bash
cd 99_Toolbox/AsyncMultiThread
cmake -B build && cmake --build build
./build/async_demo --mode basic_sync    # baseline
./build/async_demo --mode async_single  # OOO queue
./build/async_demo --mode multi_thread  # per-thread queues
```

## Verify
```
[basic_sync   ] Pipeline time: 18.4 ms  (sequential: upload → kernel → download)
[async_single ] Pipeline time: 11.2 ms  (overlap upload N+1 with kernel N)
[multi_thread ] Pipeline time:  9.8 ms  (parallel upload threads)
```

Run all three modes in sequence. Compare the timelines before reading the concept sections.

## Concept

**Step 1 — Baseline (blocking)**:
```cpp
queue.enqueueWriteBuffer(buf, CL_TRUE, ...);   // blocks
queue.enqueueNDRangeKernel(kernel, ...);        // blocks
queue.enqueueReadBuffer(buf, CL_TRUE, ...);     // blocks
```

**Step 2 — Out-of-Order Queue (overlap compute and transfer)**:
```cpp
cl::CommandQueue queue(ctx, device, CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE
                                  | CL_QUEUE_PROFILING_ENABLE);
cl::Event upload_done, kernel_done;
queue.enqueueWriteBuffer(buf_a, CL_FALSE, 0, size, data, nullptr, &upload_done);
// kernel starts as soon as upload_done fires — no CPU round-trip
queue.enqueueNDRangeKernel(kernel, cl::NullRange, global, local,
                            {upload_done}, &kernel_done);
queue.enqueueReadBuffer(buf_b, CL_FALSE, 0, size, result, {kernel_done}, nullptr);
queue.flush();  // submit without waiting — CPU is free to prepare next frame
```

**Step 3 — Multi-thread** (per-thread queues, shared context):
- `cl::Context` is thread-safe; `cl::CommandQueue` is not — one queue per thread.
- Synchronize between threads via `cl::Event` (`clWaitForEvents`), not mutexes.
- Use event callbacks (`clSetEventCallback`) to trigger downstream work without blocking a thread.

## Mini-Challenge

Modify `async_demo` to pipeline three frames: while frame N is on the kernel, upload frame N+1 and download frame N-1 simultaneously. Measure the per-frame throughput improvement vs `basic_sync`.

## Troubleshooting

- **Out-of-order queue hangs on some devices**: Not all drivers implement `CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE` correctly. Fall back to in-order queue with explicit event dependencies if you see deadlocks.
- **Per-thread queue crashes**: Sharing a `cl::CommandQueue` across threads is undefined behavior. Verify each thread constructs its own queue from the shared context.

## Used In
- [Track C — C3_Perception_Node](../../02_Projects/C_Robotics_ROS2/RoboticsROS2.md#c3_perception_node--flagship-project) (double-buffer pipeline)

---

[Back to Toolbox](../Toolbox.md)
