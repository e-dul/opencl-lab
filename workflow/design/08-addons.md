# Module 8: Add-ons (Bonus Case Studies)

**Version:** 1.0
**Status:** Active — implementation not started
**Module Path:** `04_Addons/`

---

## Goal

Provide self-contained, elective case studies for engineers who have completed at least one Module 2 specialization track. Each add-on is a standalone project that demonstrates one production-grade integration problem — FFT libraries, video transcoding, sensor processing, deployment, or cross-API comparison — without repeating foundational OpenCL instruction. The module is non-linear: each add-on is independently buildable and ordered by prerequisite depth, not by sequence.

---

## Non-goals

- Foundational OpenCL teaching (covered in Module 1)
- Core kernel optimization theory (covered in Toolbox / `99_Toolbox/`)
- Any Module 2 flagship project logic (A4 Smart Webcam, B3 BVH Ray Tracer, C3 Perception Node)
- Full path tracing or global illumination
- ROS 2 Nav2 planner/controller internals
- Real-time audio synthesis (only offline spectrogram analysis)
- API comparison benchmarking beyond what the executive summary defines (4.2 is analysis, not a benchmark suite)

---

## Roadmap / Status

- [ ] Phase 1: 4.1 vkFFT Audio — GPU FFT spectrogram via vkFFT; FFTW CPU reference; speedup gate.
  - *Context*: Executive Summary §4.1; `04_Addons/4_1_vkFFT_Audio/vkFFTAudio.md`.
- [ ] Phase 2: 4.2 OpenCL vs CUDA — Written analysis artifact (structured markdown report + code comparison); no binary required.
  - *Context*: Executive Summary §4.2; `04_Addons/4_2_OpenCL_vs_CUDA/OpenCLvsCUDA.md`.
- [ ] Phase 3: 4.3 Deployment — Packaging demo: CMake install rules, AppImage script, Docker multi-stage build; verifiable by a clean Docker run.
  - *Context*: Executive Summary §4.3; `04_Addons/4_3_Deployment/Deployment.md`.
- [ ] Phase 4: 4.4 SVM Deep Dive — Standalone benchmark: `CL_MEM_USE_HOST_PTR` vs `CL_MEM_ALLOC_HOST_PTR` vs SVM coarse-grained vs SVM fine-grained; BMP artifact from an image round-trip through each path.
  - *Context*: Executive Summary §4.4; `04_Addons/4_4_SVM_Theory/SVMTheory.md`.
- [ ] Phase 5: 4.5 Voxel Mapping — Grand Finale: DDA ray casting over voxel grid from Lidar point cloud; reuses B3 ray-AABB intersection math; ROS 2 bag input.
  - *Context*: Executive Summary §4.5; `04_Addons/4_5_Voxel_Mapping/VoxelMapping.md`.
- [ ] Phase 6: 4.6 FFmpeg Pipeline — Hardware decode (NVDEC/VAAPI) → zero-copy OpenCL surface map → filter kernel from Track A → re-encode; per-frame breakdown console output.
  - *Context*: Executive Summary §4.6; `04_Addons/4_6_FFmpeg_Pipeline/FFmpegPipeline.md`.
- [ ] Phase 7: 4.7 SoftISP — Naive bilinear debayer (V1) vs LDS-tiled debayer (V2) at 4K; pixel-identical BMP outputs; speedup gate.
  - *Context*: Executive Summary §4.7; `04_Addons/4_7_SoftISP/SoftISP.md`.
- [ ] Phase 8: Module review and cleanup — Verify all standalone builds, align CMake conventions with Module 1/2 patterns, confirm asset references resolve.

---

## Specifications

> **Inherits**: `.claude/rules/00_master_specs.md`

**Additional constraints for this module:**

