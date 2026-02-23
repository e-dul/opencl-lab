# Master Specifications & Standards (Global)

> **Role**: The Single Source of Truth for technical standards across ALL modules.
> **Inheritance**: All other Design Docs implicitly inherit these constraints unless overridden.

## 1. Technical Stack
- **Language**: C++17 (Strict). Use `auto`, lambdas, `std::filesystem`. No C++20 (embedded compatibility).
- **OpenCL**: Version 1.2 Baseline.
  - **Wrapper**: `cl.hpp` (C++ bindings). **FORBIDDEN**: Raw `clCreateBuffer` / `clReleaseMemObject`.
  - **Extensions**: OpenCL 2.0 features (SVM, Device Enqueue) must be guarded by macros.
- **Build System**: CMake 3.18+.
  - Every sub-project (`01_Visual_Kernel`) must be standalone buildable.
  - `find_package(OpenCL REQUIRED)` must be used.
  - Copy kernels with install instructuions to sync them easier
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
- **Visual Verification**: All kernels must produce visual artifacts (`output.bmp`). Console text is not enough.
- **Image IO**: Use `stb_image.h` and `stb_image_write.h`.
  - **Format**: BMP or PNG (no JPEG to avoid compression artifacts in debugging).
  - **Data**: RGBA (4 channels) or Grayscale (1 channel).

## 4. Error Handling Protocol
- **Host Code**: Throw `std::runtime_error` for CL errors.
  - Use `common/opencl_utils.hpp` macros: `CL_CHECK(err)`.
- **Kernels**: No printfs inside kernels (unless debugging). Output error codes to a debug buffer if needed.

## 5. Performance Culture
- **Profiling**: `cl_event` profiling is MANDATORY for all "Optimized" milestones.
- **Reporting**: Report time in milliseconds (ms) to 3 decimal places.
