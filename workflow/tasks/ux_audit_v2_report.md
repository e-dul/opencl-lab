# UX Audit v2 Report

## 00_Setup

### Pedagogical Gaps
| # | Issue | Location | Severity | Proposed Fix | Fix? |
|---|-------|----------|----------|--------------|------|
| 1 | No explanation distinguishing `ocl-icd-libopencl1` (runtime loader) from `ocl-icd-opencl-dev` (compile-time dev files) — both appear in AMD section without explanation | `00_Setup/Setup.md` §AMD Mesa | MED | Add one-line note per package clarifying runtime vs. compile-time role | [x] |
| 2 | Docker section opens with "coming soon" caveat but immediately shows working verification commands — contradictory framing; students may skip useful content | `00_Setup/Setup.md` §2 Docker | MED | Remove "coming soon" note or move section to a clearly-labelled optional stub | |
| 3 | `opencl-headers` vs `ocl-icd-opencl-dev` header overlap not explained — students installing both may wonder why | `00_Setup/Setup.md` §4 Build Tools | LOW | Add note: `ocl-icd-opencl-dev` is a superset; `opencl-headers` = Khronos headers only | [x] |
| 4 | No explanation of what happens if `smoke_test` is run from outside `build/` — relative kernel path will silently fail | `00_Setup/01_Smoke_Test/SmokeTest.md` §How to Run | MED | Add: "Running from a parent directory causes 'cannot open kernel file' — must run from inside `build/`" . Also more general information about kernels handling in this repo. | [x] |

### UX Friction Points
| # | Issue | Location | Severity | Proposed Fix | Fix? |
|---|-------|----------|----------|--------------|------|
| 1 | `Setup.md` has no back-link to root `README.md` | `00_Setup/Setup.md` bottom | LOW | Add `[Back to README](../README.md)` |[x]|
| 2 | `SmokeTest.md` build commands use `mkdir build / cd build / cmake ..` (old style) — inconsistent with all other modules which use `cmake -B build && cmake --build build` | `00_Setup/01_Smoke_Test/SmokeTest.md` §How to Run | MED | Replace with `cmake -B build && cmake --build build && ./build/smoke_test` |[x]|
| 3 | `SmokeTest.md` troubleshooting says `"cl.hpp not found"` → path is `/usr/include/CL/opencl.hpp` — confusing mismatch between `cl.hpp` and `opencl.hpp` names without explanation | `00_Setup/01_Smoke_Test/SmokeTest.md` §Troubleshooting | MED | Clarify: `opencl.hpp` = modern bindings; `cl.hpp` = legacy 1.2 bindings used by this course; both may coexist | [x] |

---

## 01_Host_API

### Pedagogical Gaps
| # | Issue | Location | Severity | Proposed Fix | Fix? |
|---|-------|----------|----------|--------------|------|
| 1 | No explanation that the binary generates its own synthetic input image — students may search for a pre-existing asset to pass | `01_Host_API/01_Visual_Kernel/VisualKernel.md` §Verify | MED | Add: "The binary generates a synthetic gradient input automatically — no external image file is needed" | [x] |
| 2 | `VisualKernelEvents.md` prerequisite delta does not mention `CL_QUEUE_PROFILING_ENABLE` as new concept — student who skims may miss why profiling requires queue recreation | `01_Host_API/02_Visual_Kernel_Events/VisualKernelEvents.md` §Prerequisites | LOW | Add delta note: "Queue must be created with `CL_QUEUE_PROFILING_ENABLE` (new vs 1.1)" | [x] |
| 3 | `BufferFlags.md` cross-links to Toolbox Zero-Copy before student has seen any Toolbox content — forward reference without "optional" labelling | `01_Host_API/03_Buffer_Flags/BufferFlags.md` §Key Concepts | LOW | Mark the Zero-Copy toolbox link as `(optional deeper reading)` | |

### UX Friction Points
| # | Issue | Location | Severity | Proposed Fix | Fix? |
|---|-------|----------|----------|--------------|------|
| 1 | `HostAPI.md` §Prerequisites links to "main README" (`../README.md`) — link text is ambiguous; "main README" could mean the module index or the root README | `01_Host_API/HostAPI.md` §Prerequisites | LOW | Change to "root README (`../README.md`)" | [x] |
| 2 | `HostAPI.md` "Performance Gate" section contains a correctness check (three non-zero timings), not a performance target — section title is misleading | `01_Host_API/HostAPI.md` §Performance Gate | LOW | Rename to "Verification Gate" or add expected timing ranges |[x] |

