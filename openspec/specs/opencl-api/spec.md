# Capability: opencl-api

Source: `00_master_specs.md` §1, §4, §7.2, §7.8; `03-safety.md` §3

## Overview

All OpenCL host code uses the C++ bindings (`cl.hpp`, version 1.2). Raw C API calls for resource management are forbidden. Errors must not be silently swallowed.

---

#### Scenario: C++ bindings usage

WHEN OpenCL resources are created or released
THEN `cl::Buffer`, `cl::Kernel`, `cl::CommandQueue`, etc. (cl.hpp) are used
AND raw `clCreateBuffer` / `clReleaseMemObject` are not called directly

#### Scenario: OpenCL 2.0+ extensions

WHEN a module uses SVM or Device Enqueue features
THEN those code paths are guarded by preprocessor macros
AND the binary prints a descriptive message and exits with code 0 if the device does not support them

#### Scenario: Constructor and getInfo errors

WHEN `CL_HPP_ENABLE_EXCEPTIONS` is enabled
THEN cl.hpp constructors and `getInfo<>()` throw automatically on failure
AND no manual error checking is required for these calls

#### Scenario: Silent cl_int returners

WHEN any of these methods are called:
- `cl::Kernel::setArg()`
- `cl::CommandQueue::finish()`
- `cl::CommandQueue::enqueueNDRangeKernel()`
- `cl::CommandQueue::enqueueReadBuffer()` / `enqueueWriteBuffer()`
- `enqueueUnmapMemObject()`
- `getInfo()` (two-arg overload)

THEN each call is wrapped in `CL_CHECK(...)` to catch the returned `cl_int`

#### Scenario: CL error propagation

WHEN a CL error occurs in host code
THEN `std::runtime_error` is thrown
AND `std::exit` is not called

#### Scenario: Integer buffer size calculation

WHEN buffer sizes or image index arithmetic involve int multiplication (e.g. `width * height * channels`)
THEN the first operand is promoted before multiplying: `static_cast<size_t>(width) * height * channels`
AND `static_cast<size_t>(a * b * c)` is never written

#### Scenario: Kernel global ID type

WHEN a kernel reads its work-item index
THEN `size_t gid = get_global_id(0)` is used (not `int`)
AND bounds checks use `if (gid < (size_t)size)` to avoid signed/unsigned warnings

#### Scenario: GL interop teardown order

WHEN a module uses `cl_khr_gl_sharing`
THEN all CL objects referencing GL resources are destroyed before `glfwTerminate()` / `glDeleteTextures()`
AND the shared `cl::Context` is explicitly reset (`cl_ctx = cl::Context()`) before GL teardown

#### Scenario: RAII for all resources

WHEN OpenCL resources are allocated
THEN RAII wrappers (`cl::Buffer`, `std::unique_ptr`) are used
AND raw `new/delete` is not used for resource management
