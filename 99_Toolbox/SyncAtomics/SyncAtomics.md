# Sync & Atomics

**Symptom**: Kernel produces incorrect histogram counts, wrong reduction totals, or non-deterministic output — always silently. No crash, just wrong numbers when multiple work-items write to the same memory location concurrently.

## Prerequisites
Prerequisites: OpenCL 1.2+, CMake 3.18+, `clinfo` installed. See [main README](../../README.md) for base requirements.

## Build & Run
```bash
cd 99_Toolbox/SyncAtomics
cmake -B build && cmake --build build
./build/sync_atomics --size 1048576
```

## Verify
```
[Unsafe   ] 1M-element histogram (256 bins):  corrupt — bins sum to 876,231 ≠ 1,048,576
[Global   ] 1M-element histogram (256 bins):  correct —  23.4 ms  (atomic_add global)
[Local+Red] 1M-element histogram (256 bins):  correct —   5.1 ms  (local atomics + merge)
```

The unsafe run will show a different total on every execution — that non-determinism is the bug.

## Concept

### Race Conditions
When two work-items execute `histogram[bin]++` simultaneously, both read the same stale value, both increment it, and one write overwrites the other. One increment is silently lost.

```cl
// BROKEN — classic read-modify-write hazard
histogram[bin] = histogram[bin] + 1;  // two work-items, one lost update
```

This happens in: histogram building, parallel reductions (sum, max), reference counting, BVH node allocation.

### Atomic Operations
Atomics guarantee the read-modify-write is indivisible — the hardware serialises concurrent access to the same address.

| Function | Behaviour |
|:---------|:----------|
| `atomic_add(&x, val)` | Returns old value, adds `val` atomically |
| `atomic_inc(&x)` | Equivalent to `atomic_add(&x, 1)` |
| `atomic_max(&x, val)` | Updates `x` only if `val > x`, atomically |
| `atomic_cmpxchg(&x, cmp, val)` | Replaces `x` with `val` only if `x == cmp` |

```cl
// CORRECT — atomic_add serialises concurrent writes
atomic_add(&histogram[bin], 1);
```

**Global vs. Local atomics**: `atomic_add` on a `__global` pointer serialises across the entire device — expensive on high-contention bins. `atomic_add` on a `__local` pointer serialises only within the work-group — ~10× cheaper. The strategy: accumulate into a private local histogram per work-group, then merge into global memory once per group.

```cl
__kernel void histogram_fast(__global const uchar* data,
                              __global int* global_hist,
                              __local  int* local_hist,
                              int size) {
    int lid = get_local_id(0);
    // Zero local histogram
    for (int b = lid; b < 256; b += get_local_size(0))
        local_hist[b] = 0;
    barrier(CLK_LOCAL_MEM_FENCE);

    // Accumulate into local memory (low contention)
    size_t gid = get_global_id(0);
    if (gid < (size_t)size)
        atomic_add(&local_hist[data[gid]], 1);
    barrier(CLK_LOCAL_MEM_FENCE);

    // Merge once per work-group into global (high contention, but rare)
    for (int b = lid; b < 256; b += get_local_size(0))
        atomic_add(&global_hist[b], local_hist[b]);
}
```

### Compare-and-Swap
`atomic_cmpxchg` is the foundation for lock-free algorithms. It atomically performs: *if `*p == expected`, write `desired` and return `expected`; otherwise return the current value*.

```cl
// Spinlock using CAS — for illustrative purposes only
void lock(__global int* mutex) {
    while (atomic_cmpxchg(mutex, 0, 1) != 0) {}  // spin until we own it
}
void unlock(__global int* mutex) {
    atomic_xchg(mutex, 0);
}
```

**Cost**: GPU atomics are expensive. A single contended `atomic_add` to global memory can cost 100–500× more than a regular store. Use them only when unavoidable, and always prefer the local-accumulate-then-merge pattern.

## Mini-Challenge

Change the histogram kernel to track the **maximum** value seen per bin (not the count). Use `atomic_max` in the local phase and verify the global maximum matches `*std::max_element` on the host.

## Troubleshooting

- **Histogram sums don't match element count**: Forgot `barrier(CLK_LOCAL_MEM_FENCE)` before the merge phase — some work-items begin reading `local_hist` before others finish writing.
- **`atomic_add` on float**: OpenCL 1.2 atomics operate on `int`/`uint` only. To accumulate floats atomically, implement a CAS loop: read, compute `old + val`, `cmpxchg`, retry on failure.

```c
float old_val, new_val;
do {
    old_val = *(__global float*)addr;
    new_val = old_val + delta;
} while (atom_cmpxchg((__global int*)addr,
                      *(int*)&old_val,
                      *(int*)&new_val) != *(int*)&old_val);
```

- **Correct on CPU, wrong on GPU**: CPU OpenCL drivers often serialise work-items; the race only surfaces on real GPU hardware. Always test on the target device.

## Used In
- Any kernel that builds a histogram (image processing, tone mapping, depth-of-field)
- Parallel prefix-sum and reduction stages in Track A/B/C projects
- BVH node allocation in [B3_Ray_Tracer_BVH](../../02_Projects/B_Graphics_HPC/GraphicsHPC.md)

---

[Back to Toolbox](../Toolbox.md)