---

## 02_Multimedia

### Pedagogical Gaps
| # | Issue | Location | Severity | Proposed Fix | Fix? |
|---|-------|----------|----------|--------------|------|
| 1 | `Multimedia.md` does not warn that submodules 05 and 06 are Intel iGPU-only — students on NVIDIA/AMD may attempt and hit silent exits | `02_Multimedia/Multimedia.md` §Prerequisites | HIGH | Add warning box: "05 and 06 require Intel iGPU; they exit cleanly on other hardware — this is expected" | |
| 2 | `YUVPipeline.md` jumps directly into NV12 layout and BT.601 coefficients with no introduction to why cameras output YUV or what colour space standards are | `02_Multimedia/02_YUV_Pipeline/YUVPipeline.md` §Key Concepts | MED | Add 2-sentence intro: why YUV (bandwidth savings), what BT.601 is (standard-def TV colour space) | [x] |
| 3 | `OpenCVDNN.md` uses `mask_umat.handle(cv::ACCESS_READ)` without explaining this is semi-private OpenCV internal API | `02_Multimedia/04_OpenCV_DNN/OpenCVDNN.md` §Key Concepts | MED | Add: "`handle()` is semi-private OpenCV API for OpenCL interop; `ACCESS_READ` signals read-only access" | [x] |
| 4 | `FFmpegPipeline.md` introduces VA-API, VAAPI, NVDEC, and extension function loading with no prerequisites section — largest cognitive jump in the module | `02_Multimedia/08_FFmpeg_Pipeline/FFmpegPipeline.md` | HIGH | Add Prerequisites section requiring FFmpeg familiarity and `02_YUV_Pipeline` as prior reading | |
| 5 | `SoftISP.md` requires Toolbox: Local Memory as a prerequisite — creates a forward-dependency loop for top-to-bottom readers | `02_Multimedia/09_SoftISP/SoftISP.md` §Prerequisites | MED | Add: "Read Local Memory toolbox first (15–20 min) if you have not already; return here after." | |

### UX Friction Points
| # | Issue | Location | Severity | Proposed Fix | Fix? |
|---|-------|----------|----------|--------------|------|
| 1 | `OpenCVInterop.md` Build & Run uses `cd opencv_interop` — stale path; actual directory is `01_OpenCV_Interop` (T060 rename) | `02_Multimedia/01_OpenCV_Interop/OpenCVInterop.md` §Build & Run | HIGH | Replace `cd opencv_interop` with `cd 01_OpenCV_Interop` | [x] |
| 2 | `YUVPipeline.md` Build & Run uses `cd yuv_pipeline` — stale path; actual directory is `02_YUV_Pipeline` | `02_Multimedia/02_YUV_Pipeline/YUVPipeline.md` §Build & Run | HIGH | Replace `cd yuv_pipeline` with `cd 02_YUV_Pipeline` | [x] |
| 3 | `YUYVExtension.md` Build & Run uses `cd yuyv_extension` — stale path; actual directory is `03_YUYV_Extension` | `02_Multimedia/03_YUYV_Extension/YUYVExtension.md` §Build & Run | HIGH | Replace `cd yuyv_extension` with `cd 03_YUYV_Extension` | [x] |
| 4 | `YUVPipeline.md` example uses `--input assets/sample_nv12.yuv --width 1920 --height 1080` but `sample_nv12.yuv` is 256×256 per `assets/assets.md` — dimension mismatch will corrupt output | `02_Multimedia/02_YUV_Pipeline/YUVPipeline.md` §Build & Run | HIGH | Change example to `sample_nv12_1080p.yuv` for the performance gate | [x] |
| 5 | `YUYVExtension.md` example uses `--input assets/sample_yuyv.yuv --width 1920 --height 1080` — same dimension mismatch; `sample_yuyv.yuv` is 256×256 | `02_Multimedia/03_YUYV_Extension/YUYVExtension.md` §Build & Run | HIGH | Change to  `sample_yuyv_1080p.yuv` | [x] |
| 6 | `PrivacyMode.md` title is "A.5" but the submodule occupies slot 07; index says "07 — Privacy Mode" | `02_Multimedia/07_Privacy_Mode/PrivacyMode.md` | MED | Change title to "A.7 — Privacy Mode" | [x] |
| 7 | `FFmpegPipeline.md` title is "A.5" but occupies slot 08 | `02_Multimedia/08_FFmpeg_Pipeline/FFmpegPipeline.md` | MED | Change title to "A.8 — FFmpeg Pipeline" | [x] |
| 8 | `SoftISP.md` title is "A.6" but occupies slot 09 | `02_Multimedia/09_SoftISP/SoftISP.md` | MED | Change title to "A.9 — SoftISP" | [x] |
| 9 | `SETUP.md` (OpenVINO) §4 run example uses `--input assets/sample_1080p.bmp`; `OpenVINOGPU.md` uses `--input assets/face.png` — same binary, different example inputs | `02_Multimedia/05_OpenVINO_GPU/SETUP.md` §4 | LOW | Align SETUP.md example to match `OpenVINOGPU.md`: `--input assets/face.png --model assets/selfie_segmentation.onnx` | |
| 10 | `SmartWebcam.md` `--loop` flag used without documentation of what it does | `02_Multimedia/06_Smart_Webcam/SmartWebcam.md` §Build & Run | LOW | Add: "`--loop` replays the static input image in a loop to simulate live capture without a webcam" | [x] |

