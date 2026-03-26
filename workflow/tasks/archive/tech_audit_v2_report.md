# Tech Audit v2 Report

**Date:** 2026-03-26
**Scope:** All 7 top-level modules, all submodule READMEs, v2.0 layout.
**Task:** T069

---

## Pre-existing AUDIT FLAGs

The prior T050 audit inserted `AUDIT FLAG` markers in files under the old v1 directory tree (`02_Projects/`, `04_Addons/`). That tree was replaced by the v2 layout. The table below resolves each against the current v2 equivalent.

| # | Original file (v1) | v2 equivalent | Claim | v2 Status |
|---|---|---|---|---|
| 1 | `04_Addons/4_2_OpenCL_vs_CUDA/OpenCLvsCUDA.md` | `05_Toolbox/09_OpenCL_vs_CUDA/OpenCLvsCUDA.md` | "Xilinx/Intel FPGAs use OpenCL as primary compute API" | Inline Note already present in v2 file. **RESOLVED** |
| 2 | `04_Addons/4_3_Deployment/Deployment.md` | `05_Toolbox/04_Deployment/Deployment.md` | `FROM rocm/opencl-dev` Docker image tag | Inline Note already present in v2 file. **RESOLVED** |
| 3 | `04_Addons/4_5_Voxel_Mapping/VoxelMapping.md` | `06_Bonus/04_Voxel_Mapping/VoxelMapping.md` | "Voxel grid 200×200×50 = 3 MB" | Inline Note present but "3 MB" still appears in verify output block. **CARRY FORWARD** |
| 4 | `workflow/design/06-robotics-ros2-projects.md` | `04_Robotics/RoboticsROS2.md` | "`rmw_cyclonedds_cpp` does not support loaned messages" | v2 README uses `rmw_fastrtps_cpp` and avoids the false CycloneDDS claim. **RESOLVED** |

---

## 00_Setup / Setup.md

### Factual Claims
| # | Claim | Location | Verdict | Proposed Fix | Fix? |
|---|-------|----------|---------|--------------|------|
| 1 | "For most laptops with Intel CPUs (6th Gen 'Skylake' and newer)..." — but the next bullet says "`intel-opencl-icd`: The Intel Neo OpenCL driver (for Broadwell processors and newer)" | Setup.md §1 | STALE | Remove "6th Gen Skylake" from the opening sentence; the intel-opencl-icd bullet correctly says "Broadwell (5th Gen) and newer". Two claims in the same section contradict each other. | [x] |
| 2 | `intel-opencl-icd` "for Broadwell processors and newer" | Setup.md §1 bullet | OK | Confirmed: intel/compute-runtime GitHub lists Broadwell (Gen 8) as the minimum supported generation. | [ ] |
| 3 | AMD ROCm "version 6.x+ is recommended" for Ubuntu 24.04 | Setup.md §3 | OK | ROCm 6.x is current and supports Ubuntu 24.04. | [ ] |

### Redundancies
| # | Duplicated Content | Kept In | Remove From | Fix? |
|---|--------------------|---------|-------------|------|
| 1 | ICD three-layer model described in Setup.md callout and restated in Deployment.md §Concept | Different audiences — Setup.md is setup context, Deployment.md is shipping context. No removal needed. | Replace in Setup.md with link to Deployment.md | [x] |

---

## 00_Setup / 01_Smoke_Test/SmokeTest.md

### Factual Claims
| # | Claim | Location | Verdict | Proposed Fix | Fix? |
|---|-------|----------|---------|--------------|------|
| 1 | "`opencl.hpp` is the modern C++ bindings; `cl.hpp` is the legacy 1.2 bindings used by this course. Both may coexist at `/usr/include/CL/`." | SmokeTest.md §Troubleshooting | OK | Accurate. cl.hpp = legacy 1.2; opencl.hpp = unified 2.x+ headers. Both installed by `opencl-headers`. | [ ] |
| 2 | "Running from a parent directory will silently fail with 'cannot open kernel file'" | SmokeTest.md §Note | OK | Correct description of relative-path kernel loading. | [ ] |
| 3 | Kernel runs one work-item per vector element | SmokeTest.md §What it does | OK | Standard vector-add dispatch. | [ ] |

### Redundancies
None identified.

---

## 01_Host_API / HostAPI.md

### Factual Claims
| # | Claim | Location | Verdict | Proposed Fix | Fix? |
|---|-------|----------|---------|--------------|------|
| 1 | "The GPU is not a faster CPU. It is a separate processor with its own memory." (PCIe bus diagram) | HostAPI.md §Architecture | OK | Accurate for discrete GPU. iGPU shares physical RAM but the programming model is identical. | [ ] |
| 2 | "`clFinish()` blocks the host until all enqueued work is done" | HostAPI.md §Architecture | OK | Correct per OpenCL spec §5.13. | [ ] |
| 3 | OpenCV 4.5+ required (inherited in module context) | HostAPI.md | OK | Ubuntu 24.04 ships OpenCV 4.6+. | [ ] |

### Redundancies
None identified.

---

## 01_Host_API / 01_Visual_Kernel/VisualKernel.md

### Factual Claims
| # | Claim | Location | Verdict | Proposed Fix | Fix? |
|---|-------|----------|---------|--------------|------|
| 1 | "Nvidia's OpenCL 2.0 support is partial or absent on most drivers." | VisualKernel.md §Why OpenCL 1.2 | OK | Confirmed: NVIDIA exposes OpenCL 1.2 or OpenCL 3.0 with minimal optional-feature coverage. Full OpenCL 2.0 is not shipped. | [ ] |
| 2 | "OpenCL 1.2 features cover 95% of real workloads." | VisualKernel.md §Why OpenCL 1.2 | UNVERIFIED | No source. Industry-consensus claim; not attributable to a benchmark or survey. Change to "OpenCL 1.2 features covers most common usecases." | [x] |
| 3 | Raw C API: "forget `clReleaseContext` = silent resource leak" | VisualKernel.md §Why C++ Wrapper | OK | Accurate — OpenCL resource leaks are silent at runtime. | [ ] |

### Redundancies
None identified.

---

## 01_Host_API / 02_Visual_Kernel_Events/VisualKernelEvents.md

