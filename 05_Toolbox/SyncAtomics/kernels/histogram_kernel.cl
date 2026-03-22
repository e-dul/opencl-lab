// histogram_kernel.cl — Three histogram variants demonstrating atomics in OpenCL 1.2.
// All kernels operate on uchar input (values 0–255) into a 256-bin int histogram.

// ─────────────────────────────────────────────────────────────────────────────
// Kernel A: histogram_unsafe
// WHY this is broken: each work-item reads histogram[bin], increments on the
// host register, and writes back. When multiple work-items share the same bin,
// their read-modify-write sequences interleave (data race), causing lost updates.
// The bin sum will be < n_elements on real GPU hardware. Intentionally broken to
// demonstrate the need for atomic operations.
// ─────────────────────────────────────────────────────────────────────────────
__kernel void histogram_unsafe(__global const uchar* data,
                               __global int* histogram,
                               int size) {
    size_t gid = get_global_id(0);
    if (gid < (size_t)size) {
        // WHY naked ++: this is a non-atomic read-modify-write — intentionally
        // racy to show what happens without synchronisation primitives.
        histogram[data[gid]]++;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Kernel B: histogram_global_atomic
// WHY atomic_add on __global: global atomic_add serialises all work-items that
// try to update the same bin across the entire device. This eliminates the data
// race but causes contention: work-items queue up on hot bins (e.g. uniform
// distributions have 256 bins each hit ~4096 times for 1M elements).
// ─────────────────────────────────────────────────────────────────────────────
__kernel void histogram_global_atomic(__global const uchar* data,
                                      __global int* histogram,
                                      int size) {
    size_t gid = get_global_id(0);
    if (gid < (size_t)size) {
        atomic_add(&histogram[data[gid]], 1);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Kernel C: histogram_local
// Two-phase strategy: accumulate into per-work-group local memory (fast, only
// one work-group contends per bin), then merge local results to global memory.
//
// WHY barrier after zero-fill (first barrier): local memory is uninitialised by
// default. Work-items must not start accumulating until all lanes have zeroed
// their share of local_hist — otherwise some bins may start from garbage values.
//
// WHY barrier after accumulation (second barrier): the merge phase reads
// local_hist. Without the barrier, the merge could start before slower lanes
// finish their atomic_add into local_hist, producing partial (too-low) counts.
// ─────────────────────────────────────────────────────────────────────────────
__kernel void histogram_local(__global const uchar* data,
                              __global int* global_hist,
                              __local int* local_hist,
                              int size) {
    size_t gid = get_global_id(0);
    int    lid = get_local_id(0);

    // Phase 1: zero this work-group's local histogram tile.
    // Loop handles work-group sizes < 256 (each lane zeros multiple bins).
    for (int b = lid; b < 256; b += get_local_size(0)) {
        local_hist[b] = 0;
    }
    // WHY first barrier: prevent any lane from reading an uninitialised bin
    // before the zero-fill is complete across the entire work-group.
    barrier(CLK_LOCAL_MEM_FENCE);

    // Phase 2: accumulate into local histogram.
    // atomic_add on __local is fast — contention is confined to one work-group.
    if (gid < (size_t)size) {
        atomic_add(&local_hist[data[gid]], 1);
    }
    // WHY second barrier: the merge reads local_hist; all lanes must finish
    // their local atomic_add before any lane begins writing to global memory.
    barrier(CLK_LOCAL_MEM_FENCE);

    // Phase 3: merge local results into the global histogram.
    // Each lane is responsible for a stride of bins.
    for (int b = lid; b < 256; b += get_local_size(0)) {
        atomic_add(&global_hist[b], local_hist[b]);
    }
}