---

## 03_GraphicsHPC

### Pedagogical Gaps
| # | Issue | Location | Severity | Proposed Fix | Fix? |
|---|-------|----------|----------|--------------|------|
| 1 | SAH (Surface Area Heuristic) introduced with no prior scaffolding — large jump from the Basic ray tracer | `03_GraphicsHPC/02_Ray_Tracer_BVH/RayTracerBVH.md` §BVH Build on CPU | MED | Add pointer to PBRT §4.3 or similar reference before the SAH explanation |[x] |
| 2 | `--max-depth 0` meaning "unlimited" is not documented — unintuitive default value | `03_GraphicsHPC/03_Ray_Tracer_BVH_Dynamic/RayTracerBVHDynamic.md` §`--max-depth` | LOW | Add: "Default `--max-depth 0` = unlimited depth (yields ~2N nodes for N triangles)" | [x] |

### UX Friction Points
| # | Issue | Location | Severity | Proposed Fix | Fix? |
|---|-------|----------|----------|--------------|------|
| 1 | `RayTracerBasic.md` H1 title is "B.2 — Ray Tracer Basic" but occupies slot 01; index calls it "01 — Ray Tracer Basic" | `03_GraphicsHPC/01_Ray_Tracer_Basic/RayTracerBasic.md` | MED | Change title to "B.1 — Ray Tracer Basic" | [x] |
| 2 | `RayTracerBVH.md` H1 title is "B.3 — Ray Tracer BVH" but occupies slot 02 | `03_GraphicsHPC/02_Ray_Tracer_BVH/RayTracerBVH.md` | MED | Change title to "B.2 — Ray Tracer BVH" | [x] |
| 3 | After fixing B.1/B.2 numbering, verify "B.3 Dynamic" in `RayTracerBVHDynamic.md` remains consistent | `03_GraphicsHPC/03_Ray_Tracer_BVH_Dynamic/RayTracerBVHDynamic.md` | LOW | Verify chain after B.1/B.2 fixes | [x] |
| 4 | `GraphicsHPC.md` tinyobjloader offline note `"-DCMAKE_PREFIX_PATH=..."` may be mistaken for a shell env var by junior students | `03_GraphicsHPC/GraphicsHPC.md` §Prerequisites | LOW | Clarify: "pass to `cmake -B build`, not as a shell variable" | [x] |

---

## 04_Robotics

### Pedagogical Gaps
| # | Issue | Location | Severity | Proposed Fix | Fix? |
|---|-------|----------|----------|--------------|------|
| 1 | `RoboticsROS2.md` assumes ROS 2 knowledge without providing a link for students who lack it | `04_Robotics/RoboticsROS2.md` §Prerequisites | MED | Add link to official ROS 2 beginner tutorials (~2–3 hours) | [x] |
| 2 | `NodeAcceleration.md` lifecycle tutorial link is buried in Prerequisites — easy to miss before encountering the code example | `04_Robotics/01_Node_Acceleration/NodeAcceleration.md` §Prerequisites | MED | Move or duplicate the lifecycle tutorial link to appear before the first code snippet | [x] |
| 3 | `CostmapInflation.md` Build & Run uses relative `assets/` path without noting the binary must be run from repo root | `04_Robotics/02_Costmap_Inflation/CostmapInflation.md` §Build & Run | MED | Add: "Run from the repo root, or use absolute path: `-p map_path:=$(pwd)/assets/warehouse.pgm`" |  |
| 4 | `PerceptionNode.md` RViz setup instructions lack the `ros2 run rviz2 rviz2` launch command | `04_Robotics/03_Perception_Node/PerceptionNode.md` §Challenge | MED | Add `ros2 run rviz2 rviz2` before the display config instructions | [x] |