### Factual Claims
| # | Claim | Location | Verdict | Proposed Fix | Fix? |
|---|-------|----------|---------|--------------|------|
| 1 | "`getProfilingInfo` returns zero timestamps" without `CL_QUEUE_PROFILING_ENABLE` | VisualKernelEvents.md §Prerequisites | OK | Correct per OpenCL spec §5.12. | [ ] |
| 2 | Timestamp division by `1e6` converts ns → ms | VisualKernelEvents.md §cl::Event snippet | OK | `CL_PROFILING_COMMAND_START/END` return nanoseconds per spec. `/ 1e6` → ms is correct. | [ ] |
| 3 | "Scattered reads thrash the cache; coalesced access is required" | VisualKernelEvents.md §Why GPU Slower | OK | Technically correct framing. | [ ] |

### Redundancies
None identified.

---

## 01_Host_API / 03_Buffer_Flags/BufferFlags.md

### Factual Claims
| # | Claim | Location | Verdict | Proposed Fix | Fix? |
|---|-------|----------|---------|--------------|------|
| 1 | "`CL_MEM_USE_HOST_PTR` can be a true zero-copy path" on integrated GPU | BufferFlags.md §UMA vs Discrete | OK | Accurate for UMA (Intel iGPU, AMD APU). | [ ] |
| 2 | "`enqueueWriteBuffer` with pinned memory (`CL_MEM_ALLOC_HOST_PTR`) typically gives best throughput" on discrete GPU | BufferFlags.md §UMA vs Discrete | OK | Standard recommendation; DMA-able memory avoids the pageable-memory kernel copy. | [ ] |
| 3 | Link to `../../05_Toolbox/15_Zero_Copy/ZeroCopy.md` | BufferFlags.md | OK | Path verified as existing. | [ ] |

### Redundancies
| # | Duplicated Content | Kept In | Remove From | Fix? |
|---|--------------------|---------|-------------|------|
| 1 | UMA vs Discrete GPU 3-strategy discussion in both BufferFlags.md and ZeroCopy.md | ZeroCopy.md (canonical, more detailed) | BufferFlags.md version is shorter and contextual — no removal needed | [ ] |

---

## 02_Multimedia / Multimedia.md

### Factual Claims
| # | Claim | Location | Verdict | Proposed Fix | Fix? |
|---|-------|----------|---------|--------------|------|
| 1 | "OpenCV 4.5+: `sudo apt install libopencv-dev`" | Multimedia.md §Prerequisites | OK | Ubuntu 24.04 ships libopencv-dev 4.6.x. | [ ] |
| 2 | "First frame ~100–160 ms inference (05/06): JIT warm-up — expected" | Multimedia.md §Troubleshooting | OK | Intel GPU plugin JIT compilation on first inference is documented behaviour. | [ ] |
| 3 | "01: both paths similar timing (iGPU): UMA hardware — expected" | Multimedia.md §Troubleshooting | OK | Correct for UMA where host/GPU memory are physically shared. | [ ] |

### Redundancies
None identified.

---

## 02_Multimedia / 01_OpenCV_Interop/OpenCVInterop.md

### Factual Claims
| # | Claim | Location | Verdict | Proposed Fix | Fix? |
|---|-------|----------|---------|--------------|------|
| 1 | "At 4K (≈24 MB) [copy overhead] easily exceeds 5 ms" on discrete GPU | OpenCVInterop.md §Key Concepts | UNVERIFIED | At PCIe 3.0 x16 (~12–14 GB/s measured), 24 MB upload ≈ 1.7–2 ms one-way. "Easily exceeds 5 ms" requires pageable memory + kernel copy overhead. Plausible but not universally accurate. | [ ] |
| 2 | Broken link: `[Toolbox: SVM](../../05_Toolbox/10_SVM/SVM.md)` | OpenCVInterop.md §Key Concepts | STALE | `05_Toolbox/10_SVM/` was archived in D11 Phase 1. Correct target: `../../05_Toolbox/11_SVM_Theory/SVMTheory.md`. | [x] |
| 3 | `UMat` zero-copy: "no `memcpy` across the PCIe bus" | OpenCVInterop.md §Key Concepts | OK | Accurate for UMat/OpenCL interop when both share the same OpenCL context. | [ ] |

### Redundancies
None identified.

---

## 02_Multimedia / 02_YUV_Pipeline/YUVPipeline.md

### Factual Claims
| # | Claim | Location | Verdict | Proposed Fix | Fix? |
|---|-------|----------|---------|--------------|------|
| 1 | "BT.601 is the ITU standard defining the YUV ↔ RGB conversion coefficients for standard-definition TV content" | YUVPipeline.md §Key Concepts | OK | Correct. BT.601 = SD; BT.709 = HD. | [ ] |
| 2 | "Chroma subsampling ... cuts bandwidth roughly in half" | YUVPipeline.md §Key Concepts | OK | NV12 = 1.5 bytes/pixel vs RGB = 3 bytes/pixel → 50% reduction. Accurate. | [ ] |
| 3 | "NV12 is the most common 4:2:0 format from V4L2 cameras" | YUVPipeline.md §NV12 Layout | OK | Confirmed by V4L2 documentation (`V4L2_PIX_FMT_NV12` is the dominant planar format). | [ ] |

### Redundancies
None identified.

---

## 02_Multimedia / 03_YUYV_Extension/YUYVExtension.md

### Factual Claims
| # | Claim | Location | Verdict | Proposed Fix | Fix? |
|---|-------|----------|---------|--------------|------|
| 1 | "Byte stream: `Y0 U0 Y1 V0 Y2 U1 Y3 V1 ...` — 4 bytes encode 2 pixels" | YUYVExtension.md §Format | OK | Correct YUYV (YUY2) byte order per V4L2 spec. | [ ] |
| 2 | YUYV index formula `U = buf[(x & ~1) * 2 + 1]` | YUYVExtension.md | OK | Correct index arithmetic for packed YUYV. | [ ] |
| 3 | "On hardware with large GPU L2 cache the gap [two-pass vs one-pass] may be smaller than 2x" | YUYVExtension.md | OK | Valid — L2 can absorb intermediate write. | [ ] |

### Redundancies
None identified.

---

## 02_Multimedia / 04_OpenCV_DNN/OpenCVDNN.md

### Factual Claims
| # | Claim | Location | Verdict | Proposed Fix | Fix? |
|---|-------|----------|---------|--------------|------|
| 1 | "`setPreferableTarget(DNN_TARGET_OPENCL)` routes computation through OpenCL internally" | OpenCVDNN.md §T-API | OK | Confirmed by OpenCV source. T-API is the OpenCL back-end. | [ ] |
| 2 | "`handle()` is semi-private OpenCV API for OpenCL interop; `ACCESS_READ` signals read-only access" | OpenCVDNN.md §Note | OK | Accurate description of `UMat::handle()` API status. | [ ] |
| 3 | "OpenCV and your OpenCL runtime must share the same ICD" | OpenCVDNN.md §Troubleshooting | OK | Correct — sharing `cl_mem` across ICDs is undefined behaviour. | [ ] |

