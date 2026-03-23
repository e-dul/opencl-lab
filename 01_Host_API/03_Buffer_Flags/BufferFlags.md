# 1.3 — Buffer Flags

**Goal**: Understand when data gets copied and how buffer flags affect performance — the choice between `CL_MEM_USE_HOST_PTR`, `CL_MEM_COPY_HOST_PTR`, and an explicit `enqueueWriteBuffer` determines whether you pay for a PCIe transfer at buffer creation or at dispatch time.

## Prerequisites (delta from module index)

- [1.2 — Visual Kernel Events](../02_Visual_Kernel_Events/VisualKernelEvents.md) completed.

## Build & Run

```bash
cd 03_Buffer_Flags
cmake -B build
cmake --build build
./build/buffers_layout_demo
# GPU=NVIDIA ./build/buffers_layout_demo
# GPU=AMD    ./build/buffers_layout_demo
# GPU=INTEL  ./build/buffers_layout_demo
```

No additional flags required. The demo runs all three strategies automatically and prints a comparison table.

## Verify

Program prints a comparative timing table for the three buffer strategies and exits without error:

```
Strategy              Upload (ms)   Kernel (ms)   Download (ms)   Total (ms)
--------------------  -----------   -----------   -------------   ----------
Explicit Write        ...           ...           ...             ...
Copy on Create        ...           ...           ...             ...
Use Host Ptr          ...           ...           ...             ...
```

Look for which strategy shows the lowest total time. The winner depends on your GPU architecture.

## Key Concepts

### Three Buffer Strategies

| Flag / Method | When copy happens | What it means |
| :------------ | :---------------- | :------------ |
| `enqueueWriteBuffer` (explicit) | At dispatch, as a trackable event | Upload is visible in profiling and easy to measure |
| `CL_MEM_COPY_HOST_PTR` | Inside `cl::Buffer()` constructor | Driver copies at creation time; cost is real but not captured by `cl::Event` |
| `CL_MEM_USE_HOST_PTR` | Driver decides | Hints the driver to use your host pointer directly — may be zero-copy or may DMA-copy implicitly |

### UMA vs Discrete GPU

The winning strategy depends on the physical memory topology:

- **Integrated GPU** (Intel HD, AMD APU): CPU and GPU share the same physical RAM. `CL_MEM_USE_HOST_PTR` can be a true zero-copy path — the kernel reads host memory directly. No PCIe transfer at all.

- **Discrete GPU** (NVIDIA, AMD dGPU): CPU RAM and VRAM are physically separate. `CL_MEM_USE_HOST_PTR` may trigger an implicit DMA copy. An explicit `enqueueWriteBuffer` with pinned memory (`CL_MEM_ALLOC_HOST_PTR`) typically gives the best throughput because the host allocation is DMA-able.

This is why the winner changes with `GPU=NVIDIA` vs `GPU=INTEL` — you are literally changing the memory topology.

For the hardware model behind these flags see [Toolbox: Zero-Copy](../../05_Toolbox/ZeroCopy/ZeroCopy.md).

## Mini-Challenge

Run the demo three times, pinning each vendor:

```bash
GPU=NVIDIA ./build/buffers_layout_demo
GPU=AMD    ./build/buffers_layout_demo
GPU=INTEL  ./build/buffers_layout_demo
```

Which strategy wins on each architecture? Write a one-sentence explanation of why, in terms of physical memory topology. Check your reasoning against the Zero-Copy toolbox entry.

---

[Module 1: Host API](../HostAPI.md)
