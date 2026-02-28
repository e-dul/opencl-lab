# 4.4 — SVM and Zero-Copy: Deep Dive

**When to use**: the [Zero-Copy toolbox entry](../../99_Toolbox/ZeroCopy/README.md) wasn't enough — you want to understand *why* it works at the hardware level.

## Prerequisites
See [main README](../../README.md) for base requirements (OpenCL 1.2+, CMake 3.18+).

- [Toolbox: Zero-Copy](../../99_Toolbox/ZeroCopy/README.md) — read this first
- OpenCL 2.0+ device for the SVM sections (AMD APU, Intel integrated, ARM Mali)

## Build & Run
```bash
cd 04_Addons/4_4_SVM_Theory
cmake -B build && cmake --build build
./build/svm_deep_dive --size 64     # 64 MB buffer
# GPU=INTEL ./build/svm_deep_dive   # iGPU: near-zero transfer time
```

## Verify
```
Architecture detected: NUMA (discrete GPU, PCIe 4.0 x16)
PCIe bandwidth (measured):  24.1 GB/s upload, 23.8 GB/s download
[COPY_HOST_PTR ] 64 MB upload:  2.7 ms   (11.8 GB/s — pageable overhead)
[ALLOC_HOST_PTR] 64 MB upload:  1.4 ms   (22.9 GB/s — pinned, near peak)
[USE_HOST_PTR  ] 64 MB access:  4.1 ms   (iGPU: 0.0 ms — physically shared)
SVM coarse:                     0.3 ms   (map + sync, no copy)
SVM fine-grained:               N/A      (not supported on this device)
```

## Concept: Why UMA Changes Everything

**Discrete GPU (NUMA)**:
```
CPU RAM ──PCIe─── GPU VRAM
         ↑
    Copy required for every transfer.
    PCIe 4.0 x16 peak: 32 GB/s theoretical, ~24 GB/s measured.
    Pageable memory: OS may swap pages → extra CPU-side copy → ~half bandwidth.
    Pinned memory (ALLOC_HOST_PTR): bypasses paging → full PCIe bandwidth.
```

**Integrated GPU (UMA)**:
```
CPU RAM == GPU "VRAM"  (same physical DRAM)
         ↑
    No copy. GPU and CPU share the same DDR5/LPDDR5 bus.
    CL_MEM_USE_HOST_PTR: GPU accesses your pointer with zero latency.
    Bandwidth limited by system memory (~100 GB/s on modern APUs).
```

## SVM Levels

| SVM type | Sync required | Coherency | Typical hardware |
|:---------|:-------------|:----------|:-----------------|
| Coarse-grained | `clEnqueueSVMMap` / `Unmap` | Manual | Most OpenCL 2.0 devices |
| Fine-grained buffer | None | Cache-coherent | AMD APU, Intel Arc |
| Fine-grained system | None | Any `malloc` pointer visible to GPU | ARM Mali, Apple (via Metal) |

SVM fine-grained requires a unified cache hierarchy — the CPU and GPU L2/L3 caches must be coherent. Discrete Nvidia GPUs do not expose this via OpenCL.

## Mini-Challenge

Run `svm_deep_dive` on a laptop (iGPU) and a desktop (discrete GPU). Record the `USE_HOST_PTR` time on both. Explain in one paragraph why the iGPU result is ~0 ms, citing the physical memory layout shown above.

## Troubleshooting

- **SVM coarse returns `CL_INVALID_OPERATION`**: device reports OpenCL 2.0 but SVM support is incomplete. Check `clGetDeviceInfo(CL_DEVICE_SVM_CAPABILITIES)` — must be non-zero.
- **Measured PCIe bandwidth far below spec**: run the benchmark with a larger buffer (`--size 256`). Small transfers don't saturate the bus due to command overhead.

---

[Back to Add-ons](../Addons.md)