- **Standalone Build Requirement**: Every add-on (`4_1_vkFFT_Audio/`, `4_7_SoftISP/`, etc.) must build and produce its verification artifact with no dependency on other add-ons or Module 2 projects. `cmake -B build && cmake --build build` from the add-on directory is the only required command.
- **Non-linear Ordering**: No add-on may introduce a compile-time dependency on another add-on. Shared logic must come from `common/` only.
- **4.2 Exception**: The OpenCL vs CUDA add-on is a written analysis — no binary is required. The artifact is a structured markdown file (`report.md`) co-located in `4_2_OpenCL_vs_CUDA/`. No performance gate applies.
- **4.3 Exception**: The Deployment add-on produces packaging artifacts (Dockerfile, CMake install rules, AppImage script) rather than a GPU binary. Verification is a successful `docker build` and `docker run` that executes the Module 1 `01_VisualKernel` demo inside the container.
- **4.5 ROS 2 Dependency**: `4_5_Voxel_Mapping` has a hard ROS 2 Humble+ dependency. CMake must check `$ENV{ROS_DISTRO}` and emit `message(FATAL_ERROR)` with instructions if unset. All other add-ons must build without ROS 2.
- **4.6 Hardware Interop Extension**: `clCreateFromVA_APIMediaSurfaceINTEL` (Intel/AMD VAAPI path) and `cl_khr_egl_image` (Nvidia EGL path) must be checked at runtime via `clGetPlatformInfo` / extension string query. A software-decode fallback (`AVFrame` CPU → `clEnqueueWriteBuffer`) must be available when hardware interop is unavailable, ensuring the binary runs on all platforms.
- **4.7 LDS Pixel-Identical Check**: V1 (naive) and V2 (LDS-tiled) outputs must be pixel-identical. Any difference terminates the run with `std::runtime_error`. This check is part of the binary, not a test script.
- **vkFFT Fetching**: Fetched via `FetchContent_Declare` in `4_1_vkFFT_Audio/CMakeLists.txt`. Must not require a system install. Offline build note: `-DCMAKE_PREFIX_PATH=/path/to/vkfft`.
- **FFTW CPU Reference (4.1)**: `find_package(FFTW3 REQUIRED)` gated: if FFTW3 is not found, the CPU reference path is skipped and a `[CPU reference skipped: FFTW3 not found]` message is printed. The GPU path and speedup gate require FFTW3.
- **CLI**: All binaries use CLI11 via `common/common.cmake`. See per-add-on CLI specs in the Architecture section.
- **Assets**: All add-ons read from `assets/` at repository root. Paths passed via CLI; no hardcoded asset paths in code.

---

## Architecture (high-level)

### Components

- **4.1 vkFFT Spectrogram** (`4_1_vkFFT_Audio/`): Reads a `.wav` file, segments into overlapping frames, runs a batch 1D FFT via vkFFT, computes per-bin magnitude, writes a frequency-vs-time heatmap BMP. FFTW CPU reference for the same batch. CLI: `--input`, `--fft-size`, `--hop-size`.

- **4.2 OpenCL vs CUDA Analysis** (`4_2_OpenCL_vs_CUDA/`): No binary. A structured `report.md` covering: ecosystem comparison table, use-case decision matrix, code side-by-side (same kernel in CUDA C and OpenCL C), and a pragmatic recommendation. Artifact: the markdown file itself.

- **4.3 Deployment Package** (`4_3_Deployment/`): CMake `install()` rules targeting `${CMAKE_INSTALL_PREFIX}`. A `Dockerfile` (multi-stage: build image → runtime image with ICD loader). An `appimage.sh` packaging script. Wraps the `01_VisualKernel` binary as the packaged application. CLI inherited from `01_VisualKernel`.

- **4.4 SVM Benchmark** (`4_4_SVM_Theory/`): Four memory transfer paths for a 1080p image round-trip (host → device → host): `CL_MEM_COPY_HOST_PTR`, `CL_MEM_USE_HOST_PTR`, SVM coarse-grained (`clSVMAlloc`), SVM fine-grained (guarded by `#ifdef CL_VERSION_2_0`). Reports time per path in ms; writes `output_svm_verify.bmp` from the final path to confirm correctness. CLI: `--width`, `--height`, `--iterations`.

- **4.5 Voxel Mapping** (`4_5_Voxel_Mapping/`): Subscribes to `/points` (`sensor_msgs/PointCloud2`). Per-frame GPU pipeline: upload point cloud → DDA ray casting kernel (one work-item per point, marks FREE voxels along ray and OCCUPIED at endpoint using `atomic_or`) → optional dynamic filter (flip-count buffer) → download 2D slice → write `output_voxel_slice.bmp`. CLI: `--bag`, `--topic`, `--resolution`, `--output`.

- **4.6 FFmpeg Transcoder** (`4_6_FFmpeg_Pipeline/`): Reads an `.mp4`, hardware-decodes to GPU surface (VAAPI/EGL), maps surface to OpenCL image (zero-copy), applies the same blur/filter kernel from Track A, re-encodes. Reports per-frame stage breakdown. CLI: `--input`, `--output`, `--effect` (`bokeh` | `sepia`).

- **4.7 SoftISP Demo** (`4_7_SoftISP/`): Reads a raw RGGB Bayer `.raw` file, runs V1 (naive bilinear, global memory) and V2 (LDS-tiled bilinear) debayer kernels, writes both outputs as `output_rgb_v1.bmp` and `output_rgb_v2.bmp`, asserts pixel identity, reports speedup. CLI: `--input`, `--width`, `--height`.