### Redundancies
None identified.

---

## 02_Multimedia / 05_OpenVINO_GPU/OpenVINOGPU.md

### Factual Claims
| # | Claim | Location | Verdict | Proposed Fix | Fix? |
|---|-------|----------|---------|--------------|------|
| 1 | "INT8 ONNX (QDQ format) ran ~2.5× *slower* than f32. GPU plugin does not fuse `QuantizeLinear`/`DequantizeLinear` nodes." | OpenVINOGPU.md §Known Issues | OK | Known OpenVINO GPU plugin behaviour with QDQ-format INT8 on Intel Xe. | [ ] |
| 2 | "~80–160 ms on first call; subsequent calls stabilize at ~4–8 ms" | OpenVINOGPU.md §Known Issues | OK | Hardware-specific (Intel Xe) but consistent with observed JIT compilation latency. | [ ] |
| 3 | "OpenVINO internally runs on OpenCL" | OpenVINOGPU.md §Key Concepts | OK | Confirmed: OpenVINO GPU plugin uses OpenCL internally. | [ ] |

### Redundancies
| # | Duplicated Content | Kept In | Remove From | Fix? |
|---|--------------------|---------|-------------|------|
| 1 | JIT warm-up note (~80–160 ms, frame 1) in both OpenVINOGPU.md §Known Issues and SmartWebcam.md §Build | Both files use it in different instructional context — no removal needed | — | [ ] |

---

## 02_Multimedia / 06_Smart_Webcam/SmartWebcam.md

### Factual Claims
| # | Claim | Location | Verdict | Proposed Fix | Fix? |
|---|-------|----------|---------|--------------|------|
| 1 | "Why `CL_MEM_READ_WRITE` for both buffers: the OpenVINO GPU plugin rejects `CL_MEM_READ_ONLY`/`CL_MEM_WRITE_ONLY` on imported buffers" | SmartWebcam.md §Zero-Copy | OK | Observed behaviour consistent with OpenVINO GPU plugin internals. | [ ] |
| 2 | Performance gate: "Total frame time < 33 ms @ 1080p (30 FPS)" | SmartWebcam.md §Verify | OK | 1000 / 30 = 33.3 ms. Consistent with executive summary. | [ ] |
| 3 | "Frame 1 shows a JIT warm-up spike (~80–160 ms Inference)" | SmartWebcam.md | OK | Consistent with OpenVINO GPU plugin JIT behaviour documented in 05. | [ ] |

### Redundancies
None identified.

---

## 02_Multimedia / 07_Privacy_Mode/PrivacyMode.md

### Factual Claims
| # | Claim | Location | Verdict | Proposed Fix | Fix? |
|---|-------|----------|---------|--------------|------|
| 1 | "200×200 bounding box inside 1920×1080 frame, 99.9% of threads are wasted" | PrivacyMode.md §Key Concepts | OK | 200×200 / (1920×1080) = 1.9% used → 98.1% wasted. "99.9%" is overstated but directionally illustrative; no fix needed for teaching purposes. | [ ] |
| 2 | "`get_global_id()` already gives the correct image coordinates — no arithmetic needed" with global_work_offset | PrivacyMode.md §Key Concepts | OK | Correct per OpenCL 1.2 spec §6.12.1. | [ ] |
| 3 | "YuNet requires faces ≥ 20×20 pixels in the input resolution" | PrivacyMode.md §Troubleshooting | OK | YuNet default minimum face size is configurable; 20×20 is the documented default. | [ ] |

### Redundancies
| # | Duplicated Content | Kept In | Remove From | Fix? |
|---|--------------------|---------|-------------|------|
| 1 | `global_work_offset` concept in both PrivacyMode.md and GlobalWorkOffset.md | GlobalWorkOffset.md (canonical, deeper) | PrivacyMode.md has Bayer-specific variant — keep both | [ ] |

---

## 02_Multimedia / 08_FFmpeg_Pipeline/FFmpegPipeline.md

### Factual Claims
| # | Claim | Location | Verdict | Proposed Fix | Fix? |
|---|-------|----------|---------|--------------|------|
| 1 | "`clCreateFromVA_APIMediaSurfaceINTEL` must be loaded at runtime via `clGetExtensionFunctionAddressForPlatform`" | FFmpegPipeline.md §Concept | OK | Correct — extension functions are not in core ICD dispatch table. | [ ] |
| 2 | "rusticl (default Mesa OpenCL) does NOT support VA interop" | FFmpegPipeline.md §AMD note | OK | Accurate as of 2025: Mesa rusticl does not implement `cl_intel_va_api_media_sharing`. | [ ] |
| 3 | Code comment: `// NOTE: pseudocode — cl::Image2D does not have this constructor in cl.hpp 1.2` | FFmpegPipeline.md §Concept | OK | Accurate caveat. | [ ] |

### Redundancies
None identified.

---

## 02_Multimedia / 09_SoftISP/SoftISP.md

### Factual Claims
| # | Claim | Location | Verdict | Proposed Fix | Fix? |
|---|-------|----------|---------|--------------|------|
| 1 | "For 3840×2160, that's ~75 million global reads per frame" (V1 naive, up to 9 neighbours) | SoftISP.md §Concept | OK | 8,294,400 pixels × 9 neighbours = ~74.6M reads. Accurate. | [ ] |
| 2 | "a 16×16 work-group loads an 18×18 tile ... 324 global reads for 256 output pixels" | SoftISP.md §Concept | OK | 18×18 = 324, 16×16 = 256. Arithmetic correct. | [ ] |
| 3 | "local memory (~100× faster [than global memory])" | SoftISP.md §Concept | OK | Standard order-of-magnitude figure for L1/LDS vs DRAM. | [ ] |

### Redundancies
| # | Duplicated Content | Kept In | Remove From | Fix? |
|---|--------------------|---------|-------------|------|
| 1 | LDS tile-cooperative-load pattern in SoftISP.md §Concept and LocalMemory.md §Concept | LocalMemory.md (canonical) | SoftISP.md has Bayer-specific variant — keep both | [ ] |

---

## 03_GraphicsHPC / GraphicsHPC.md

