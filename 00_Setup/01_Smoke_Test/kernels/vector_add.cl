// 00_Setup/01_Smoke_Test/kernels/vector_add.cl
// Simple Vector Addition Kernel
// Description: Adds two vectors A and B, stores result in C.

__kernel void vector_add(__global const float* a, 
                         __global const float* b, 
                         __global float* c, 
                         const int n) {
    // Get the global thread ID (index in the vector)
    int i = get_global_id(0);

    // Boundary check to prevent out-of-bounds access
    if (i < n) {
        c[i] = a[i] + b[i];
    }
}