### Data Flow

#### 4.1 vkFFT Spectrogram
1. Read `.wav` → PCM float buffer (host).
2. Segment into frames → `cl::Buffer` (interleaved complex, zero-padded to FFT size).
3. `VkFFTAppend` (forward, batch) → magnitude buffer → `cl::Event` timing.
4. FFTW path: same frames, same FFT size, CPU → `std::chrono` timing.
5. Map magnitude buffer to RGBA heatmap → `stb_image_write` BMP.
6. Print: GPU FFT time ms, CPU FFT time ms, speedup.

#### 4.4 SVM Benchmark
1. Allocate host image (RGBA, 1920×1080).
2. For each path: allocate / map buffer → enqueue copy-to-device → passthrough kernel → copy-to-host → `cl::Event` time.
3. SVM 2.0 path: guarded by `#ifdef CL_VERSION_2_0`; skipped with message if OpenCL < 2.0.
4. Correctness check: readback == original input.
5. Write `output_svm_verify.bmp`. Print comparison table.

#### 4.5 Voxel Mapping (per frame)
1. Subscriber callback receives `PointCloud2::SharedPtr`.
2. Upload points → `cl::Buffer` (`CL_FALSE` non-blocking) → `cl::Event`.
3. DDA kernel: `global_work_size = num_points`. Each work-item walks its ray through the voxel grid, marking cells with `atomic_or`. → `cl::Event`.
4. (Optional challenge) Flip-count kernel: classify dynamic voxels. → `cl::Event`.
5. Extract 2D top-down slice from voxel grid → colorize RGBA (black/white/grey) → `stb_image_write` BMP.
6. Log per-stage times; assert total < 5 ms.

#### 4.6 FFmpeg Transcoder (per frame)
1. `avcodec_receive_frame` → hardware `AVFrame`.
2. Map VAAPI/EGL surface → `cl_mem` (zero-copy). `clEnqueueAcquire*` → `cl::Event`.
3. Dispatch filter kernel (same `.cl` source as Track A). → `cl::Event`.
4. `clEnqueueRelease*` → re-encode frame via hardware encoder.
5. Print per-frame: decode ms, map ms, filter ms, encode ms, total ms, effective FPS.

#### 4.7 SoftISP (V1 then V2)
1. Read `.raw` → `cl::Buffer` (uchar, RGGB).
2. V1 dispatch: `debayer_naive` global memory kernel → `cl::Event` time.
3. V2 dispatch: `debayer_lds` LDS-tiled kernel → `cl::Event` time.
4. Read both outputs. Assert pixel-identical (byte-exact).
5. Write `output_rgb_v1.bmp`, `output_rgb_v2.bmp`.
6. Print: V1 ms, V2 ms, speedup, effective FPS at resolution.

---

## Key Decisions (and Rationale)

1. **Non-linear Structure — No Sequential Dependency Between Add-ons**
   - **Why**: Users arrive at add-ons from different Module 2 tracks with different experience. Enforcing a fixed order would block, e.g., an embedded engineer (Track A) from accessing 4.7 SoftISP while waiting on 4.5 Voxel Mapping prerequisites.

2. **vkFFT via FetchContent, Not System Install**
   - **Why**: Consistent with the project-wide standalone-buildable rule. A user who copies `4_1_vkFFT_Audio/` to a new machine should not need to pre-install vkFFT. This also avoids version mismatch across machines.

3. **4.2 Is a Written Report, Not a Benchmark Binary**
   - **Why**: An honest OpenCL vs CUDA comparison is primarily an ecosystem and use-case analysis, not a micro-benchmark. A binary producing numbers from a single GPU would mislead more than inform. The structured markdown format forces explicit decision criteria.

4. **4.3 Wraps Module 1, Not Module 2 Projects**
   - **Why**: `01_VisualKernel` has the simplest dependency graph (no FFmpeg, no ROS 2, no OpenGL). Packaging a complex project would conflate deployment complexity with application complexity. Wrapping the simplest working binary isolates the deployment lesson.

5. **4.4 SVM Paths in a Single Binary**
   - **Why**: Running four memory strategies in one binary with identical input/output eliminates measurement noise from process startup and OS scheduling. The comparison table becomes the artifact, not four separate invocations.

6. **4.6 Hardware Interop with Mandatory Software Fallback**
   - **Why**: `cl_intel_va_api_media_sharing` is Intel/AMD-only; the EGL path covers Nvidia. Neither is available in CPU/PoCL environments. A software-decode fallback ensures the binary is testable in CI without a hardware decoder and documents the cost of skipping zero-copy.

