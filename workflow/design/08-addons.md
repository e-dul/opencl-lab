# Module 4: Add-ons (Bonus Case Studies)

**Version:** 1.2
**Status:** Active — Phase 4 complete
**Module Path:** `04_Addons/`

---

## Goal

Provide self-contained, elective case studies for engineers who have completed at least one Module 2 specialization track. Each add-on is a standalone project demonstrating one production-grade integration problem — FFT libraries, video transcoding, sensor processing, deployment, or cross-API comparison — without repeating foundational OpenCL instruction. The module is non-linear: independently buildable and ordered by prerequisite depth, not sequence.

---

## Non-goals

- Foundational OpenCL teaching (Module 1)
- Core kernel optimization theory (Toolbox / `99_Toolbox/`)
- Module 2 flagship project logic (A4 Smart Webcam, B3 BVH Ray Tracer, C3 Perception Node)
- Full path tracing or global illumination
- ROS 2 Nav2 planner/controller internals
- Real-time audio synthesis (offline spectrogram analysis only)
- API comparison benchmarking beyond the executive summary definition (4.2 is analysis, not a benchmark suite)

---

## Roadmap / Status

- [x] Phase 1: 4.1 vkFFT Audio — GPU FFT spectrogram via vkFFT; FFTW CPU reference; speedup gate.
  - *Context*: Executive Summary §4.1; `04_Addons/4_1_vkFFT_Audio/vkFFTAudio.md`
- [x] Phase 2: 4.2 OpenCL vs CUDA — Written analysis artifact (structured `report.md` + code comparison); no binary.
  - *Context*: Executive Summary §4.2; `04_Addons/4_2_OpenCL_vs_CUDA/OpenCLvsCUDA.md`
- [x] Phase 3: 4.3 Deployment — CMake install rules, AppImage script, Docker multi-stage build; verified by clean `docker run`.
  - *Context*: Executive Summary §4.3; `04_Addons/4_3_Deployment/Deployment.md`
- [x] Phase 4: 4.4 SVM Deep Dive — Standalone benchmark: `CL_MEM_COPY_HOST_PTR` vs `USE_HOST_PTR` vs SVM coarse-grained vs SVM fine-grained; BMP artifact from image round-trip.
  - *Context*: Executive Summary §4.4; `04_Addons/4_4_SVM_Theory/SVMTheory.md`
- [x] Phase 5: 4.5 Voxel Mapping — DDA ray casting over voxel grid from LiDAR point cloud; reuses B3 ray-AABB math; subscribes to live `sensor_msgs/PointCloud2` topic (bags played via `ros2 bag play`).
  - *Context*: Executive Summary §4.5; `04_Addons/4_5_Voxel_Mapping/VoxelMapping.md`
- [ ] Phase 6: 4.6 FFmpeg Pipeline — Hardware decode (NVDEC/VAAPI) → zero-copy OpenCL surface map → filter kernel from Track A → re-encode; per-frame breakdown.
  - *Context*: Executive Summary §4.6; `04_Addons/4_6_FFmpeg_Pipeline/FFmpegPipeline.md`
- [ ] Phase 7: 4.7 SoftISP — Naive bilinear debayer (V1) vs LDS-tiled debayer (V2) at 4K; pixel-identical BMP outputs; speedup gate.
  - *Context*: Executive Summary §4.7; `04_Addons/4_7_SoftISP/SoftISP.md`