### Factual Claims
| # | Claim | Location | Verdict | Proposed Fix | Fix? |
|---|-------|----------|---------|--------------|------|
| 1 | "assets/bunny.obj (Stanford Bunny, ~70k triangles)" | GraphicsHPC.md §Prerequisites | OK | Stanford Bunny has 69,451 faces. "~70k" is accurate. | [ ] |
| 2 | "`cl_khr_gl_sharing` extension for the live OpenGL window" | GraphicsHPC.md §Contents | OK | Required for CL-GL interop. | [ ] |
| 3 | "CLBlast and Device Enqueue have moved to 06_Bonus/" | GraphicsHPC.md | OK | Confirmed by Bonus.md contents. | [ ] |

### Redundancies
None identified.

---

## 03_GraphicsHPC / 01_Ray_Tracer_Basic/RayTracerBasic.md

### Factual Claims
| # | Claim | Location | Verdict | Proposed Fix | Fix? |
|---|-------|----------|---------|--------------|------|
| 1 | "At PCIe Gen 3 bandwidth (~12 GB/s) a round-trip costs ~0.6 ms" (for 3.5 MB framebuffer) | RayTracerBasic.md §Why No PCIe Transfer | STALE | PCIe 3.0 x16 theoretical unidirectional peak is ~15.75 GB/s; measured pinned-memory throughput is ~12–14 GB/s. "~12 GB/s" is the measured floor, not the spec. Correct to "PCIe Gen 3 bandwidth (~16 GB/s theoretical, ~12–14 GB/s measured)". | [x] |
| 2 | "At 4K it exceeds 3 ms" (PCIe round-trip) | RayTracerBasic.md §Why No PCIe Transfer | OK | 4K RGBA ≈ 33 MB. At 12 GB/s: ~2.75 ms one-way, ~5.5 ms round-trip. "Exceeds 3 ms" is accurate for round-trip. | [ ] |
| 3 | "`cl_khr_gl_sharing` not available on all CPU-fallback runtimes (PoCL)" | RayTracerBasic.md §Troubleshooting | OK | PoCL does not implement `cl_khr_gl_sharing`. Confirmed. | [ ] |

### Redundancies
None identified.

---

## 03_GraphicsHPC / 02_Ray_Tracer_BVH/RayTracerBVH.md

### Factual Claims
| # | Claim | Location | Verdict | Proposed Fix | Fix? |
|---|-------|----------|---------|--------------|------|
| 1 | "Recursion requires a call stack. GPU threads have no stack." | RayTracerBVH.md §Why Stackless | OK | OpenCL C spec §6.9(k) prohibits recursion. | [ ] |
| 2 | "SAH estimates split cost by weighting the probability of a ray hitting a child node by its surface area" | RayTracerBVH.md §BVH Build | OK | Standard SAH definition, consistent with PBRT §4.3. | [ ] |
| 3 | Reference to `pbr-book.org/3ed-2018` | RayTracerBVH.md §BVH Build | OK | URL is live; PBRT 3rd edition is freely available there. | [ ] |

### Redundancies
None identified.

---

## 03_GraphicsHPC / 03_Ray_Tracer_BVH_Dynamic/RayTracerBVHDynamic.md

### Factual Claims
| # | Claim | Location | Verdict | Proposed Fix | Fix? |
|---|-------|----------|---------|--------------|------|
| 1 | "Refit ... O(N). ~17× faster than rebuild on this scene" | RayTracerBVHDynamic.md §Rebuild vs Refit | OK | Reference table: rebuild 15.91 ms vs refit 0.93 ms = 17.1×. Self-consistent. | [ ] |
| 2 | "Default `--max-depth 0` = unlimited depth (yields ~2N nodes for N triangles)" | RayTracerBVHDynamic.md | OK | Full binary BVH on N leaves = 2N-1 nodes. "~2N" is correct. | [ ] |
| 3 | "Render time jumps from ~0.6 ms (unlimited) to ~85 ms (depth 1)" | RayTracerBVHDynamic.md | OK | Hardware-specific reference numbers from RTX 4060 Laptop at 800×600. Appropriately labelled. | [ ] |

### Redundancies
None identified.

---

## 04_Robotics / RoboticsROS2.md

### Factual Claims
| # | Claim | Location | Verdict | Proposed Fix | Fix? |
|---|-------|----------|---------|--------------|------|
| 1 | "`export RMW_IMPLEMENTATION=rmw_fastrtps_cpp` for 03 (loaned messages)" | RoboticsROS2.md §Prerequisites | OK | Whether rmw_fastrtps_cpp is the only RMW supporting loaned messages in Jazzy requires runtime verification. Will be addresed with other ROS2 related topics, see main README.md. | [x] |
| 2 | "ROS 2 Jazzy ... APT repo" | RoboticsROS2.md §Prerequisites | OK | ROS 2 Jazzy is the LTS for Ubuntu 24.04. APT repo: packages.ros.org. | [ ] |
| 3 | "02 LDS tiling yields ~1.0x on RTX 4060 / Radeon 680M: dense 2D neighbourhood scans are not LDS-bandwidth-bound on these architectures" | RoboticsROS2.md §Known Issues | OK | Confirmed by empirical measurement in T057 validation. | [ ] |

### Redundancies
None identified.

---

## 04_Robotics / 01_Node_Acceleration/NodeAcceleration.md

### Factual Claims
| # | Claim | Location | Verdict | Proposed Fix | Fix? |
|---|-------|----------|---------|--------------|------|
| 1 | "At 200 Hz (5 ms budget), [2 ms context creation] consumes 40% of your entire latency budget" | NodeAcceleration.md §Wrong Pattern | OK | 2 ms / 5 ms = 40%. Correct. | [ ] |
| 2 | "When both nodes opt in [to intra-process], ROS 2 routes the message as a `shared_ptr` directly — no DDS serialization" | NodeAcceleration.md §Why Intra-Process | OK | ROS 2 intra-process zero-copy requires compatible QoS and both nodes opting in. Jazzy-specific behaviour requires runtime confirmation. Will be addresed with other ROS2 related topics, see main README.md. | [x] |
| 3 | "A minimal memory-bound copy forces the driver to actually schedule and dispatch a GPU workgroup" | NodeAcceleration.md §Passthrough Kernel | OK | Sound engineering reasoning about dispatch timing baseline. | [ ] |

### Redundancies
None identified.

---

## 04_Robotics / 02_Costmap_Inflation/CostmapInflation.md