### UX Friction Points
| # | Issue | Location | Severity | Proposed Fix | Fix? |
|---|-------|----------|----------|--------------|------|
| 1 | `SETUP.md` §4 Verify uses `ros2 run demo_nodes_cpp talker` but `ros-jazzy-demo-nodes-cpp` is not in the §1 install list | `04_Robotics/SETUP.md` §4 Verify | MED | Add `ros-jazzy-demo-nodes-cpp` to install list. And add verify command with `ros2 doctor`.  | [x] |
| 2 | MANUAL: `NodeAcceleration.md` `ros2 param` inspection requires a second terminal — not stated | `04_Robotics/01_Node_Acceleration/NodeAcceleration.md` §Inspecting Parameters | MED | Add: "In a second terminal (with ROS 2 sourced), while the node is running:" before the `ros2 param` block | [x] |
| 3 | MANUAL: `PerceptionNode.md` per-terminal run blocks do not repeat `source` and `export RMW_IMPLEMENTATION` commands despite requiring them | `04_Robotics/03_Perception_Node/PerceptionNode.md` §Build & Run | MED | Repeat `source /opt/ros/jazzy/setup.bash` and `export RMW_IMPLEMENTATION=rmw_fastrtps_cpp` inside each terminal command block | MANUAL: |
| 4 | `PerceptionNode.md` Mini-Challenge links `[Toolbox: Memory Coalescing](../../05_Toolbox/Toolbox.md)` to the Toolbox index, not the specific page | `04_Robotics/03_Perception_Node/PerceptionNode.md` §Mini-Challenge | LOW | Change to `../../05_Toolbox/02_Coalesced_Access/CoalescedAccess.md` | [x] |

---

## 05_Toolbox

### Pedagogical Gaps
| # | Issue | Location | Severity | Proposed Fix | Fix? |
|---|-------|----------|----------|--------------|------|
| 1 | `LocalMemory.md` `Used In` entry "Module 4 SoftISP debayering" has no hyperlink — inconsistent with all other `Used In` entries | `05_Toolbox/01_Local_Memory/LocalMemory.md` §Used In | LOW | Replace with `[Track A — 09_SoftISP](../../02_Multimedia/09_SoftISP/SoftISP.md)` | [x] |
| 2 | `SVMTheory.md` and `OpenCLvsCUDA.md` H1 titles use old "4.x" numbering scheme — inconsistent with rest of Toolbox | `05_Toolbox/11_SVM_Theory/SVMTheory.md`, `05_Toolbox/09_OpenCL_vs_CUDA/OpenCLvsCUDA.md` | LOW | Remove "4.x —" prefix from both titles | [x] |
| 3 | `Toolbox.md` How to Use §1 references `02_Projects/` — this path does not exist | `05_Toolbox/Toolbox.md` §How to Use | MED | Replace `02_Projects/` with "your track module (`02_Multimedia/`, `03_GraphicsHPC/`, or `04_Robotics/`)" | [x] |
| 4 | `AsyncMultiThread.md` does not clearly set expectations that "No overlap detected" is a normal result on most consumer GPUs before the student runs the demo | `05_Toolbox/16_Async_Multi_Thread/AsyncMultiThread.md` §Verify | MED | Add before the Verify section: "Note: 'No overlap detected' is the typical result on NVIDIA CUDA and AMD rusticl — this is expected behavior, not a code defect." | [x] |

