# Audit Findings — All Modules

**Date:** 2026-03-22
**Scope:** Items A + B — fact-checking + redundancy identification across all 6 modules and all subfolder docs. No fixes applied yet (Item C is separate).

**Decision values:** `approve` · `skip` · `override: <your text>`

---

## Files Audited

| Task-specified path | Actual file |
| --- | --- |
| `01_Host_API/README.md` | `01_Host_API/HostAPI.md` |
| `02_Projects/A_Multimedia/README.md` | `02_Projects/A_Multimedia/Multimedia.md` — exists; two cross-links use wrong filename |
| `02_Projects/B_Graphics_HPC/README.md` | `02_Projects/B_Graphics_HPC/GraphicsHPC.md` |
| `02_Projects/C_Robotics_ROS2/README.md` | `02_Projects/C_Robotics_ROS2/RoboticsROS2.md` |
| `99_Toolbox/README.md` | `99_Toolbox/Toolbox.md` + 12 subfolder docs |
| `04_Addons/README.md` | `04_Addons/Addons.md` + 7 subfolder docs |

---

## Action Items for Item C

### 1. FALSE — correct or add `> AUDIT FLAG:` inline

| File | Location | What is wrong | Suggested fix | Decision |
| --- | --- | --- | --- | --- |
| `workflow/design/01-host-api.md` | Known Issues | "`NVIDIA requires CL_QUEUE_PROFILING_ENABLE`" implies NVIDIA is unusual | Rephrase: flag is universally required by all OpenCL implementations | approve |
| `workflow/design/04-multimedia-projects.md` | Cancelled A3.2 note | "Building TFLite from source requires the Android NDK toolchain" | NDK is Android-only; Linux x86_64 uses the standard Linux toolchain | approve |
| `workflow/design/04-multimedia-projects.md` | Known Issues, Intel Xe section | `face_detection_yunet_2022mar.onnx` listed as both failing model and fix | Failing model is likely `2023mar`, fix is `2022mar` | approve |
| `workflow/design/07-toolbox.md` | Phase 2 note | "7× gap visible on discrete GPU" | RTX 4060 Laptop shows 1.3×; AMD Radeon 680M iGPU shows 8.1× — gap is on iGPU, not discrete | approve |
| `99_Toolbox/Debugging/Debugging.md` | AMD profiling section | `rocprof --hsa-trace` for OpenCL tracing | Replace with `--opencl-trace` (or AMDuProfCLI) | approve |
| `99_Toolbox/FastMath/FastMath.md` | Troubleshooting | "Standard `sqrt` clamps negative inputs" | `sqrt(x < 0)` returns NaN per OpenCL spec — does not clamp | approve |
| `99_Toolbox/SyncAtomics/SyncAtomics.md` | CAS loop code | Uses deprecated `atom_cmpxchg` | OpenCL 1.2 name is `atomic_cmpxchg` | approve |
| `99_Toolbox/ThreadDivergence/ThreadDivergence.md` | Core concept table | "AMD RDNA uses 32-wide waves" | RDNA default is wave64; wave32 is opt-in | approve |
| `99_Toolbox/SVM/SVM.md` | Prerequisites | `` `clinfo \| grep 'Device OpenCL C'` `` SVM detection | Produces false negatives on Intel NEO; correct gate: `CL_DEVICE_SVM_CAPABILITIES` | approve |
| `04_Addons/4_4_SVM_Theory/SVMTheory.md` | SVM levels table | "Fine-grained system SVM: Apple (via Metal)" | Apple deprecated OpenCL in macOS 10.14; remove Apple row | approve |

### 2. CONTRADICTION — one file disagrees with the other

| File to fix | Claims | Authoritative source | Decision |
| --- | --- | --- | --- |
| `04_Addons/Addons.md` | ROS 2 "Humble+" for addon 4.5 Voxel Mapping | `workflow/design/08-addons.md` specifies Jazzy as a hard dependency | approve |

### 3. UNVERIFIED — confirm or add `> AUDIT FLAG:` inline

