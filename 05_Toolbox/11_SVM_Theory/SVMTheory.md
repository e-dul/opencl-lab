# 4.4 — SVM and Zero-Copy: Deep Dive

**When to use**: the [Zero-Copy toolbox entry](../../05_Toolbox/15_Zero_Copy/ZeroCopy.md) wasn't enough — you want to understand *why* it works at the hardware level.

> **Requires:** OpenCL 2.0+ device (AMD APU, Intel iGPU, ARM Mali). Falls back gracefully with an informational message on OpenCL 1.2 devices. Not supported on NVIDIA OpenCL.

## Prerequisites
See [main README](../../README.md) for base requirements (OpenCL 1.2+, CMake 3.18+).

- [Toolbox: Zero-Copy](../../05_Toolbox/15_Zero_Copy/ZeroCopy.md) — read this first
- OpenCL 2.0+ device for the SVM sections (AMD APU, Intel integrated, ARM Mali)

## Build & Run
```bash
cd 05_Toolbox/11_SVM_Theory
cmake -B build && cmake --build build
./build/svm_theory --width 8192 --height 8192     # ~256 MB buffer (8192×8192 floats)
# GPU=INTEL ./build/svm_theory --width 8192 --height 8192   # iGPU: near-zero transfer time
```

## Verify
```
Architecture detected: NUMA (discrete GPU, PCIe 4.0 x16)
PCIe bandwidth (measured):  24.1 GB/s upload, 23.8 GB/s download
[regular_buffer] 256 MB upload:  10.6 ms  (pageable overhead)
[coarse_svm    ] 256 MB access:   1.2 ms  (map + sync, no copy)
[fine_svm      ]                  N/A     (not supported on this device)
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
| Fine-grained system | None | Any `malloc` pointer visible to GPU | ARM Mali |

SVM fine-grained requires a unified cache hierarchy — the CPU and GPU L2/L3 caches must be coherent. Discrete Nvidia GPUs do not expose this via OpenCL.

## Mini-Challenge

Run `svm_theory` on a laptop (iGPU) and a desktop (discrete GPU). Record the `USE_HOST_PTR` time on both. Explain in one paragraph why the iGPU result is ~0 ms, citing the physical memory layout shown above.

## Troubleshooting

- **SVM coarse returns `CL_INVALID_OPERATION`**: device reports OpenCL 2.0 but SVM support is incomplete. Check `clGetDeviceInfo(CL_DEVICE_SVM_CAPABILITIES)` — must be non-zero.
- **Measured PCIe bandwidth far below spec**: run the benchmark with a larger buffer (`--width 16384 --height 16384`). Small transfers don't saturate the bus due to command overhead.

---

[Back to Toolbox.md](../Toolbox.md)
