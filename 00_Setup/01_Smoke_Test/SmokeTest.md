# Module 0: Smoke Test

This is the most basic "Hello World" for OpenCL. 
Its purpose is to verify that your drivers, C++ compiler, and build system are working correctly.

## What it does
1.  Detects OpenCL Platform (Intel/Nvidia/AMD).
2.  Allocates memory on the GPU.
3.  Copies two vectors `[1,1,1...]` and `[2,2,2...]`.
4.  Runs a **kernel** to add them. A **kernel** is a function that runs in parallel on the GPU — each work-item (thread) processes one element of the vector.
5.  Verifies the result is `[3,3,3...]`.

## How to Run

```bash
cmake -B build && cmake --build build
```

> **Note:** In this repo, OpenCL kernel `.cl` files are symlinked into the build directory at build time and must be present next to the binary at runtime. Run the binary from inside the `build/` directory — the kernel loader uses a relative path to locate `kernels/vector_add.cl`. Running from a parent directory will silently fail with "cannot open kernel file".

```bash
./smoke_test
```

## Troubleshooting
*   **"No OpenCL platforms found"**: Install `intel-opencl-icd` (Intel) or check NVIDIA drivers.
*   **"cl.hpp not found"**: The OpenCL C++ header is provided by the `opencl-headers` apt package (installed in Module 0 §4). It lives at `/usr/include/CL/opencl.hpp` — no `vendor/` directory is needed. Note: `opencl.hpp` is the modern C++ bindings; `cl.hpp` is the legacy 1.2 bindings used by this course. Both may coexist at `/usr/include/CL/`.

---

[Back to Setup.md](../Setup.md)
