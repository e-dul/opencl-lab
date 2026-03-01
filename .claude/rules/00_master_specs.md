# Master Specifications & Standards (Global)

> **Role**: The Single Source of Truth for technical standards across ALL modules.
> **Inheritance**: All other Design Docs implicitly inherit these constraints unless overridden.

## 1. Technical Stack
- **Language**: C++17 (Strict). Use `auto`, lambdas, `std::filesystem`. No C++20 (embedded compatibility).
- **OpenCL**: Version 1.2 Baseline.
  - **Wrapper**: `cl.hpp` (C++ bindings). **FORBIDDEN**: Raw `clCreateBuffer` / `clReleaseMemObject`.
  - **Extensions**: OpenCL 2.0 features (SVM, Device Enqueue) must be guarded by macros.
- **Build System**: CMake 3.18+.
  - Every sub-project (`01_Visual_Kernel`) must be standalone buildable via `cmake -B build && cmake --build build` from within that directory. No shared parent required.
  - `find_package(OpenCL REQUIRED)` must be used in every `CMakeLists.txt`.
  - Kernel files must be copied to the binary directory post-build using:
    ```cmake
    add_custom_command(TARGET <target> POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_directory
              ${CMAKE_CURRENT_SOURCE_DIR}/kernels
              $<TARGET_FILE_DIR:<target>>/kernels
      COMMENT "Copying kernels"
    )
    ```
- **CLI Parsing**: Use [CLI11](https://github.com/CLIUtils/CLI11) (v2.4.2, header-only, FetchContent) in all modules.
  - **FORBIDDEN**: Hand-rolled `Args` structs or custom `parse_args()` functions.
  - Integrated via `common/common.cmake`; linked as `CLI11::CLI11`.

## 2. Directory Structure Protocol
- **Snapshots over Branches**: Code evolves in sequential folders (`01_Basic/`, `02_Optimized/`).
- **Dependencies**:
  - `common/`: Header-only shared utilities (Platform selection, IO).
  - `vendor/`: Third-party header-only libs (`stb_image`, `cl.hpp`).
  - **Rule**: No complex linking. User must be able to copy-paste code easily.

## 3. Input / Output Standards
- **Visual Verification**: All kernels must produce visual artifacts (`output.bmp`). Console text alone is not enough.
  - **Exception**: Purely numeric tools (benchmarks, pipeline timing, multi-GPU) must produce a structured console timing table as the artifact. No BMP required.
- **Image IO**: Use `stb_image.h` and `stb_image_write.h`.
  - **Format**: BMP or PNG (no JPEG to avoid compression artifacts in debugging).
  - **Data**: RGBA (4 channels) or Grayscale (1 channel).
- **Asset Paths**: All modules read assets from `assets/` at the repository root. Paths must be passed via CLI args. **FORBIDDEN**: Hardcoded asset paths in code.

## 4. Error Handling Protocol
- **Host Code**: Throw `std::runtime_error` for CL errors.
  - Use `common/opencl_utils.hpp` macros: `CL_CHECK(err)`.
- **Kernels**: No printfs inside kernels (unless debugging). Output error codes to a debug buffer if needed.
- **OpenCL 2.0+ Graceful Fallback**: When a binary includes OpenCL 2.0+ features (SVM, Device Enqueue) and the device does not support them, the binary **must** print a descriptive message and exit with code 0. Crashes or silent hangs are forbidden.

## 5. Runtime Environment

- **GPU Selection**: Controlled via the `GPU` env var (vendor substring, case-insensitive).
  - Implemented in `common/ocl_wrapper.hpp` → `create_context()`.
  - Matches against `CL_PLATFORM_VENDOR` and `CL_DEVICE_VENDOR` (handles Mesa/rusticl stacks).
  - Examples: `GPU=NVIDIA`, `GPU=AMD`, `GPU=INTEL`.
  - Default (unset): first platform with a GPU; CPU fallback if none found.
  - **FORBIDDEN**: Hard-coded platform/device indices in module code.

## 6. Performance Culture
- **Profiling**: `cl_event` profiling is MANDATORY for all "Optimized" milestones.
- **Reporting**: Report time in milliseconds (ms) to 3 decimal places.
  - **GPU stages**: Timed via `cl::Event` (`CL_PROFILING_COMMAND_START` / `CL_PROFILING_COMMAND_END`).
  - **CPU stages** (serialization, publish, host logic): Timed via `std::chrono::steady_clock`.
- **Wall-clock measurements do NOT satisfy performance gates.** Only `cl::Event` profiling results count for GPU gate verification.