7. **4.7 Pixel-Identical Assertion In-Binary**
   - **Why**: A BMP visual comparison by eye is insufficient — human vision cannot detect single-pixel errors at the image boundary that indicate a halo indexing bug. An in-binary byte-exact diff produces a deterministic, machine-verifiable DoD and teaches correctness-first optimization.

8. **4.5 Reuses B3 Ray-AABB Math Unchanged**
   - **Why**: The educational payoff of the grand finale is demonstrating knowledge transfer: a graphics technique (ray-box intersection from the BVH traversal) solves a robotics problem (lidar ray casting in voxel space). Using the identical function reinforces this point explicitly.

---

## Known Issues / Risks

- **vkFFT OpenCL Backend Maturity**: vkFFT's OpenCL backend lags behind its Vulkan backend. Some radix configurations may fail on non-Nvidia drivers. The add-on must catch `VkFFTResult != VKFFT_SUCCESS` and emit a human-readable error with the failing FFT configuration.
- **VAAPI/EGL Surface Mapping Vendor Lock**: `clCreateFromVA_APIMediaSurfaceINTEL` is Intel/AMD-only. The software fallback must be tested on all three GPU vendors. The extension string check must match per-platform: Intel/AMD uses `cl_intel_va_api_media_sharing`; Nvidia uses `cl_khr_egl_image`.
- **SVM Fine-Grained Availability**: OpenCL 2.0 SVM fine-grained is not supported on NVIDIA OpenCL drivers. The 4.4 benchmark must detect `CL_DEVICE_SVM_CAPABILITIES` at runtime and skip inaccessible paths with a clear console message, not a crash.
- **4.5 Atomic Contention at High Point Density**: DDA traversal with `atomic_or` on a shared voxel grid can serialize under high point density (100k points, small resolution). The Known Issue must be documented in the console output and in the README. Mitigation (per-thread staging buffer + merge kernel) is the challenge, not the base implementation.
- **4.7 LDS Tile Halo Boundary**: Bilinear debayer requires a 1-pixel halo. Work-items at image edges must clamp indices to `[0, width-1]` × `[0, height-1]`. Incorrect clamping is the most common bug; the pixel-identity assertion catches it.
- **4.3 Docker PoCL vs Native Driver**: The deployment container uses PoCL for portability. If the packaged binary is tested with a system OpenCL driver inside Docker, `GPU` env var selection must still work. The Dockerfile must install the ICD loader (`ocl-icd-libopencl1`) and at minimum PoCL as the runtime.
- **Asset Availability**: `assets/sample.wav`, `assets/raw_bayer_4k.raw`, `assets/sample.mp4`, `assets/lidar_sample.bag` are large binary files. CMake must check for their presence and emit `message(WARNING)` if missing, not a build error. The binary must fail gracefully at runtime with a clear error when the asset file does not exist.

---

## Performance Gates (Module Completion)

| Add-on | Metric | Target |
| :--- | :--- | :--- |
| 4.1 vkFFT Spectrogram | GPU FFT batch (1024 frames × 2048 bins) | < 2 ms |
| 4.1 vkFFT vs FFTW | Speedup | ≥ 10× (reported in console) |
| 4.4 SVM Benchmark | `CL_MEM_USE_HOST_PTR` vs `CL_MEM_COPY_HOST_PTR` | Reported; USE_HOST_PTR expected ≤ 50% of COPY time on iGPU |
| 4.5 Voxel Mapping | End-to-end pipeline per frame | < 5 ms @ 100k points (inherits C3 gate) |
| 4.6 FFmpeg Transcoder | Total per-frame time (decode + map + filter + encode) | < 10 ms @ 1080p (≥ 100 FPS) |
| 4.7 SoftISP V2 LDS | Debayer 4K RGGB → RGBA | < 10 ms (≥ 100 FPS at 3840×2160) |
| 4.7 V2 vs V1 | Speedup | ≥ 3× reported in console |

All GPU timing via `cl::Event` profiling in milliseconds to 3 decimal places. CPU timing via `std::chrono::steady_clock`. Wall-clock estimates do not satisfy any gate. 4.2 and 4.3 have no numeric performance gate.

---

## Specifications & Standards