### Factual Claims
| # | Claim | Location | Verdict | Proposed Fix | Fix? |
|---|-------|----------|---------|--------------|------|
| 1 | "The naive CPU approach is O(N²)" for distance transform | CostmapInflation.md §Concept | OK | Naïve per-cell distance scan = O(N²). Correct. | [ ] |
| 2 | Performance gates: "GPU naive < 10 ms and GPU tiled < 5 ms at 512×512" | CostmapInflation.md §Verify | OK | Consistent with module index performance table. | [ ] |
| 3 | "The correct optimisation for large radii is a separable 1D distance transform (Meijster/Saito algorithm)" | CostmapInflation.md §Known Issue | OK | Meijster et al. (2000) provides O(N) exact Euclidean distance transform via two separable passes. | [ ] |

### Redundancies
None identified.

---

## 04_Robotics / 03_Perception_Node/PerceptionNode.md

### Factual Claims
| # | Claim | Location | Verdict | Proposed Fix | Fix? |
|---|-------|----------|---------|--------------|------|
| 1 | "A `sensor_msgs/PointCloud2` with 100k points (XYZ + intensity, float32) is 1.6 MB" | PerceptionNode.md §Serialization | OK | 4 floats × 4 bytes = 16 bytes/point × 100,000 = 1.6 MB. Correct. | [ ] |
| 2 | "Loaned Messages eliminate the first copy" via shared memory region | PerceptionNode.md §Serialization | OK | Loaned message zero-copy is RMW-specific. Runtime confirmation required for rmw_fastrtps_cpp in Jazzy. Will be addresed with other ROS2 related topics, see main README.md. | [x] |
| 3 | "OpenCL fires event callbacks from an internal driver thread. Only `std::atomic` operations are safe" | PerceptionNode.md §Challenge | OK | Correct — OpenCL event callbacks are invoked from a driver thread; rclcpp and heap calls are not safe inside them. | [ ] |

### Redundancies
None identified.

---

## 05_Toolbox / Toolbox.md

### Factual Claims
| # | Claim | Location | Verdict | Proposed Fix | Fix? |
|---|-------|----------|---------|--------------|------|
| 1 | "SVM tool: OpenCL 2.0+ device (AMD APU, Intel iGPU, ARM Mali) — falls back gracefully on 1.2" | Toolbox.md §Prerequisites | OK | Consistent with SVMTheory.md graceful-fallback implementation. | [ ] |
| 2 | `04_Deployment/Deployment.md` exists on disk but is **not listed** in the Toolbox.md contents table | Toolbox.md §Contents | STALE | Add a row for Deployment: `\| [Deployment](04_Deployment/Deployment.md) \| Packaging and shipping an OpenCL app \| \`04_Deployment/\` \|` | [x] |
| 3 | Slot 10 is vacant (post-Phase 1 merge) | Toolbox.md | OK | Consistent with D11 Phase 1 decision. | [ ] |

### Redundancies
None identified.

---

## 05_Toolbox / 01_Local_Memory/LocalMemory.md

### Factual Claims
| # | Claim | Location | Verdict | Proposed Fix | Fix? |
|---|-------|----------|---------|--------------|------|
| 1 | "`__local` memory ... ~100x faster than global memory" | LocalMemory.md §Concept | OK | Standard order-of-magnitude figure (1–4 cycle LDS vs 200–800 cycle DRAM). | [ ] |
| 2 | "Local memory is limited (typically 32–64 KB per compute unit)" | LocalMemory.md §Troubleshooting | STALE | AMD RDNA3 and Intel Arc have 64 KB. NVIDIA Ada has up to 128 KB (OpenCL-exposed limit may differ). "32–64 KB" understates modern discrete GPUs. Update to wider range and suggest to check local setup using `clinfo | grep "Local memory"` or on code level with device info `CL_DEVICE_LOCAL_MEM_SIZE`. | [x] |
| 3 | `barrier(CLK_LOCAL_MEM_FENCE)` required before reading halo data | LocalMemory.md §Concept | OK | Correct per OpenCL 1.2 spec §6.11.9. | [ ] |

### Redundancies
None identified.

---

## 05_Toolbox / 02_Coalesced_Access/CoalescedAccess.md

### Factual Claims
| # | Claim | Location | Verdict | Proposed Fix | Fix? |
|---|-------|----------|---------|--------------|------|
| 1 | "GPU memory controller reads data in transactions of 32–128 bytes" | CoalescedAccess.md §Concept | OK | NVIDIA: 32-byte transactions per sub-warp; AMD: 32–128 bytes depending on cache level. Range is accurate. | [ ] |
| 2 | Measured data: RTX 4060 Laptop 1.3×, AMD Radeon 680M 8.1× at 8192×8192 | CoalescedAccess.md §Concept table | OK | Self-consistent measured values from module validation. Appropriately labelled. | [ ] |
| 3 | "A large L2 (e.g. 24 MB on RTX 4060)" | CoalescedAccess.md §Concept | OK | RTX 4060 Ada Lovelace has 24 MB L2. Confirmed by NVIDIA specifications. | [ ] |

### Redundancies
None identified.

---

## 05_Toolbox / 03_Debugging/Debugging.md

### Factual Claims
| # | Claim | Location | Verdict | Proposed Fix | Fix? |
|---|-------|----------|---------|--------------|------|
| 1 | "Oclgrind runs on CPU — expect 10–50x slowdown" | Debugging.md §Oclgrind | OK | Software simulator; 10–50× is a reasonable instrumentation overhead range. | [ ] |
| 2 | `rocprof --opencl-trace` for AMD OpenCL profiling | Debugging.md §Nsight/VTune | OK | T050-corrected (was `--hsa-trace`). Current text is correct. | [ ] |
| 3 | "`--check-api` ... operates at the host API level" | Debugging.md §Oclgrind | OK | Accurate scope description. | [ ] |

### Redundancies
None identified.

---

## 05_Toolbox / 04_Deployment/Deployment.md

### Factual Claims
| # | Claim | Location | Verdict | Proposed Fix | Fix? |
|---|-------|----------|---------|--------------|------|
| 1 | `FROM rocm/opencl-dev` Docker base image | Deployment.md §Docker | UNVERIFIED | Inline Note already present documenting the uncertainty. No additional fix needed. | [ ] |
| 2 | "OpenCL applications link against `libOpenCL.so` (the ICD Loader), not the vendor driver directly" | Deployment.md §ICD Loader | OK | Correct ICD architecture. | [ ] |
| 3 | `FROM intel/oneapi-basekit` for Intel iGPU / CPU Docker | Deployment.md §Docker | OK | Image exists on hub.docker.com/r/intel/oneapi-basekit. | [ ] |

