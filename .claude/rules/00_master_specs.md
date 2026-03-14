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
  - `find_package(OpenCL REQUIRED)` must be used in every `CMakeLists.txt`. Unless it's included via common.cmake.
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
- **Utilities**: try to use image utils from `common/image_utils.hpp` instead implementing from scratch each time
- **Image IO**: Use `stb_image.h` and `stb_image_write.h`.
  - **Format**: BMP or PNG (no JPEG to avoid compression artifacts in debugging).
  - **Data**: RGBA (4 channels) or Grayscale (1 channel).
- **Asset Paths**: All modules read assets from `assets/` at the repository root. Paths must be passed via CLI args. **FORBIDDEN**: Hardcoded asset paths in code.

## 4. Error Handling Protocol
- **Host Code**: Throw `std::runtime_error` for CL errors.
  - Use `common/opencl_utils.hpp` macros: `CL_CHECK(err)` only for raw cl_int return values not covered by the C++ bindings.
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
- When performance criteria/gate for task is not met try `GPU=AMD` to gather more data

## 7. Code Correctness Checklist (common review failures)

### 7.1 Integer Arithmetic Safety
- **FORBIDDEN**: `static_cast<size_t>(a * b * c)` when `a`, `b`, `c` are `int` — the multiplication overflows before the cast.
- **REQUIRED**: Promote the first operand before multiplying: `static_cast<size_t>(a) * b * c`.
- Applies to: buffer sizes, image index arithmetic (`y * width + x`), stbi dimensions.
- When passing pixel count to a kernel as `cl_int`: throw `std::runtime_error` if `size > INT_MAX`. **FORBIDDEN**: silent `std::min` truncation.

### 7.2 OpenCL Return Code Coverage
- `CL_HPP_ENABLE_EXCEPTIONS` causes **constructors** and `getInfo<>()` to throw automatically.
- The following methods return `cl_int` and do **NOT** throw — wrap every call in `CL_CHECK`:
  - `cl::Kernel::setArg()`
  - `cl::CommandQueue::finish()`
  - `cl::CommandQueue::enqueueNDRangeKernel()`
  - `cl::CommandQueue::enqueueReadBuffer()` / `enqueueWriteBuffer()`

### 7.3 CMake Strictness
- Every `CMakeLists.txt` must set `set(CMAKE_CXX_EXTENSIONS OFF)` alongside `CMAKE_CXX_STANDARD 17` to enforce `-std=c++17` (not `-std=gnu++17`).

### 7.4 Kernel Correctness
- Use `size_t gid = get_global_id(0)` — not `int`. `get_global_id()` returns `size_t`.
- Guard: `if (gid < (size_t)size)` to avoid signed/unsigned comparison warnings.

### 7.5 WHY Comments
- Non-obvious flag combinations (e.g. `CL_MEM_ALLOC_HOST_PTR | CL_MEM_COPY_HOST_PTR`) must have a comment explaining why both flags are combined, not just what they do.

### 7.6 GL Interop Teardown
- When using `cl_khr_gl_sharing`, **all** CL objects that reference GL resources (`cl::ImageGL`, `cl::Context` created with GL props, `cl::CommandQueue` on that context) must be destroyed **before** `glfwTerminate()` / `glDeleteTextures()`.
- Scope inner objects (`cl::ImageGL`, `cl::CommandQueue`, kernels, buffers) in an explicit `{}` block that ends before GL teardown.
- Explicitly reset the shared `cl::Context` itself — `cl_ctx = cl::Context();` — after the inner scope, before `glfwTerminate`. A `cl::Context` declared outside the inner scope is not destroyed by the inner scope's exit and will crash in `clReleaseContext` after the GL context is gone.

### 7.7 Other
- Throw exceptions instead of using `std::exit`
- Remove unused headers
- Always wrap enqueueUnmapMemObject and getInfo (two-arg overload) in CL_CHECK — they are silent cl_int returners, not covered by CL_HPP_ENABLE_EXCEPTIONS.
- When changing kernel parameter types, update the host setArg type atomically in the same edit to avoid width mismatches.
- Coarse-grained SVM requires clEnqueueSVMMap before any host read/write — timing benchmarks that unmap before filling must re-map for the fill, or restructure the benchmark to separate timing from data initialization.

## 8. Standard Definition of Done
Every task inherits these baseline DoD items unless explicitly marked inapplicable:
- [ ] `cmake -B build && cmake --build build` succeeds with zero errors and zero warnings.
- [ ] Binary runs without arguments and completes without error.
- [ ] `--help` prints CLI11-generated usage including all defined flags.
- [ ] `GPU=<vendor> ./build/<bin>` selects the correct device without crashing.

DoD performance gates for speedup should include a hardware-waiver clause from the start, rather than targeting a fixed ratio that depends on driver internals.