// vector_add.cl — OpenCL kernel (device code only; host side shown in report.md)
//
// WHY __kernel / __global: OpenCL C uses address-space qualifiers to tell the
// compiler where memory lives. "__global" means GPU VRAM (device-side heap),
// equivalent to CUDA's default pointer space inside a __global__ function.

__kernel void vector_add(__global const float* a,   // read-only input
                         __global const float* b,   // read-only input
                         __global       float* c,   // write-only output
                         const int n)               // element count
{
    // WHY size_t: get_global_id() returns size_t.  Using int would silently
    // truncate on arrays >2 GiB and cause signed/unsigned comparison warnings.
    size_t gid = get_global_id(0);

    // WHY explicit guard: the NDRange is padded to a multiple of the work-group
    // size, so the last few threads may be out-of-bounds.
    if (gid < (size_t)n) {
        c[gid] = a[gid] + b[gid];
    }
}
