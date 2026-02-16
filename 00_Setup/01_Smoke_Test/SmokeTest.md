# Module 0: Smoke Test

This is the most basic "Hello World" for OpenCL. 
Its purpose is to verify that your drivers, C++ compiler, and build system are working correctly.

## What it does
1.  Detects OpenCL Platform (Intel/Nvidia/AMD).
2.  Allocates memory on the GPU.
3.  Copies two vectors `[1,1,1...]` and `[2,2,2...]`.
4.  Runs a kernel to add them.
5.  Verifies the result is `[3,3,3...]`.

## How to Run

```bash
mkdir build
cd build
cmake ..
make
./smoke_test
```

## Troubleshooting
*   **"No OpenCL platforms found"**: Install `intel-opencl-icd` (Intel) or check NVIDIA drivers.
*   **"cl.hpp not found"**: Ensure `vendor/CL/cl.hpp` exists in the repo root or standard system paths.