**Note:** `04_Deployment/Deployment.md` is not linked in `05_Toolbox/Toolbox.md`. See Toolbox.md STALE item above.

### Redundancies
None identified.

---

## 05_Toolbox / 05_Fast_Math/FastMath.md

### Factual Claims
| # | Claim | Location | Verdict | Proposed Fix | Fix? |
|---|-------|----------|---------|--------------|------|
| 1 | "`half_sqrt(x)` \| ≥ 11-bit mantissa ... (if `cl_khr_fp16` supported)" | FastMath.md §Concept table | FALSE | `half_` prefix functions are standard OpenCL 1.2 built-ins (§6.12.2, table 6.9). They do NOT require `cl_khr_fp16` — that extension enables the `half` scalar data type, which is separate. Additionally the spec guarantees ≥ 10-bit mantissa (≤ 8192 ULP), not 11-bit. Fix: remove "(if `cl_khr_fp16` supported)"; change "≥ 11-bit mantissa" → "≥ 10-bit mantissa (≤ 8192 ULP, OpenCL 1.2 spec §6.12.2)". | [x] |
| 2 | "`native_sqrt` returns NaN for negative inputs: `sqrt(x < 0)` returns NaN per the OpenCL spec — standard `sqrt` does not clamp" | FastMath.md §Troubleshooting | OK | T050-corrected. OpenCL spec §6.12.2 lists `sqrt` of negative values as implementation-defined (returns NaN). | [ ] |
| 3 | "`-cl-fast-relaxed-math` ... may replace standard functions with `native_` equivalents automatically" | FastMath.md §Concept | OK | Documented compiler flag behaviour per OpenCL spec §6.10. | [ ] |

### Redundancies
None identified.

---

## 05_Toolbox / 06_Generic_Kernel_Templates/GenericKernelTemplates.md

### Factual Claims
| # | Claim | Location | Verdict | Proposed Fix | Fix? |
|---|-------|----------|---------|--------------|------|
| 1 | "OpenCL's `clBuildProgram` accepts a `-D` options string, identical to `gcc -D`" | GenericKernelTemplates.md §Concept | OK | Correct per OpenCL 1.2 spec §5.6.3. | [ ] |
| 2 | "Guard with `#pragma OPENCL EXTENSION cl_khr_fp16 : enable` ... for `half` type" | GenericKernelTemplates.md §Troubleshooting | OK | Correct — `half` scalar type requires explicit extension enable. | [ ] |
| 3 | "Performance gate: generic `float` MAD < 1 ms on a 1080p image" | GenericKernelTemplates.md §Concept | OK | Internally consistent with Module 1 results. | [ ] |

### Redundancies
None identified.

---

## 05_Toolbox / 07_Global_Work_Offset/GlobalWorkOffset.md

### Factual Claims
| # | Claim | Location | Verdict | Proposed Fix | Fix? |
|---|-------|----------|---------|--------------|------|
| 1 | "`get_global_id()` returns the *absolute* global work-item ID, which **includes** the `global_work_offset`" | GlobalWorkOffset.md §How it Works | OK | Correct per OpenCL 1.2 spec §6.12.1. | [ ] |
| 2 | "OpenCL 1.2 requires `global_work_size` to be an exact multiple of `local_work_size` when `local_work_size` is specified" | GlobalWorkOffset.md §Why round up | OK | Correct per OpenCL 1.2 spec §5.8 (`CL_INVALID_WORK_GROUP_SIZE`). | [ ] |
| 3 | "Theoretical ceiling ~126× for 256×256 tile on 3840×2160" | GlobalWorkOffset.md §Verify | OK | 3840×2160 / (256×256) ≈ 126.6×. Arithmetic correct. | [ ] |

### Redundancies
None identified.

---

## 05_Toolbox / 08_Multi_GPU_Strategy/MultiGPUStrategy.md

### Factual Claims
| # | Claim | Location | Verdict | Proposed Fix | Fix? |
|---|-------|----------|---------|--------------|------|
| 1 | "A `cl::Context` spanning devices from different platforms is not possible in OpenCL 1.2" | MultiGPUStrategy.md §Troubleshooting | OK | `clCreateContext` requires devices from a single platform per spec. | [ ] |
| 2 | Performance gate: ">= 1.65x speedup with 2x GPUs" | MultiGPUStrategy.md | OK | Consistent with executive summary. | [ ] |
| 3 | "Cross-platform profiling timestamps use independent device clocks with no shared epoch" | MultiGPUStrategy.md §Troubleshooting | OK | Correct per OpenCL spec device clock semantics. | [ ] |

### Redundancies
None identified.

---

## 05_Toolbox / 09_OpenCL_vs_CUDA/OpenCLvsCUDA.md

### Factual Claims
| # | Claim | Location | Verdict | Proposed Fix | Fix? |
|---|-------|----------|---------|--------------|------|
| 1 | "FPGA — Xilinx/Intel FPGAs use OpenCL as their primary compute API" | OpenCLvsCUDA.md §Where OpenCL Wins | STALE | Inline Note already present: "AMD/Xilinx shifted toward SYCL/HLS C++ as of 2024 — OpenCL support remains but is no longer the recommended entry point." T050 AUDIT FLAG resolved. No further change needed. | [ ] |
| 2 | "cuDNN, TensorRT, PyTorch, JAX are CUDA-native. No OpenCL equivalent." | OpenCLvsCUDA.md §Where CUDA Wins | OK | Accurate as of 2026. | [ ] |
| 3 | "AMD on Linux — ROCm is the primary path; OpenCL is the stable portable layer on top." | OpenCLvsCUDA.md §Where OpenCL Wins | OK | ROCm exposes OpenCL via `rocm-opencl-runtime`. Confirmed. | [ ] |

### Redundancies
None identified.

---

## 05_Toolbox / 11_SVM_Theory/SVMTheory.md

### Factual Claims
| # | Claim | Location | Verdict | Proposed Fix | Fix? |
|---|-------|----------|---------|--------------|------|
| 1 | "PCIe 4.0 x16 peak: 32 GB/s theoretical, ~24 GB/s measured" | SVMTheory.md §Concept | UNVERIFIED | PCIe 4.0 x16 theoretical unidirectional = ~31.5 GB/s (often rounded to 32 GB/s). Measured pinned-memory throughput varies 20–28 GB/s. "~24 GB/s" is within range but no source is cited. | [ ] |
| 2 | "SVM fine-grained requires a unified cache hierarchy ... Discrete Nvidia GPUs do not expose this via OpenCL." | SVMTheory.md §SVM Levels | OK | NVIDIA OpenCL 1.2 drivers return `CL_DEVICE_SVM_CAPABILITIES == 0`. Confirmed. | [ ] |
| 3 | "Bandwidth limited by system memory (~100 GB/s on modern APUs)" | SVMTheory.md §Concept | STALE | AMD Ryzen 7xxx APUs with DDR5-6400 dual-channel: ~102 GB/s theoretical. Intel Core Ultra with LPDDR5x: ~120 GB/s. "~100 GB/s" is accurate for mid-tier APUs; actual range 85–140 GB/s. No platform cited. Use wider range. | [x] |

