// 00_Setup/01_Smoke_Test/kernels/vector_add.cl
// Simple Vector Addition Kernel
// Description: Adds two vectors A and B, stores result in C.

__kernel void vector_add(__global const float* a, 
                         __global const float* b, 
                         __global float* c, 
                         const int n) {
    // WHY size_t: get_global_id() returns size_t; assigning to int silently
    // truncates on large NDRanges and produces signed/unsigned comparison warnings.
    size_t i = get_global_id(0);

    // Boundary check to prevent out-of-bounds access
    if (i < (size_t)n) {
        c[i] = a[i] + b[i];
    }
}