- **Directory Structure**:
  ```
  04_Addons/
  ├── Addons.md                         (user-facing index, existing)
  ├── 4_1_vkFFT_Audio/
  │   ├── CMakeLists.txt
  │   ├── main.cpp
  │   └── kernels/                       (none — vkFFT generates kernels internally)
  ├── 4_2_OpenCL_vs_CUDA/
  │   ├── report.md                      (written artifact — no binary)
  │   └── code_comparison/
  │       ├── vector_add.cu              (CUDA reference, not compiled)
  │       └── vector_add.cl              (OpenCL equivalent)
  ├── 4_3_Deployment/
  │   ├── CMakeLists.txt                 (install() rules)
  │   ├── Dockerfile
  │   └── appimage.sh
  ├── 4_4_SVM_Theory/
  │   ├── CMakeLists.txt
  │   ├── main.cpp
  │   └── kernels/passthrough.cl
  ├── 4_5_Voxel_Mapping/
  │   ├── CMakeLists.txt
  │   ├── main.cpp
  │   └── kernels/
  │       ├── dda_cast.cl
  │       └── flip_count.cl
  ├── 4_6_FFmpeg_Pipeline/
  │   ├── CMakeLists.txt
  │   ├── main.cpp
  │   └── kernels/filter.cl              (reuse / symlink from Track A)
  └── 4_7_SoftISP/
      ├── CMakeLists.txt
      ├── main.cpp
      └── kernels/
          ├── debayer_naive.cl
          └── debayer_lds.cl
  ```
- **Verification Standard**:
  - 4.1: `output_spectrogram.bmp` (frequency-vs-time heatmap, non-uniform color distribution). Console: GPU ms, CPU ms, speedup.
  - 4.2: `report.md` present and non-empty. No binary.
  - 4.3: `docker build` succeeds; `docker run` produces `output.bmp` identical to a local run.
  - 4.4: `output_svm_verify.bmp` matches the input image. Console: time table for all available paths.
  - 4.5: `output_voxel_slice.bmp` (occupied black, free white, unknown grey). Console: per-stage ms summing to < 5 ms.
  - 4.6: `filtered.mp4` plays correctly with effect visible. Console: per-frame breakdown.
  - 4.7: `output_rgb_v1.bmp` and `output_rgb_v2.bmp` pixel-identical and visually correct (no Bayer pattern visible). Console: V1 ms, V2 ms, speedup.
- **Tooling**:
  - `cl.hpp` (C++ bindings, OpenCL 1.2 baseline) for all binaries.
  - `stb_image` / `stb_image_write` for all BMP I/O.
  - `CLI11` via `common/common.cmake` for all argument parsing.
  - `find_package(OpenCL REQUIRED)` in every `CMakeLists.txt`.
  - `common/ocl_wrapper.hpp` → `create_context()` for GPU selection.
  - `CL_CHECK(err)` macro from `common/opencl_utils.hpp` for all error handling.
  - vkFFT: `FetchContent_Declare` in 4.1.
  - FFmpeg: `find_package(PkgConfig REQUIRED)` → `pkg_check_modules(FFMPEG REQUIRED libavcodec libavformat libavutil)` in 4.6.
  - ROS 2: `find_package(rclcpp REQUIRED)` + `$ENV{ROS_DISTRO}` guard in 4.5 only.
  - FFTW3: `find_package(FFTW3)` (optional) in 4.1.
- **OpenCL 2.0+ Gating**: SVM fine-grained (4.4) and any `enqueue_kernel` use must be wrapped in `#ifdef CL_VERSION_2_0`. Runtime `CL_DEVICE_SVM_CAPABILITIES` check required before any SVM allocation. All 2.0+ paths must fail gracefully with a descriptive message and exit code 0 (not a crash) when the device does not support them.

---

## Prerequisites

- Module 1 completed (`01_Host_API/`): `cl.hpp` usage, `cl::Event` profiling, `CL_CHECK` error handling.
- Module 2 specialization prerequisites per add-on:
  - 4.1: Any Module 2 track.
  - 4.2: Any Module 2 track.
  - 4.3: Module 1 only.
  - 4.4: Any Module 2 track.
  - 4.5: Track B (B3 complete) + Track C (C3 complete) + ROS 2 Humble+.
  - 4.6: Track A (A4 complete) + FFmpeg with hardware acceleration (`libavcodec-dev`, `libavformat-dev`, `libavutil-dev`).
  - 4.7: Toolbox `LocalMemory` tool reviewed. No external library dependencies.
- `assets/` files: `sample.wav` (4.1), `raw_bayer_4k.raw` (4.7), `sample.mp4` (4.6), `lidar_sample.bag` (4.5). See asset download instructions in [main README](../README.md).

See [main README](../README.md) for base requirements (OpenCL 1.2+, CMake 3.18+, Docker setup).
