#pragma once
// bvh_utils.hpp — Shared BVH types and math primitives.
//
// WHY this header exists: Aabb, TriangleCpu, BvhNode, BvhTree and the five math
// helpers (sub3, cross3, dot3, safe_rcp, moller_trumbore) were copy-pasted
// verbatim across B3, B3_Dynamic, and B4 bvh_builder.hpp files. Centralising
// them here means bug fixes (e.g. degenerate AABB handling) propagate to all
// modules in one edit.
//
// What stays in each module's bvh_builder.hpp:
//   build_bvh()       — the learning artefact (SAH splitting, hit/miss link wiring).
//   bvh_self_test()   — self-validates the module's own build_bvh.
//   B3_Dynamic additionally keeps refit(), bvh_refit_self_test().
//
// Usage: #include "../../../common/bvh_utils.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Axis-Aligned Bounding Box (AABB)
// ---------------------------------------------------------------------------
struct Aabb {
    std::array<float, 3> lo = { std::numeric_limits<float>::max(),
                                 std::numeric_limits<float>::max(),
                                 std::numeric_limits<float>::max() };
    std::array<float, 3> hi = { std::numeric_limits<float>::lowest(),
                                 std::numeric_limits<float>::lowest(),
                                 std::numeric_limits<float>::lowest() };

    void expand(float x, float y, float z) {
        lo[0] = std::min(lo[0], x); lo[1] = std::min(lo[1], y); lo[2] = std::min(lo[2], z);
        hi[0] = std::max(hi[0], x); hi[1] = std::max(hi[1], y); hi[2] = std::max(hi[2], z);
    }

    void expand(const Aabb& o) {
        for (int i = 0; i < 3; ++i) {
            lo[i] = std::min(lo[i], o.lo[i]);
            hi[i] = std::max(hi[i], o.hi[i]);
        }
    }

    // Centroid of the AABB.
    float centroid(int axis) const { return (lo[axis] + hi[axis]) * 0.5f; }

    // Surface area (used in SAH cost estimation).
    float surface_area() const {
        float dx = hi[0] - lo[0];
        float dy = hi[1] - lo[1];
        float dz = hi[2] - lo[2];
        return 2.0f * (dx * dy + dy * dz + dz * dx);
    }

    // Returns true when ray (origin + t*dir) hits this AABB.
    // WHY slab method: handles all edge cases (parallel rays, ray origins inside AABB)
    // without branches beyond the final min/max compare.
    bool intersect(const std::array<float,3>& ro,
                   const std::array<float,3>& rd_inv,
                   float t_max) const {
        float tmin = 0.0f, tmax = t_max;
        for (int i = 0; i < 3; ++i) {
            float t0 = (lo[i] - ro[i]) * rd_inv[i];
            float t1 = (hi[i] - ro[i]) * rd_inv[i];
            if (t0 > t1) std::swap(t0, t1);
            tmin = std::max(tmin, t0);
            tmax = std::min(tmax, t1);
        }
        return tmin <= tmax;
    }
};

// ---------------------------------------------------------------------------
// Triangle (CPU build-time only; runtime SoA buffers built separately)
// ---------------------------------------------------------------------------
struct TriangleCpu {
    std::array<float, 3> v[3];  // three vertices, each {x,y,z}
    std::array<float, 3> n[3];  // per-vertex normals
    int original_index;         // index into the original unsorted triangle list

    Aabb aabb() const {
        Aabb b;
        for (int i = 0; i < 3; ++i) b.expand(v[i][0], v[i][1], v[i][2]);
        return b;
    }

    std::array<float, 3> centroid() const {
        return { (v[0][0] + v[1][0] + v[2][0]) / 3.0f,
                 (v[0][1] + v[1][1] + v[2][1]) / 3.0f,
                 (v[0][2] + v[1][2] + v[2][2]) / 3.0f };
    }
};

