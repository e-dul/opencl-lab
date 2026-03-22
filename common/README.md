# common/ — Reference Card

Header-only utilities shared across all modules. Include via the `common` INTERFACE target (wired by `common.cmake`).

---

## Files

| File | Purpose |
|---|---|
| `opencl_utils.hpp` | Core OpenCL boilerplate: version macros, `CL_CHECK`, kernel loading, event timing, NDRange helpers, program compilation with build-log on error. |
| `ocl_wrapper.hpp` | One-call OpenCL context bootstrap (`create_context`). Reads `GPU` env var to select vendor; CPU fallback. Returns `OclContext` aggregate. |
| `image_utils.hpp` | stb-backed image IO: load RGB/RGBA, save BMP, load raw binary, generate test gradient. |
| `graphics_hpc_utils.hpp` | GPU framebuffer readback + tonemap to BMP (`save_framebuffer`). For ray-tracing and rasterisation modules. |
| `bvh_utils.hpp` | CPU-side BVH types (`Aabb`, `TriangleCpu`, `BvhNode`, `BvhTree`) and math primitives (`sub3`, `cross3`, `dot3`, `safe_rcp`, `moller_trumbore`). |
| `common.cmake` | CMake helper: fetches OpenCL/stb/CLI11, defines `opencl_lab_target()`, `copy_kernels()`, GL-interop detection, tinyobjloader fetch, ROS 2 guards. |
| `CMakeLists.txt` | Declares the `common` INTERFACE library; exposes `common/` and `vendor/` include paths. |

---

## Usage Snippets

### Bootstrap an OpenCL context
```cpp
#include "ocl_wrapper.hpp"

OclContext ocl = create_context();   // honours GPU=NVIDIA / GPU=AMD env var
cl::CommandQueue& q = ocl.queue;
```

### Compile a kernel and time it
```cpp
#include "opencl_utils.hpp"

cl::Program prog = build_program(ocl.context, ocl.device,
                                 (get_binary_dir() / "kernels/my.cl").string());
cl::Event ev;
CL_CHECK(ocl.queue.enqueueNDRangeKernel(kernel, cl::NullRange,
                                         cl::NDRange(round_up(n, 64)),
                                         cl::NDRange(64), nullptr, &ev));
CL_CHECK(ocl.queue.finish());
std::cout << duration_ms(ev) << " ms\n";
```

### Load an image and save result
```cpp
// In one .cpp file only, before including image_utils.hpp:
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image.h>
#include <stb_image_write.h>
#include "image_utils.hpp"

int w, h, ch;
auto pixels = load_rgba_image("assets/photo.png", w, h, ch);
// ... process pixels ...
save_bmp("output.bmp", pixels, w, h, ch);
```

### Wire a module target (CMakeLists.txt)
```cmake
include(${CMAKE_CURRENT_SOURCE_DIR}/../../common/common.cmake)
add_executable(my_demo main.cpp)
opencl_lab_target(my_demo)
copy_kernels(my_demo)
```