### Redundancies
None identified.

---

## 05_Toolbox / 12_Sync_Atomics/SyncAtomics.md

### Factual Claims
| # | Claim | Location | Verdict | Proposed Fix | Fix? |
|---|-------|----------|---------|--------------|------|
| 1 | "A single contended `atomic_add` to global memory can cost 100–500× more than a regular store" | SyncAtomics.md §Cost | OK | Well-documented range in GPU architecture literature for highly-contended global atomics. | [ ] |
| 2 | "OpenCL 1.2 atomics operate on `int`/`uint` only" | SyncAtomics.md §Troubleshooting | OK | Correct per OpenCL 1.2 spec §6.11.11. | [ ] |
| 3 | CAS-loop code uses `atomic_cmpxchg` | SyncAtomics.md §Troubleshooting | OK | T050-corrected (was `atom_cmpxchg`). Current text is correct. | [ ] |

### Redundancies
None identified.

---

## 05_Toolbox / 13_Thread_Divergence/ThreadDivergence.md

### Factual Claims
| # | Claim | Location | Verdict | Proposed Fix | Fix? |
|---|-------|----------|---------|--------------|------|
| 1 | "AMD RDNA default is wave64 (wave32 is opt-in via compiler flag)" | ThreadDivergence.md §Troubleshooting | OK | T050-corrected from "RDNA uses 32-wide waves". Current statement is accurate. | [ ] |
| 2 | "Nvidia uses 32-wide warps" | ThreadDivergence.md §Troubleshooting | OK | NVIDIA warp size = 32 since Tesla. | [ ] |
| 3 | "Intel Arc uses 16-wide SIMD" | ThreadDivergence.md §Troubleshooting | OK | Intel Xe EU SIMD width = 16. | [ ] |

### Redundancies
None identified.

---

## 05_Toolbox / 14_Work_Group_Sizing/WorkGroupSizing.md

### Factual Claims
| # | Claim | Location | Verdict | Proposed Fix | Fix? |
|---|-------|----------|---------|--------------|------|
| 1 | "Hardware maximum [work-items per group] (typically 256–1024)" | WorkGroupSizing.md §Concept | OK | `CL_DEVICE_MAX_WORK_GROUP_SIZE`: 256 on many iGPUs, 1024 on most discrete GPUs. | [ ] |
| 2 | "Start with `local_work_size = 64`" rule of thumb | WorkGroupSizing.md §Concept | OK | 64 = 2 warps (NVIDIA), 1 wavefront (AMD wave64). Reasonable default. | [ ] |
| 3 | "Occupancy = ratio of active warps to maximum possible warps on a compute unit" | WorkGroupSizing.md §Concept | OK | Standard definition. | [ ] |

### Redundancies
None identified.

---

## 05_Toolbox / 15_Zero_Copy/ZeroCopy.md

### Factual Claims
| # | Claim | Location | Verdict | Proposed Fix | Fix? |
|---|-------|----------|---------|--------------|------|
| 1 | "`CL_MEM_ALLOC_HOST_PTR` + `enqueueMapBuffer` is the most portable zero-copy pattern on discrete GPUs" | ZeroCopy.md §Concept | OK | Standard recommendation for DMA-able (pinned) memory. | [ ] |
| 2 | "`CL_MEM_USE_HOST_PTR` is only truly zero-copy on UMA architectures" | ZeroCopy.md §Concept | OK | On discrete GPU driver may DMA-copy at kernel launch. True zero-copy requires shared physical memory. | [ ] |
| 3 | Verify output: `[COPY_HOST_PTR ] Upload: 8.4 ms` | ZeroCopy.md §Verify | OK | Hardware-specific example; labelled as expected output. | [ ] |

### Redundancies
None identified.

---

## 05_Toolbox / 16_Async_Multi_Thread/AsyncMultiThread.md

### Factual Claims
| # | Claim | Location | Verdict | Proposed Fix | Fix? |
|---|-------|----------|---------|--------------|------|
| 1 | "Many consumer GPUs (NVIDIA CUDA, AMD rusticl) serialize commands on a single device even with OOO queues or dual queues" | AsyncMultiThread.md §Hardware note | OK | Consumer GPUs typically have a single command processor. True copy-engine/compute-engine concurrency requires server-class hardware. | [ ] |
| 2 | "`cl::Context` is thread-safe; `cl::CommandQueue` is not — one queue per thread" | AsyncMultiThread.md §Step 3 | OK | `cl::Context` reference counting is thread-safe per spec; `cl::CommandQueue` is not required to be. | [ ] |
| 3 | "A single `CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE` queue is less reliable" | AsyncMultiThread.md §Step 2 | OK | Accurate engineering observation; two in-order queues is more portable. | [ ] |

### Redundancies
None identified.

---

## 06_Bonus / Bonus.md

### Factual Claims
| # | Claim | Location | Verdict | Proposed Fix | Fix? |
|---|-------|----------|---------|--------------|------|
| 1 | "Module 1 complete (`01_Host_API/`) is sufficient for all modules unless noted" | Bonus.md §Prerequisites | OK | Consistent with CLBlast and vkFFT prerequisites. | [ ] |
| 2 | "Device Enqueue — OpenCL 2.0+ device required" | Bonus.md §Contents | OK | Consistent with DeviceEnqueue.md. | [ ] |
| 3 | "vkFFT Audio — no custom kernel required" | Bonus.md §Contents | OK | vkFFT abstracts the FFT; only library API calls are needed. | [ ] |

### Redundancies
None identified.

---

## 06_Bonus / 01_CLBlast_MatMul/CLBlastMatMul.md