- [ ] Phase 8: Module review and cleanup — Verify all standalone builds, align CMake conventions with Module 1/2 patterns, confirm asset references resolve.
  - [ ] Realign `04_Addons/4_2_OpenCL_vs_CUDA/OpenCLvsCUDA.md` README: remove reference to `portability_demo` binary (4.2 is report + code samples only; no binary by design — see Key Decision #3).

---

## Specifications

> **Inherits**: `.claude/rules/00_master_specs.md`

**Additional constraints:**

- **Standalone Build**: Every add-on must build with `cmake -B build && cmake --build build` from within its directory. No dependency on other add-ons.
- **Non-linear Ordering**: No compile-time dependency between add-ons. Shared logic via `common/` only.
- **4.2 Exception**: Written analysis only — no binary. Artifact is `report.md` in `4_2_OpenCL_vs_CUDA/`. No performance gate.
- **4.3 Exception**: Produces packaging artifacts (Dockerfile, CMake install rules, AppImage script). Verification: successful `docker build` + `docker run` of the `01_VisualKernel` demo.
- **4.5 ROS 2 Dependency**: Hard ROS 2 Jazzy dependency. CMake must check `$ENV{ROS_DISTRO}` and emit `message(FATAL_ERROR)` with instructions if unset. All other add-ons build without ROS 2.
- **4.6 Hardware Interop**: `clCreateFromVA_APIMediaSurfaceINTEL` (Intel/AMD) and `cl_khr_egl_image` (Nvidia) checked at runtime. Software-decode fallback (`AVFrame` CPU → `clEnqueueWriteBuffer`) required when hardware interop unavailable.
- **4.7 LDS Pixel-Identical Check**: V1 and V2 outputs must be byte-exact. Any difference terminates run with `std::runtime_error`. Check is in-binary, not a test script.
- **vkFFT Fetching**: Via `FetchContent_Declare` in `4_1_vkFFT_Audio/CMakeLists.txt`. No system install required.
- **FFTW CPU Reference (4.1)**: `find_package(FFTW3)` optional — if not found, CPU reference path is skipped with `[CPU reference skipped: FFTW3 not found]` message. Speedup gate requires FFTW3.
- **CLI**: All binaries use CLI11 via `common/common.cmake`. No hand-rolled arg parsing.
- **Assets**: All add-ons read from `assets/` at repo root via CLI args. No hardcoded paths.

---

## Architecture (high-level)

### Components

- **4.1 vkFFT Spectrogram** (`4_1_vkFFT_Audio/`): Reads `.wav`, segments into overlapping frames, runs batch 1D FFT via vkFFT, computes per-bin magnitude, writes frequency-vs-time heatmap BMP. FFTW CPU reference for same batch. CLI: `--input`, `--fft-size`, `--hop-size`.

- **4.2 OpenCL vs CUDA Analysis** (`4_2_OpenCL_vs_CUDA/`): No binary. Structured `report.md`: ecosystem comparison table, use-case decision matrix, side-by-side code (`vector_add.cu` / `vector_add.cl`), pragmatic recommendation.

- **4.3 Deployment Package** (`4_3_Deployment/`): CMake `install()` rules, multi-stage `Dockerfile` (build → runtime with ICD loader + PoCL), `appimage.sh`. Wraps `01_VisualKernel` as the packaged application.

- **4.4 SVM Benchmark** (`4_4_SVM_Theory/`): Four memory transfer paths for 1080p image round-trip: `CL_MEM_COPY_HOST_PTR`, `CL_MEM_USE_HOST_PTR`, SVM coarse-grained, SVM fine-grained (guarded `#ifdef CL_VERSION_2_0`). Reports ms per path; writes `output_svm_verify.bmp`. CLI: `--width`, `--height`, `--iterations`.

- **4.5 Voxel Mapping** (`4_5_Voxel_Mapping/`): Two binaries. `voxel_mapping`: subscribes to `--topic` (`sensor_msgs/PointCloud2`), accumulates voxel grid across frames, writes `output_voxel_slice.bmp` on shutdown. Publishes two live debug topics each frame: `/voxel_map` (`sensor_msgs/PointCloud2` of occupied voxel XYZ centroids in world frame) and `/voxel_slice` (`sensor_msgs/Image`, MONO8, above-sensor column projection). CLI: `--topic`, `--resolution`, `--output`, `--enable-flip-filter`, `--flip-threshold`. `voxel_point_cloud_publisher`: synthetic publisher with `--scene static|dynamic` (dynamic scene orbits sphere clusters per frame to exercise flip-count filter). Bags played via `ros2 bag play`. No `rosbag2_cpp` dependency.

- **4.6 FFmpeg Transcoder** (`4_6_FFmpeg_Pipeline/`): Reads `.mp4`, hardware-decodes to GPU surface (VAAPI/EGL), maps to OpenCL image (zero-copy), applies blur/filter kernel from Track A, re-encodes. Per-frame stage breakdown. CLI: `--input`, `--output`, `--effect` (`blur`|`sepia`).

- **4.7 SoftISP Demo** (`4_7_SoftISP/`): Reads raw RGGB Bayer `.raw`, runs V1 (naive bilinear) and V2 (LDS-tiled) debayer kernels, writes both BMPs, asserts pixel identity, reports speedup. CLI: `--input`, `--width`, `--height`.

### Data Flow

#### 4.1 vkFFT Spectrogram
1. Read `.wav` → PCM float buffer (host).
2. Segment into frames → `cl::Buffer` (interleaved complex, zero-padded to FFT size).
3. `VkFFTAppend` (forward, batch) → magnitude buffer → `cl::Event` timing.
4. FFTW path: same frames, CPU `std::chrono` timing.
5. Map magnitude → RGBA heatmap → BMP via `stb_image_write`.
6. Print: GPU FFT ms, CPU FFT ms, speedup.

#### 4.4 SVM Benchmark
1. Allocate host image (RGBA, 1920×1080).
2. For each path: allocate/map → enqueue copy-to-device → passthrough kernel → copy-to-host → `cl::Event` time.
3. SVM 2.0 path: guarded `#ifdef CL_VERSION_2_0`; skipped with message if `CL_DEVICE_SVM_CAPABILITIES` bitmask has no SVM bits set.
4. Correctness check: readback == original.
5. Write `output_svm_verify.bmp`. Print comparison table.

#### 4.5 Voxel Mapping (per frame)
1. Subscriber callback receives `PointCloud2::SharedPtr`.
2. Upload points → `cl::Buffer` (non-blocking) → `cl::Event`.
3. DDA kernel: one work-item per point, marks FREE voxels along ray and OCCUPIED at endpoint via `atomic_or` → `cl::Event`.
4. (Optional) Flip-count kernel: classify dynamic voxels → `cl::Event`. Host-side: zero occupancy where `flip_count > threshold`.
5. Read back grid buffer (synchronous, for debug publishers).
6. Build `/voxel_map` cloud: iterate grid, collect voxels with `OCCUPIED_BIT`, convert indices → world XYZ centroids, publish `sensor_msgs/PointCloud2`.
7. Build `/voxel_slice` image: above-sensor column projection (z ≥ gz/2), colorize (OCCUPIED=black(0), FREE=white(255), UNKNOWN=grey(128)), publish `sensor_msgs/Image` (MONO8, width=gx, height=gy).
8. Assert total GPU pipeline (upload + DDA [+ flip]) < 5 ms. Print `[WARN]` if exceeded.
9. On shutdown: run same projection → write `output_voxel_slice.bmp`.

#### 4.6 FFmpeg Transcoder (per frame)
1. `avcodec_receive_frame` → hardware `AVFrame`.
2. Map VAAPI/EGL surface → `cl_mem` (zero-copy). `clEnqueueAcquire*` → `cl::Event`.
3. Dispatch filter kernel (same `.cl` as Track A). → `cl::Event`.
4. `clEnqueueRelease*` → re-encode via hardware encoder.
5. Print: decode ms, map ms, filter ms, encode ms, total ms, effective FPS.

#### 4.7 SoftISP (V1 then V2)
1. Read `.raw` → `cl::Buffer` (uchar, RGGB).
2. V1 dispatch: `debayer_naive` → `cl::Event`.
3. V2 dispatch: `debayer_lds` → `cl::Event`.
4. Read both outputs. Assert byte-exact.
5. Write `output_rgb_v1.bmp`, `output_rgb_v2.bmp`.
6. Print: V1 ms, V2 ms, speedup, effective FPS.

---

## Key Decisions (and Rationale)

1. **Non-linear Structure**
   - **Why**: Users arrive from different Module 2 tracks. Enforcing sequential order would block e.g. an embedded engineer (Track A) from accessing 4.7 SoftISP while waiting on 4.5 Voxel Mapping prerequisites.

2. **vkFFT via FetchContent, Not System Install**
   - **Why**: Consistent with project-wide standalone-buildable rule. No version mismatch across machines.

3. **4.2 Is a Written Report, Not a Benchmark Binary**
   - **Why**: An honest OpenCL vs CUDA comparison is an ecosystem and use-case analysis, not a micro-benchmark. Numbers from a single GPU mislead more than inform.

4. **4.3 Wraps Module 1, Not Module 2 Projects**
   - **Why**: `01_VisualKernel` has the simplest dependency graph. Packaging a complex project conflates deployment complexity with application complexity.

5. **4.4 Four Memory Paths in One Binary**
   - **Why**: Single binary with identical input/output eliminates measurement noise from process startup and OS scheduling. The comparison table is the artifact.

6. **4.6 Hardware Interop with Mandatory Software Fallback**
   - **Why**: `cl_intel_va_api_media_sharing` is Intel/AMD-only; EGL path covers Nvidia. Software fallback ensures testability in CI without hardware decoder.

7. **4.7 Pixel-Identical Assertion In-Binary**
   - **Why**: Visual comparison is insufficient — human vision cannot detect single-pixel halo bugs. In-binary byte-exact diff is the DoD.

8. **4.5 Reuses B3 Ray-AABB Math Unchanged**
   - **Why**: Educational payoff is demonstrating knowledge transfer: a graphics technique (ray-box intersection from BVH) solves a robotics problem (LiDAR ray casting in voxel space).

---

## Known Issues / Risks

- **vkFFT OpenCL Backend Maturity**: Lags behind Vulkan backend. Some radix configurations may fail on non-Nvidia drivers. Must catch `VkFFTResult != VKFFT_SUCCESS` with human-readable error including the failing FFT configuration.
- **VAAPI/EGL Surface Mapping Vendor Lock**: `clCreateFromVA_APIMediaSurfaceINTEL` is Intel/AMD-only. Software fallback must be tested on all three GPU vendors. Extension string check must be per-platform.
- **SVM Fine-Grained Availability**: Not supported on NVIDIA OpenCL drivers. Must detect `CL_DEVICE_SVM_CAPABILITIES` at runtime and skip inaccessible paths with a clear message — not a crash.
- **4.4 SVM Gating via `CL_DEVICE_OPENCL_C_VERSION` is Wrong**: `CL_DEVICE_OPENCL_C_VERSION` returns "OpenCL C 1.2" on Intel OpenCL 3.0 drivers even when SVM is fully supported. Gate must use `CL_DEVICE_SVM_CAPABILITIES` bitmask directly. Fixed in implementation.
- **4.4 Intel Iris Xe UMA — All Paths Equal**: On Intel Iris Xe (iGPU, UMA), `COPY_HOST_PTR`, `USE_HOST_PTR`, and SVM coarse-grained all report ~0.48 ms. The driver zero-copies all three paths over shared memory. The ≤ 50% performance gate is waived for UMA configurations. SVM fine-grained system is not supported on this device.
- **4.5 Atomic Contention at High Point Density**: DDA traversal with `atomic_or` on shared voxel grid serializes under high point density. Document in console output and README. Mitigation (per-thread staging buffer + merge kernel) is the challenge, not base implementation.
- **4.5 flip_count_buf_ Requires Per-Cycle Reset**: After a flip-count threshold crossing zeroes a voxel's occupancy, `flip_count_buf_` must be reset to 0 for that voxel; otherwise dynamic voxels are permanently filtered even when they become static. Reset is implemented in the host-side post-processing pass each frame.
- **4.5 Sensor Pose is Static**: No odometry integration. All frames accumulate in sensor frame. Correct only when the sensor does not move. Printed as `[INFO]` on first message; documented in README.
- **4.5 Above-Sensor Column Projection Excludes Ground Level**: `/voxel_slice` and `output_voxel_slice.bmp` project z from `gz/2 + 1` (not `gz/2`) to `gz - 1`, excluding the ground-level voxel layer to reduce ground-return noise in the 2D footprint.
- **4.7 LDS Tile Halo Boundary**: Work-items at image edges must clamp indices to `[0, width-1]` × `[0, height-1]`. Incorrect clamping is the most common bug; pixel-identity assertion catches it.
- **4.3 Docker PoCL vs Native Driver**: Dockerfile must install `ocl-icd-libopencl1` and at minimum PoCL. `GPU` env var selection must still work inside the container. *(Resolved: GPU passthrough via `--device /dev/dri` + host ICD/library mounts works; PoCL CPU fallback confirmed without GPU.)*
- **4.3 AppImage Kernel Path**: Kernel files must be copied to `AppDir/usr/bin/kernels/` (next to binary) — not only `usr/share/`. The binary resolves kernels via `std::filesystem::read_symlink("/proc/self/exe").parent_path() / "kernels/"` to work inside the squashfs mount path.
- **4.3 libOpenCL exclusion**: `libOpenCL.so.1` must be excluded from AppImage (`--exclude-library libOpenCL.so.1`). Bundling it bypasses the host ICD loader and silently falls back to CPU.
- **Asset Availability**: `sample.wav`, `raw_bayer_4k.raw`, `sample.mp4` are large binary files. CMake emits `message(WARNING)` if missing (not a build error). Binary fails gracefully at runtime with clear error. 4.5 has no required asset — `voxel_point_cloud_publisher` provides synthetic data; real bags are user-supplied and played via `ros2 bag play`.
- **4.1 NVIDIA Barrier-Event Timing (0 ms)**: The NVIDIA OpenCL driver collapses back-to-back `clEnqueueBarrierWithWaitList` calls bracketing vkFFT enqueue to the same timestamp, reporting 0.000 ms GPU FFT batch time. FFT executes correctly (non-uniform BMP produced). The `< 2 ms` and `≥ 10× speedup` gates cannot be confirmed via `cl::Event` profiling on this hardware — both gates are waived for NVIDIA drivers using this approach.
- **4.1 AMD iGPU Speedup ≈ 1×**: On AMD Radeon 680M (rusticl, iGPU), GPU and CPU share memory bandwidth. GPU FFT batch for 1051 frames = 2.510 ms vs CPU FFTW 2.578 ms (≈ 1× speedup). The `< 2 ms` gate passes for the 169-frame batch (0.547 ms); the 1051-frame batch marginally exceeds it — hardware waiver applies. CL event timing returns correct non-zero values on AMD, confirming the 0 ms issue is NVIDIA-driver-specific.

---

## Performance Gates (Module Completion)

| Add-on | Metric | Target |
| :--- | :--- | :--- |
| 4.1 vkFFT Spectrogram | GPU FFT batch (1024 frames × 2048 bins) | < 2 ms |
| 4.1 vkFFT vs FFTW | Speedup | ≥ 10× (reported in console) |
| 4.4 SVM Benchmark | `USE_HOST_PTR` vs `COPY_HOST_PTR` | Reported; USE_HOST_PTR expected ≤ 50% of COPY time on iGPU. **UMA waiver**: on Intel Iris Xe (shared memory), all three paths (COPY, USE, SVM coarse) converge to ~0.48 ms — driver zero-copies all paths; ≤ 50% gap does not apply. |
| 4.5 Voxel Mapping | End-to-end pipeline per frame | < 5 ms @ 100k points |
| 4.6 FFmpeg Transcoder | decode + map + filter + encode per frame | < 10 ms @ 1080p (≥ 100 FPS) |
| 4.7 SoftISP V2 LDS | Debayer 4K RGGB → RGBA | < 10 ms (≥ 100 FPS @ 3840×2160) |
| 4.7 V2 vs V1 | Speedup | ≥ 3× reported in console |

*4.2 and 4.3 have no numeric performance gate.*

*Hardware waiver: gates measured via `cl::Event` profiling only. Report actual hardware results; waive if physically impossible on single-GPU devices.*

---

## Specifications & Standards

- **Directory Structure**:
  ```
  04_Addons/
  ├── Addons.md
  ├── 4_1_vkFFT_Audio/
  │   ├── CMakeLists.txt
  │   └── main.cpp
  ├── 4_2_OpenCL_vs_CUDA/
  │   ├── report.md
  │   └── code_comparison/
  │       ├── vector_add.cu
  │       └── vector_add.cl
  ├── 4_3_Deployment/
  │   ├── CMakeLists.txt
  │   ├── Dockerfile
  │   └── appimage.sh
  ├── 4_4_SVM_Theory/
  │   ├── CMakeLists.txt
  │   ├── main.cpp
  │   └── kernels/passthrough.cl
  ├── 4_5_Voxel_Mapping/
  │   ├── CMakeLists.txt
  │   ├── main.cpp                      (binary: voxel_mapping)
  │   ├── point_cloud_publisher.cpp     (binary: voxel_point_cloud_publisher)
  │   └── kernels/
  │       ├── dda_cast.cl
  │       └── flip_count.cl
  ├── 4_6_FFmpeg_Pipeline/
  │   ├── CMakeLists.txt
  │   ├── main.cpp
  │   └── kernels/filter.cl
  └── 4_7_SoftISP/
      ├── CMakeLists.txt
      ├── main.cpp
      └── kernels/
          ├── debayer_naive.cl
          └── debayer_lds.cl
  ```

- **Verification Standard**:
  - 4.1: `output_spectrogram.bmp` (non-uniform color heatmap). Console: GPU ms, CPU ms, speedup.
  - 4.2: `report.md` present and non-empty. No binary.
  - 4.3: `docker build` succeeds; `docker run` produces `output.bmp`.
  - 4.4: `output_svm_verify.bmp` matches input. Console: time table for all available paths.
  - 4.5: `output_voxel_slice.bmp` (occupied black, free white, unknown grey). Console: per-stage ms < 5 ms total. Live RViz: `/voxel_map` cloud + `/voxel_slice` image visible each frame.
  - 4.6: `filtered.mp4` plays correctly with effect visible. Console: per-frame breakdown.
  - 4.7: `output_rgb_v1.bmp` and `output_rgb_v2.bmp` pixel-identical and visually correct. Console: V1 ms, V2 ms, speedup.

- **Tooling**:
  - vkFFT: `FetchContent_Declare` in 4.1 only.
  - FFmpeg: `find_package(PkgConfig REQUIRED)` → `pkg_check_modules(FFMPEG REQUIRED libavcodec libavformat libavutil)` in 4.6.
  - ROS 2: `find_package(rclcpp REQUIRED)` + `find_package(sensor_msgs REQUIRED)` + `$ENV{ROS_DISTRO}` guard (via `opencl_lab_ros2_guard()`) in 4.5 only. No `rosbag2_cpp` — bags played via `ros2 bag play`.
  - FFTW3: `find_package(FFTW3)` (optional) in 4.1.
  - CLI11: via `common/common.cmake` in all binaries.

- **OpenCL 2.0+ Gating**: SVM fine-grained (4.4) wrapped in `#ifdef CL_VERSION_2_0`. Runtime `CL_DEVICE_SVM_CAPABILITIES` bitmask check before any SVM allocation. **Do NOT gate on `CL_DEVICE_OPENCL_C_VERSION` string** — Intel OpenCL 3.0 drivers report "OpenCL C 1.2" for this query even when SVM is supported; the bitmask is the authoritative check.

---

## Prerequisites

- Module 1 completed: `cl.hpp`, `cl::Event` profiling, `CL_CHECK` error handling.
- Per add-on:
  - 4.1: Any Module 2 track.
  - 4.2: Any Module 2 track.
  - 4.3: Module 1 only.
  - 4.4: Any Module 2 track.
  - 4.5: Track B (B3 complete) + Track C (C3 complete) + ROS 2 Jazzy.
  - 4.6: Track A (A4 complete) + `libavcodec-dev libavformat-dev libavutil-dev`.
  - 4.7: Toolbox `LocalMemory` reviewed. No external library dependencies.
- Assets: `assets/sample.wav` (4.1), `assets/raw_bayer_4k.raw` (4.7), `assets/sample.mp4` (4.6). 4.5 has no required asset — use `voxel_point_cloud_publisher` for live testing or `ros2 bag play <bag>` for bag replay.

See [main README](../README.md) for base requirements (OpenCL 1.2+, CMake 3.18+, Docker 20.10+).