// ---------------------------------------------------------------------------
// Flat BVH node — GPU-ready layout.
//
// Interior node: tri_start == -1; hit_link points to left child.
// Leaf node    : tri_start >= 0; contains [tri_start, tri_start + tri_count).
//
// hit_link  = index of next node to visit when the ray hits this AABB.
//             For interior: left child (== node_idx + 1 in depth-first order).
//             For leaf    : right sibling / ancestor (same as miss_link when
//                           there is no right child; defined by caller context).
// miss_link = index of next node to visit when the ray misses this AABB.
//             Points to the right sibling or, at the top of a subtree, the
//             parent's miss_link (effectively "skip the whole subtree").
//
// WHY this layout: GPU threads have no call stack. hit/miss links replace the
// recursive call stack with O(1) integer arithmetic — canonical GPU BVH trick.
// ---------------------------------------------------------------------------
struct BvhNode {
    float aabb_lo[3];
    float aabb_hi[3];
    int   hit_link;    // node index if ray hits AABB
    int   miss_link;   // node index if ray misses AABB
    int   tri_start;   // -1 for interior, else first triangle index in leaf
    int   tri_count;   // 0 for interior
    int   pad[2];      // align to 16-float boundary for GPU reads
};
static_assert(sizeof(BvhNode) == 48, "BvhNode must be 48 bytes");

// ---------------------------------------------------------------------------
// BVH build result
// ---------------------------------------------------------------------------
struct BvhTree {
    std::vector<BvhNode>     nodes;
    std::vector<TriangleCpu> sorted_tris;  // reordered by BVH leaf grouping
};

// ---------------------------------------------------------------------------
// Math primitives
//
// WHY free functions instead of lambdas: the original code used local lambdas
// nested inside bvh_self_test and moller_trumbore, causing triplication across
// modules. Free functions are reusable from any translation unit without copies.
// ---------------------------------------------------------------------------

// Component-wise vector subtraction.
inline std::array<float,3> sub3(const std::array<float,3>& a,
                                 const std::array<float,3>& b) {
    return {a[0]-b[0], a[1]-b[1], a[2]-b[2]};
}

// 3D cross product.
inline std::array<float,3> cross3(const std::array<float,3>& a,
                                   const std::array<float,3>& b) {
    return {a[1]*b[2]-a[2]*b[1], a[2]*b[0]-a[0]*b[2], a[0]*b[1]-a[1]*b[0]};
}

// 3D dot product.
inline float dot3(const std::array<float,3>& a, const std::array<float,3>& b) {
    return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];
}

// Safe reciprocal — returns max float for near-zero inputs.
// WHY: AABB slab intersection uses 1/ray_dir; a zero component must yield
// +inf (not NaN) so the slab interval collapses correctly to [-inf,+inf].
inline float safe_rcp(float v) {
    return (std::fabs(v) < 1e-12f) ? std::numeric_limits<float>::max() : 1.0f / v;
}

// Möller–Trumbore triangle intersection (CPU reference implementation).
// Returns t > 0 on hit, -1.0 on miss.
// WHY this algorithm: matches the GPU kernel's intersection test so the CPU
// self-test exercises the same logical path the GPU will follow.
inline float moller_trumbore(const std::array<float,3>& orig,
                               const std::array<float,3>& dir,
                               const TriangleCpu&          tri)
{
    const auto& v0 = tri.v[0]; const auto& v1 = tri.v[1]; const auto& v2 = tri.v[2];
    auto edge1 = sub3(v1, v0);
    auto edge2 = sub3(v2, v0);
    auto h     = cross3(dir, edge2);
    float det  = dot3(edge1, h);
    if (std::fabs(det) < 1e-8f) return -1.0f;
    float inv_det = 1.0f / det;
    auto  s   = sub3(orig, v0);
    float u   = dot3(s, h) * inv_det;
    if (u < 0.0f || u > 1.0f) return -1.0f;
    auto  q   = cross3(s, edge1);
    float v   = dot3(dir, q) * inv_det;
    if (v < 0.0f || u + v > 1.0f) return -1.0f;
    float t   = dot3(edge2, q) * inv_det;
    return (t > 1e-4f) ? t : -1.0f;
}
