# Shared Virtual Memory (SVM)

**Symptom**: You are calling `enqueueMapBuffer` / `enqueueUnmapMemObject` on every frame and the map/unmap overhead appears in profiling. Platform is OpenCL 2.0+ on UMA hardware.

> Requires OpenCL 2.0+. To check SVM support, query `CL_DEVICE_SVM_CAPABILITIES` via `clGetDeviceInfo` — a non-zero bitmask means SVM is available. `clinfo | grep "Device OpenCL C"` produces false negatives on Intel NEO drivers and should not be used as the SVM gate. Not supported on Nvidia OpenCL.

## Prerequisites
Prerequisites: OpenCL 1.2+, CMake 3.18+, `clinfo` installed. See [main README](../../README.md) for base requirements.

> **Note**: This tool deliberately targets OpenCL 2.0. It bypasses `common.cmake` and sets `CL_HPP_TARGET_OPENCL_VERSION 200` in its `CMakeLists.txt` to unlock 2.0-only SVM APIs (`clSVMAlloc`, `clEnqueueSVMMap`). All other tools in this toolbox use the 1.2 baseline.

**Additional**:
- OpenCL 2.0+ capable device (AMD APU, Intel integrated GPU, ARM Mali)
- Verify: query `CL_DEVICE_SVM_CAPABILITIES` — non-zero means SVM is available (see [SVMTheory](../../04_Addons/4_4_SVM_Theory/SVMTheory.md) for hardware-level explanation of each SVM tier)

## Build & Run
```bash
cd 99_Toolbox/SVM
cmake -B build && cmake --build build
./build/svm_demo --size 4096
```

## Verify
```
[Buffer + Map/Unmap] Round-trip: 0.82 ms
[Coarse SVM        ] Round-trip: 0.11 ms   (no explicit map needed)
[Fine-Grained SVM  ] Round-trip: 0.03 ms   (atomic access, no sync needed)
```

Run the demo first. If your device lacks SVM support, the demo falls back gracefully and reports which levels are unavailable.

## Concept

SVM allocates a memory region visible to both CPU and GPU at the same virtual address — no explicit transfer commands needed.

| SVM type | Sync required | Use case |
|:---------|:-------------|:---------|
| Coarse-grained | `clEnqueueSVMMap` / `Unmap` | Replace buffer + map patterns |
| Fine-grained buffer | None (cache-coherent) | Shared data structures |
| Fine-grained system | None (any malloc pointer) | Transparent GPU access |

Fine-grained SVM is the most powerful but requires hardware cache coherency between CPU and GPU — available on AMD APUs, Intel integrated, ARM Mali. Discrete Nvidia GPUs do not expose this via OpenCL.

## Mini-Challenge

Switch the demo to coarse-grained SVM and remove the explicit map/unmap calls. Verify the round-trip time drops vs the baseline buffer approach. Then try fine-grained if your device supports it.

## Troubleshooting

- **Demo reports "SVM not supported"**: Your device is OpenCL 1.2 only. SVM is an OpenCL 2.0 feature. Use the [Zero-Copy](../ZeroCopy/ZeroCopy.md) tool instead.
- **Fine-grained SVM not available on Nvidia**: Expected. Nvidia's OpenCL driver caps at 1.2 features. Use CUDA for fine-grained SVM equivalents on Nvidia hardware.

## Used In
- [Track A — A1_OpenCV_Interop](../../02_Projects/A_Multimedia/Multimedia.md) (UMat interop on iGPU)

---

[Back to Toolbox](../Toolbox.md)
