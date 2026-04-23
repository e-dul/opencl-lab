#pragma once
// lbvh_types.hpp — GPU-ready node layout and tree result for the LBVH pipeline.
//
// Node array layout (2N-1 nodes total for N triangles):
//   indices [0 .. N-2]   : internal nodes  (Karras 2012 convention)
//   indices [N-1 .. 2N-2]: leaf nodes
//
// WHY separate from BvhNode in common/bvh_utils.hpp:
//   BvhNode uses hit_link/miss_link for stackless traversal.
//   LbvhNode uses left_child/right_child/parent for the Karras parallel build
//   and the stack-based traversal kernel. The two layouts are fundamentally different.

#include <CL/opencl.hpp>
#include <vector>

// ---------------------------------------------------------------------------
// LbvhNode — GPU-ready layout for the LBVH node array.
//
// WHY 4-element padding after children: cl_float[3] in a struct has 12-byte
// extent. Without padding, the struct would be 40 bytes — not a power-of-two
// boundary. Padding to 48 bytes aligns each node to a 16-byte boundary,
// matching the 4-element SIMD lanes on most GPUs and avoiding split cache lines.
//
// WHY float3 alignment trap: OpenCL's built-in `float3` type has the SAME
// alignment and storage as `float4` (16 bytes). If you declare `float3 aabb_lo`
// in a C struct, the C compiler packs it to 12 bytes, but OpenCL pads to 16 —
// causing a HOST/DEVICE LAYOUT MISMATCH. Solution: use `float aabb_lo[3]` with
// an explicit float pad, matching what the OpenCL compiler will emit.
// ---------------------------------------------------------------------------
struct LbvhNode {
    cl_int   left_child;    // index into node array; -1 = unset
    cl_int   right_child;   // index into node array; -1 = unset
    cl_int   parent;        // index into node array; -1 = root
    cl_int   pad;           // explicit padding — see float3 alignment trap above

    cl_float aabb_lo[3];    // tight AABB minimum corner (set by lbvh_aabb kernel)
    cl_float aabb_lo_pad;   // alignment pad (must be cl_float, not padding char)

    cl_float aabb_hi[3];    // tight AABB maximum corner
    cl_float aabb_hi_pad;   // alignment pad
};

// WHY 48 bytes: 4×cl_int (16) + 4×cl_float (16) + 4×cl_float (16) = 48.
// Changing this size means the kernel struct layout no longer matches the host —
// add/remove fields only in pairs (host + kernel) and update this assert.
static_assert(sizeof(LbvhNode) == 48,
    "LbvhNode must be 48 bytes — float3 alignment trap: use float[3] + float pad");

// ---------------------------------------------------------------------------
// LbvhTree — result of one full GPU LBVH build.
// node_buf / sorted_idx_buf: GPU-resident cl::Buffers populated by LbvhBuilder.
// ---------------------------------------------------------------------------
struct LbvhTree {
    cl::Buffer node_buf;        // device copy: 2N-1 LbvhNodes
    cl::Buffer sorted_idx_buf;  // device copy: N sorted triangle indices
};

// ---------------------------------------------------------------------------
// TimingBreakdown — per-frame GPU timing from the 6-kernel build pipeline.
//
// All times are cl::Event GPU measurements in milliseconds.
// ---------------------------------------------------------------------------
struct TimingBreakdown {
    double scene_bounds_ms = 0.0;  // kernel 1: scene AABB reduction
    double morton_ms       = 0.0;  // kernel 2: Morton code computation
    double sort_ms         = 0.0;  // kernels 3a–3c: 4-pass radix sort (total)
    double build_ms        = 0.0;  // kernel 4: Karras parallel tree build
    double aabb_ms         = 0.0;  // kernel 5: parallel AABB fitting

    double bvh_build_total_ms() const {
        return scene_bounds_ms + morton_ms + sort_ms + build_ms + aabb_ms;
    }
};
