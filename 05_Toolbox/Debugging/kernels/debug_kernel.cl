// debug_kernel.cl — intentionally buggy kernel for Oclgrind demonstration.
//
// WHY two modes driven by a runtime 'mode' arg:
//   The compiler cannot constant-fold or dead-code-eliminate either branch
//   because 'mode' is a kernel argument resolved at runtime.  Both bugs are
//   live and will be caught by Oclgrind when the appropriate mode is selected.
//
// mode == 0 (out-of-bounds):
//   Work-item 0 writes to buf[n], which is one element past the end of the
//   allocation of n floats.  On real hardware this is silent; Oclgrind flags
//   it as "Invalid write".
//
// mode == 1 (race condition):
//   All 64 work-items unconditionally write 1.0f to buf[0] without any
//   synchronization.  The result is undefined (last writer wins) and Oclgrind
//   reports a data-race when --check-api is passed.

__kernel void debug_kernel(__global float* buf, int n, int mode)
{
    // WHY size_t: get_global_id() returns size_t; using int would produce
    // signed/unsigned comparison warnings and truncate on large work sizes.
    size_t gid = get_global_id(0);

    if (mode == 0) {
        // Out-of-bounds bug: work-item 0 writes one element past the end.
        // All other work-items do a safe in-bounds write so the kernel has
        // realistic legitimate traffic around the single bad access.
        if (gid == 0) {
            buf[n] = -1.0f;   // BUG: n is the length; valid indices are [0, n-1].
        } else if (gid < (size_t)n) {
            buf[gid] = (float)gid;
        }
    } else {
        // Race-condition bug: every work-item writes to index 0 simultaneously.
        // No barrier or atomic — the winner is non-deterministic.
        buf[0] = 1.0f;        // BUG: concurrent unsynchronized write from all work-items.

        // Write remaining elements safely so the kernel does real work.
        if (gid > 0 && gid < (size_t)n) {
            buf[gid] = (float)gid;
        }
    }
}