### UX Friction Points
| # | Issue | Location | Severity | Proposed Fix | Fix? |
|---|-------|----------|----------|--------------|------|
| 1 | All Toolbox `cd` commands use `cd 05_Toolbox/01_Local_Memory` style (repo-root-relative) but no Toolbox README states this precondition | Every Toolbox submodule §Build & Run | MED | Add to `Toolbox.md`: "All `cd` paths assume the repo root as working directory (`Applied-OpenCL-Lab/`)" | |
| 2 | Root `README.md` line 101 lists `[10_SVM](05_Toolbox/10_SVM/)` — this directory does not exist (vacant slot post-T067). Broken link | `README.md` §Toolbox table | HIGH | Remove the `10_SVM` row from root README toolbox table | [x] |
| 3 | Root `README.md` slot-10 (`10_SVM`) and `Toolbox.md` (no slot-10 entry, has `11_SVM_Theory`) are inconsistent — students see SVM listed in one index but not the other | `README.md` vs `05_Toolbox/Toolbox.md` | HIGH | Align both: remove slot-10 row from root README; slot 11 SVM Theory remains | [x] |
| 4 | `GenericKernelTemplates.md` builds three binaries from one CMake project — not communicated to student before the build step | `05_Toolbox/06_Generic_Kernel_Templates/GenericKernelTemplates.md` §Build & Run | LOW | Add: "This module builds three separate binaries from one CMake project." | [x] |
| 5 | `SyncAtomics.md` float CAS code block uses C strict-aliasing-violating pointer cast `*(int*)&old_val` without noting this is OpenCL C (not C++) | `05_Toolbox/12_Sync_Atomics/SyncAtomics.md` §Troubleshooting | LOW | Add code comment: `// OpenCL C only — strict aliasing rules differ from C++` | |

---

## 06_Bonus

### Pedagogical Gaps
| # | Issue | Location | Severity | Proposed Fix | Fix? |
|---|-------|----------|----------|--------------|------|
| 1 | `CLBlastMatMul.md` GEMM FLOP formula (2N³) not explained — students without linear algebra background may not understand the accounting | `06_Bonus/01_CLBlast_MatMul/CLBlastMatMul.md` §Key Terms callout | LOW | Add: "(each output element needs N multiply-add pairs = 2 FLOPs each → 2N³ total)" | [x] |
| 2 | `DeviceEnqueue.md` expected output shows GPU path *slower* than CPU — the explanation note appears after the table; students will see the confusing result first | `06_Bonus/02_Device_Enqueue/DeviceEnqueue.md` §Expected Output | MED | Move the "Device enqueue overhead can exceed..." note to appear before the timing table | [x] |
| 3 | `VoxelMapping.md` Verify section contains a self-correction TODO note visible to students: "Note: The '3 MB' figure is storage-type-dependent and undocumented" | `06_Bonus/04_Voxel_Mapping/VoxelMapping.md` §Verify | MED | Resolve the actual element type, update the size figure, remove the author note | |
| 4 | `VoxelMapping.md` uses `atomic_or` without introducing it — no prior module or Toolbox entry covers bitwise atomics | `06_Bonus/04_Voxel_Mapping/VoxelMapping.md` §Concept | MED | Add: "`atomic_or` sets individual bits without clearing others — same principle as `atomic_add` from Toolbox 12, applied to bitfield flags" | [x] |

### UX Friction Points
| # | Issue | Location | Severity | Proposed Fix | Fix? |
|---|-------|----------|----------|--------------|------|
| 1 | `vkFFTAudio.md` Build & Run uses `--input assets/sample.wav` — this file does not exist in the repo; actual files are `assets/test_440hz.wav` and `assets/test_1024frames.wav` | `06_Bonus/03_VkFFT_Audio/vkFFTAudio.md` §Build & Run | HIGH | Replace `assets/sample.wav` with `assets/test_440hz.wav` in all run examples | [x] |
| 2 | `Bonus.md` has no "What's Next" navigation section — all other module indexes have one | `06_Bonus/Bonus.md` | LOW | Add `## What's Next` with return links to tracks or a "lab complete" note | |
| 3 | `CLBlastMatMul.md` back-link and navigation — audited (T062). No UX friction issues found. | `06_Bonus/01_CLBlast_MatMul/CLBlastMatMul.md` | LOW | No action needed | |
| 4 | `DeviceEnqueue.md` back-link and navigation — audited (T062). No UX friction issues found. | `06_Bonus/02_Device_Enqueue/DeviceEnqueue.md` | LOW | No action needed | |
| 5 | `VoxelMapping.md` dynamic scene example backgrounds the publisher with `&` but provides no instruction to stop the background process after Ctrl-C | `06_Bonus/04_Voxel_Mapping/VoxelMapping.md` §Build & Run | LOW | Add: "To stop the backgrounded publisher: `kill %1` or `fg` then Ctrl-C" | |
| 6 | MANUAL: `VoxelMapping.md` "Live Visualisation (RViz)" section lacks a `MANUAL:` prefix in the heading | `06_Bonus/04_Voxel_Mapping/VoxelMapping.md` §Live Visualisation | LOW | Add `MANUAL:` prefix to the section heading | MANUAL: |
