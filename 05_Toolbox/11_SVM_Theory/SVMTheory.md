# SVM and Zero-Copy: Deep Dive

**When to use**: the [Zero-Copy toolbox entry](../../05_Toolbox/15_Zero_Copy/ZeroCopy.md) wasn't enough — you want to understand *why* it works at the hardware level.

> **Requires:** OpenCL 2.0+ device (AMD APU, Intel iGPU, ARM Mali) for SVM modes. Falls back gracefully with an informational message on OpenCL 1.2 devices. Not supported on NVIDIA OpenCL.

## Prerequisites
See [main README](../../README.md) for base requirements (OpenCL 1.2+, CMake 3.18+).

- [Toolbox: Zero-Copy](../../05_Toolbox/15_Zero_Copy/ZeroCopy.md) — read this first
- OpenCL 2.0+ device for the SVM sections (AMD APU, Intel integrated, ARM Mali)

## Build & Run
> **Build note:** Users with an OpenCL 1.2-only SDK compile in 1.2-only mode (no SVM symbols). Verify your device's OpenCL C version with `clinfo | grep 'Device OpenCL C Version'` — 2.0+ is required for the SVM modes.

```bash
cd 05_Toolbox/11_SVM_Theory
cmake -B build && cmake --build build

# Run all 5 modes (default):
./build/svm_theory

# Run a specific mode:
./build/svm_theory --mode buffer_map
./build/svm_theory --mode copy_host_ptr
./build/svm_theory --mode use_host_ptr
./build/svm_theory --mode coarse_svm
./build/svm_theory --mode fine_svm
./build/svm_theory --mode all

# Custom image size (larger = more representative transfer times):
./build/svm_theory --width 8192 --height 8192

# iGPU path (near-zero transfer time expected):
GPU=INTEL ./build/svm_theory --mode all
```

## Modes

| Mode | Flag value | OpenCL | Transfer timing | Description |
|:-----|:-----------|:-------|:----------------|:------------|
| Buffer + Map/Unmap | `buffer_map` | 1.x+ | `enqueueMapBuffer` + `Unmap` latency (chrono) | **Baseline**: `CL_MEM_COPY_HOST_PTR` + explicit map/unmap fence |
| Copy host ptr | `copy_host_ptr` | 1.x+ | `enqueueWriteBuffer` (cl::Event) | Driver allocates device memory and copies on creation |
| Use host ptr | `use_host_ptr` | 1.x+ | `enqueueWriteBuffer` (cl::Event) | Driver may zero-copy on UMA hardware |
| Coarse SVM | `coarse_svm` | 2.0+ | `clEnqueueSVMMap` + `Unmap` round-trip (chrono) | Shared pointer with explicit map/unmap cache fences |
| Fine SVM | `fine_svm` | 2.0+ | 0 ms (direct coherent write) | Fully coherent: CPU writes visible to GPU with no fence |

### `buffer_map` — the OpenCL 1.x baseline
`buffer_map` is the universal baseline path: `CL_MEM_COPY_HOST_PTR` + explicit `enqueueMapBuffer`/`enqueueUnmapMemObject` fences — no OpenCL 2.0 features required. See the Modes table above for a full comparison of all five paths.

## Verify
```
Device: Intel(R) Iris(R) Xe Graphics

[buffer_map    ]  Transfer: 0.017 ms  Kernel: 0.048 ms
[copy_host_ptr ]  Transfer: 0.067 ms  Kernel: 0.056 ms
[use_host_ptr  ]  Transfer: 0.042 ms  Kernel: 0.059 ms
[coarse_svm    ]  Transfer: 0.001 ms  Kernel: 0.064 ms
[fine_svm      ]  SKIPPED — CL_DEVICE_SVM_FINE_GRAIN_SYSTEM not supported
```

## Concept: Why UMA Changes Everything

> **Data format note:** This module uses `uchar4` (RGBA pixels) as the benchmark data type. For why 4-channel aligned access matters for memory bandwidth, see [05_Toolbox/02_Coalesced_Access/CoalescedAccess.md](../02_Coalesced_Access/CoalescedAccess.md).

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
    Bandwidth limited by system memory (85–140 GB/s on modern APUs, depending on DDR5/LPDDR5x configuration).
```

## SVM Levels

| SVM type | Sync required | Coherency | Typical hardware |
|:---------|:-------------|:----------|:-----------------|
| Coarse-grained | `clEnqueueSVMMap` / `Unmap` | Manual | Most OpenCL 2.0 devices |
| Fine-grained buffer | None | Cache-coherent | AMD APU, Intel Arc |
| Fine-grained system | None | Any `malloc` pointer visible to GPU | ARM Mali |

SVM fine-grained requires a unified cache hierarchy — the CPU and GPU L2/L3 caches must be coherent. Discrete Nvidia GPUs do not expose this via OpenCL.

## Mini-Challenge

Run `svm_theory` on a laptop (iGPU) and a desktop (discrete GPU). Record the `Transfer` time for
`buffer_map` and `use_host_ptr` on both. Explain in one paragraph why the iGPU transfer time
is ~0 ms, citing the physical memory layout shown above.

## Troubleshooting

- **SVM coarse returns `CL_INVALID_OPERATION`**: device reports OpenCL 2.0 but SVM support is incomplete. Check `clGetDeviceInfo(CL_DEVICE_SVM_CAPABILITIES)` — must be non-zero.
- **Measured PCIe bandwidth far below spec**: run the benchmark with a larger buffer (`--width 16384 --height 16384`). Small transfers don't saturate the bus due to command overhead.
- **`coarse_svm` or `fine_svm` exits with `[INFO]` message**: device does not support SVM (typical on NVIDIA OpenCL 1.2). Use `buffer_map`, `copy_host_ptr`, or `use_host_ptr` instead.

---

[Back to Toolbox.md](../Toolbox.md)