| File | Claim | Why unverified | Decision |
| --- | --- | --- | --- |
| `workflow/design/06-robotics-ros2-projects.md` | "`rmw_cyclonedds_cpp` does not support loaned messages" | Version-dependent; no specific version cited | approve |
| `04_Addons/4_2_OpenCL_vs_CUDA/OpenCLvsCUDA.md` | "Xilinx/Intel FPGAs use OpenCL as their primary compute API" | Accurate for Intel; AMD/Xilinx shifted toward SYCL/HLS C++ as of 2024 | approve |
| `04_Addons/4_3_Deployment/Deployment.md` | `FROM rocm/opencl-dev` Docker base image | AMD images use different naming (`rocm/dev-ubuntu-*`); tag unverified | approve |
| `04_Addons/4_5_Voxel_Mapping/VoxelMapping.md` | "Voxel grid 200×200×50 = 3 MB" | Storage type undocumented — `uint`→8 MB, `uchar`→2 MB | approve |

### 4. STALE — remove from file

| File | Content to remove | Decision |
| --- | --- | --- |
| `02_Projects/C_Robotics_ROS2/RoboticsROS2.md` | Known issue: "`SyntheticPublisher` merged into `main.cpp`" — marked RESOLVED in design doc | approve |

### 5. REDUNDANCY — remove duplicate within single file

| Content | File | Duplicate location | Decision |
| --- | --- | --- | --- |
| Mixed-scene cluster coordinates `(2,0,1), (−2,0,1), (0,3,1)` | `02_Projects/C_Robotics_ROS2/RoboticsROS2.md` | Second occurrence in C3 Challenge Verify section (~lines 381–384) | approve |

### 6. BROKEN LINKS — fix link targets

| Source file | Current target | Correct target | Decision |
| --- | --- | --- | --- |
| `01_Host_API/HostAPI.md` | `../02_Projects/A_Multimedia/README.md` | `../02_Projects/A_Multimedia/Multimedia.md` | approve |
| `02_Projects/B_Graphics_HPC/GraphicsHPC.md` | `../A_Multimedia/README.md` | `../A_Multimedia/Multimedia.md` | approve |

### 7. STRUCTURAL — human decision required

| File | Issue | Recommendation | Decision |
| --- | --- | --- | --- |
| `02_Projects/A_Multimedia/Multimedia.md` | A5_Privacy_Mode Contents entry has multi-line inline editorial reasoning | Move rationale to design doc; README entry should be present or absent cleanly | skip, V2 will have cleanup |
| `99_Toolbox/SVM/SVM.md` + `04_Addons/4_4_SVM_Theory/SVMTheory.md` | SVM levels table near-identical in both files | Add cross-reference: "see SVMTheory.md for hardware explanation" | approve + link to addons |
| `99_Toolbox/ZeroCopy/ZeroCopy.md` + `04_Addons/4_4_SVM_Theory/SVMTheory.md` | Three-flag buffer table duplicated across both files | Add cross-reference in one of the two | approve + link to addons |

---

## Reference — Suggested Resources

- [AMD ROCm profiling — `rocprof` flags](https://rocm.docs.amd.com/projects/rocprofiler/en/latest/) — `--opencl-trace` vs `--hsa-trace` (§1 Debugging.md)
- [OpenCL 1.2 spec §6.11.11 — Atomic functions](https://www.khronos.org/registry/OpenCL/specs/opencl-1.2.pdf) — confirms `atomic_cmpxchg` (§1 SyncAtomics.md)
- [AMD RDNA3 ISA Reference Guide](https://gpuopen.com/rdna3-isa) — wave64 default, wave32 opt-in (§1 ThreadDivergence.md)
- [OpenCL 3.0 SVM capabilities query](https://registry.khronos.org/OpenCL/specs/3.0-unified/html/OpenCL_API.html#CL_DEVICE_SVM_CAPABILITIES) — correct SVM detection gate (§1 SVM.md)
- [rmw_cyclonedds loaned messages](https://github.com/ros2/rmw_cyclonedds) — version tracking (§3 Robotics design doc)
- [YuNet model changelog](https://github.com/opencv/opencv_zoo/tree/main/models/face_detection_yunet) — model versions (§1 multimedia design doc)