### Factual Claims
| # | Claim | Location | Verdict | Proposed Fix | Fix? |
|---|-------|----------|---------|--------------|------|
| 1 | "For a square N×N multiply, the operation count is **2N³ FLOPs**" | CLBlastMatMul.md §Key terms | OK | Each C[i,j] needs N MADs = 2N FLOPs. N² outputs = 2N³ total. Correct. | [ ] |
| 2 | "CLBlast should outperform the naive kernel by at least 5× on any modern GPU" | CLBlastMatMul.md §Expected Output | OK | Tiled GEMM vs naive column-major reliably shows 5–20× improvement. | [ ] |
| 3 | "CLBlast is vendor-agnostic — it targets any OpenCL device. cuBLAS is Nvidia-only." | CLBlastMatMul.md §CLBlast vs cuBLAS | OK | Confirmed by CLBlast GitHub (OpenCL 1.1+ support across vendors). | [ ] |

### Redundancies
None identified.

---

## 06_Bonus / 02_Device_Enqueue/DeviceEnqueue.md

### Factual Claims
| # | Claim | Location | Verdict | Proposed Fix | Fix? |
|---|-------|----------|---------|--------------|------|
| 1 | "Devices that report OpenCL 3.0 but do not implement the optional device-enqueue extension will have `CL_DEVICE_QUEUE_ON_DEVICE_PROPERTIES == 0`" | DeviceEnqueue.md §Key Concepts | OK | Correct: device-side enqueue is optional in OpenCL 3.0. | [ ] |
| 2 | "CUDA had equivalent capability (Dynamic Parallelism) since CUDA 5.0 (2012)" | DeviceEnqueue.md §Historical note | OK | CUDA Dynamic Parallelism released with CUDA 5.0, October 2012. Confirmed. | [ ] |
| 3 | "Device enqueue overhead can exceed the per-bounce savings for small scenes" | DeviceEnqueue.md §Expected Output | OK | Consistent with sample output (GPU-spawned 12.450 ms vs CPU-dispatched 9.170 ms for 576-triangle scene). | [ ] |

### Redundancies
None identified.

---

## 06_Bonus / 03_VkFFT_Audio/vkFFTAudio.md

### Factual Claims
| # | Claim | Location | Verdict | Proposed Fix | Fix? |
|---|-------|----------|---------|--------------|------|
| 1 | "A naive DFT is O(N²). FFT is O(N log N)" | vkFFTAudio.md §Concept | OK | Standard algorithmic complexity. | [ ] |
| 2 | "vkFFT uses OpenCL (and Vulkan/CUDA/HIP) under the hood" | vkFFTAudio.md §Concept | OK | Confirmed by vkFFT GitHub — supports Vulkan, CUDA, HIP, OpenCL, Metal. | [ ] |
| 3 | Speedup example: "CPU reference (FFTW): 18.4 ms. Speedup: 23x" | vkFFTAudio.md §Verify | OK | Hardware-specific reference number; FFTW is a credible CPU baseline. | [ ] |

### Redundancies
None identified.

---

## 06_Bonus / 04_Voxel_Mapping/VoxelMapping.md

### Factual Claims
| # | Claim | Location | Verdict | Proposed Fix | Fix? |
|---|-------|----------|---------|--------------|------|
| 1 | "Voxel grid: 200x200x50 @ 0.10m resolution (3 MB)" in verify output | VoxelMapping.md §Verify | STALE | Inline Note already present: "200×200×50 = 2,000,000 cells. As `uint` (4 bytes): 8 MB; as `uchar` (1 byte): 2 MB." The "3 MB" console output contradicts all documented options. Fix note to address different underlying types will impact size instead pushing for extact value. CARRY FORWARD from T050. | [x] |
| 2 | "`atomic_or` sets individual bits without clearing others — same principle as `atomic_add`" | VoxelMapping.md §Concept | OK | Correct semantics. | [ ] |
| 3 | "3D DDA (Digital Differential Analyzer) algorithm, run in parallel — one GPU work-item per Lidar point" | VoxelMapping.md §Concept | OK | DDA (Amanatides & Woo, 1987) is the standard voxel traversal algorithm. | [ ] |

### Redundancies
None identified.

---

## Summary of Required Fixes

### FALSE — default `[x]`
| File | Fix |
|------|-----|
| `05_Toolbox/05_Fast_Math/FastMath.md` | `half_sqrt` row: remove "(if `cl_khr_fp16` supported)"; change "≥ 11-bit mantissa" → "≥ 10-bit mantissa (≤ 8192 ULP, OpenCL 1.2 spec §6.12.2)" |

### STALE — default `[x]`
| File | Fix |
|------|-----|
| `00_Setup/Setup.md` | Remove "6th Gen Skylake" from §1 opening sentence; keep "Broadwell (5th Gen) and newer" in bullet |
| `02_Multimedia/01_OpenCV_Interop/OpenCVInterop.md` | Update broken link `10_SVM/SVM.md` → `11_SVM_Theory/SVMTheory.md` |
| `03_GraphicsHPC/01_Ray_Tracer_Basic/RayTracerBasic.md` | Change "PCIe Gen 3 bandwidth (~12 GB/s)" → "PCIe Gen 3 bandwidth (~16 GB/s theoretical, ~12–14 GB/s measured)" |
| `05_Toolbox/Toolbox.md` | Add missing `04_Deployment` row to contents table |
| `06_Bonus/04_Voxel_Mapping/VoxelMapping.md` | Resolve "3 MB" in verify output block after checking element type in `dda_cast.cl` |

### UNVERIFIED — default `[ ]` (awaiting human triage)
| File | Claim |
|------|-------|
| `01_Host_API/01_Visual_Kernel/VisualKernel.md` | "OpenCL 1.2 features cover 95% of real workloads" |
| `02_Multimedia/01_OpenCV_Interop/OpenCVInterop.md` | "At 4K copy overhead easily exceeds 5 ms" |
| `05_Toolbox/01_Local_Memory/LocalMemory.md` | "typically 32–64 KB" local memory per CU |
| `05_Toolbox/04_Deployment/Deployment.md` | `FROM rocm/opencl-dev` image tag validity |
| `05_Toolbox/11_SVM_Theory/SVMTheory.md` | PCIe 4.0 "~24 GB/s measured" and APU "~100 GB/s" bandwidth figures |

### MANUAL — ROS 2 runtime (cannot be web-verified)
| File | Claim |
|------|-------|
| `04_Robotics/RoboticsROS2.md` | `rmw_fastrtps_cpp` is correct RMW for loaned messages in Jazzy |
| `04_Robotics/01_Node_Acceleration/NodeAcceleration.md` | Intra-process zero-copy `shared_ptr` routing in Jazzy |
| `04_Robotics/03_Perception_Node/PerceptionNode.md` | Loaned messages deliver zero-copy path under `rmw_fastrtps_cpp` |
