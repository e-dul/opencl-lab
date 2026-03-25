#pragma once
// bvh_builder.hpp — CPU SAH-BVH builder for triangle meshes.
//
// Produces a flat BvhNode[] array with precomputed hit_link / miss_link indices
// for stackless iterative traversal on the GPU. Nodes are ordered depth-first
// (left child first) so traversal is sequential in memory for most rays.
//
// Algorithm: recursive median-split SAH approximation. Each step finds the
// axis with maximum AABB extent and splits at the centroid median. Good for
// ~70k triangles (bunny) with O(N log N) build time.
//
// Shared types (Aabb, TriangleCpu, BvhNode, BvhTree) and math helpers
// (sub3, cross3, dot3, safe_rcp, moller_trumbore) live in bvh_utils.hpp.

#include "bvh_utils.hpp"

#include <algorithm>
#include <numeric>
#include <stdexcept>
#include <vector>

// ---------------------------------------------------------------------------
// Internal build helpers
// ---------------------------------------------------------------------------
namespace bvh_detail {

// Recursive builder: builds nodes for indices[first..last).
// Returns the index of the node just written into `nodes`.
// miss_if_miss: the node index to jump to if the root of this subtree is missed.
//
// WHY index array: nth_element swaps 4-byte ints instead of 76-byte TriangleCpu
// structs — ~19x less data movement per swap, dramatically reducing cache pressure.
// WHY precomputed aabbs/centroids: avoids re-deriving them inside the comparator
// (O(N log N) invocations) and during AABB union (O(N log N) struct reads replaced
// by 12-byte Aabb reads).
static int build_recursive(std::vector<BvhNode>&                    nodes,
                            std::vector<int>&                        indices,
                            const std::vector<Aabb>&                 tri_aabbs,
                            const std::vector<std::array<float,3>>& tri_cents,
                            int                                      first,
                            int                                      last,    // exclusive
                            int                                      miss_if_miss,
                            int                                      leaf_max = 4)
{
    const int node_idx = static_cast<int>(nodes.size());
    nodes.push_back({});  // reserve slot; will be filled below
    BvhNode& node = nodes.back();

    // Union precomputed per-triangle AABBs — reads 12-byte Aabb, not 76-byte struct
    Aabb aabb;
    for (int i = first; i < last; ++i)
        aabb.expand(tri_aabbs[indices[i]]);
    for (int a = 0; a < 3; ++a) {
        node.aabb_lo[a] = aabb.lo[a];
        node.aabb_hi[a] = aabb.hi[a];
    }
    node.miss_link = miss_if_miss;

    const int count = last - first;

    // ── Leaf ─────────────────────────────────────────────────────────────────
    if (count <= leaf_max) {
        node.tri_start = first;
        node.tri_count = count;
        // hit_link for a leaf points to miss_link (no children to visit)
        node.hit_link  = miss_if_miss;
        return node_idx;
    }

    // ── Interior: find split axis (longest AABB extent) ───────────────────────
    int split_axis = 0;
    float max_extent = 0.0f;
    for (int a = 0; a < 3; ++a) {
        float e = aabb.hi[a] - aabb.lo[a];
        if (e > max_extent) { max_extent = e; split_axis = a; }
    }

    // Median split: sort index array — O(1) centroid lookup, 4-byte int swaps
    int mid = (first + last) / 2;
    std::nth_element(indices.begin() + first, indices.begin() + mid, indices.begin() + last,
        [&](int a, int b) {
            return tri_cents[a][split_axis] < tri_cents[b][split_axis];
        });

    // Interior node: mark as non-leaf
    node.tri_start = -1;
    node.tri_count = 0;

    // Left child is the next node written (depth-first order).
    // Left child's miss_link points to the right child (not yet allocated).
    // We do not know right child's index yet, so we build left child first,
    // then record right child's final index as the left-child miss_link.
    //
    // WHY this ordering: in depth-first order, left-child traversal is
    // sequential in memory, maximising cache efficiency for "hit" rays.

    // Build left child. For its miss_link we temporarily use miss_if_miss;
    // we will patch it after we know the right child's index.
    int left_idx  = build_recursive(nodes, indices, tri_aabbs, tri_cents, first, mid, 0, leaf_max);
    // After build_recursive, nodes may have been reallocated — re-grab ref.
    int right_idx = static_cast<int>(nodes.size());  // right child will be next

    // Scan all nodes written by the left subtree (left_idx .. right_idx-1)
    // and patch placeholder links (0) → right_idx.
    // WHY hit_link for leaves also patched: leaf hit_link == miss_link by
    // invariant; both were set to miss_if_miss (placeholder 0) and must
    // point to the right sibling to avoid re-visiting the root.
    for (int i = left_idx; i < right_idx; ++i) {
        if (nodes[i].miss_link == 0) {
            nodes[i].miss_link = right_idx;
        }
        // Leaf nodes: hit_link == miss_link — keep them in sync.
        if (nodes[i].tri_start >= 0 && nodes[i].hit_link == 0) {
            nodes[i].hit_link = right_idx;
        }
    }

    int right_idx_actual = build_recursive(nodes, indices, tri_aabbs, tri_cents, mid, last, miss_if_miss, leaf_max);
    (void)right_idx_actual;  // == right_idx by construction

    // Now we know the right child's index. Wire up the interior node.
    // Re-acquire ref (nodes may have reallocated during recursion).
    nodes[node_idx].hit_link = left_idx;  // == node_idx + 1 in depth-first order

    return node_idx;
}

} // namespace bvh_detail

// ---------------------------------------------------------------------------
// Public API: build SAH-BVH from triangle soup.
// leaf_max: maximum triangles per leaf (default 4 — balances traversal cost).
// ---------------------------------------------------------------------------
inline BvhTree build_bvh(std::vector<TriangleCpu> tris, int leaf_max = 4) {
    if (tris.empty()) throw std::runtime_error("BVH: empty triangle list");

    const int n = static_cast<int>(tris.size());

    // Assign original indices before reordering
    for (int i = 0; i < n; ++i)
        tris[i].original_index = i;

    // Precompute per-triangle AABBs and centroids once.
    // WHY: avoids re-deriving them O(N log N) times inside build_recursive —
    // AABB union reads 12-byte Aabb instead of 76-byte TriangleCpu;
    // centroid comparisons become O(1) array lookups.
    std::vector<Aabb>                tri_aabbs(n);
    std::vector<std::array<float,3>> tri_cents(n);
    for (int i = 0; i < n; ++i) {
        tri_aabbs[i] = tris[i].aabb();
        tri_cents[i] = tris[i].centroid();
    }

    // Build over an index array — nth_element swaps 4-byte ints, not 76-byte structs.
    std::vector<int> indices(n);
    std::iota(indices.begin(), indices.end(), 0);

    BvhTree result;
    result.nodes.reserve(static_cast<size_t>(n) * 2);  // upper bound: 2N-1 nodes

    // Build. Root miss_link = -1 (sentinel: "traversal complete, no hit").
    bvh_detail::build_recursive(result.nodes, indices, tri_aabbs, tri_cents,
                                 0, n, -1, leaf_max);

    // Reorder sorted_tris to match the index order established by nth_element.
    result.sorted_tris.resize(n);
    for (int i = 0; i < n; ++i)
        result.sorted_tris[i] = tris[indices[i]];

    return result;
}

// ---------------------------------------------------------------------------
// CPU self-test: verify BVH traversal logic before GPU dispatch.
//
// Constructs a minimal known scene (one triangle at origin) and fires a ray
// guaranteed to hit it. Asserts the traversal reaches the correct leaf and
// finds an intersection. Throws std::runtime_error if the test fails.
//
// WHY self-test: incorrect miss_link values produce black patches (rays
// terminate early) or infinite loops. Catching this on CPU is far faster to
// debug than inspecting GPU output images.
// ---------------------------------------------------------------------------
inline void bvh_self_test() {
    // Build a scene with 3 triangles.
    // Triangle 0: flat on XZ plane, centred at origin.
    // Triangle 1: offset +X.
    // Triangle 2: offset -X.
    std::vector<TriangleCpu> test_tris(3);
    test_tris[0].v[0] = {-1.0f, 0.0f, -1.0f};
    test_tris[0].v[1] = { 1.0f, 0.0f, -1.0f};
    test_tris[0].v[2] = { 0.0f, 0.0f,  1.0f};
    test_tris[0].n[0] = test_tris[0].n[1] = test_tris[0].n[2] = {0.0f, 1.0f, 0.0f};

    test_tris[1].v[0] = { 5.0f, 0.0f, -1.0f};
    test_tris[1].v[1] = { 7.0f, 0.0f, -1.0f};
    test_tris[1].v[2] = { 6.0f, 0.0f,  1.0f};
    test_tris[1].n[0] = test_tris[1].n[1] = test_tris[1].n[2] = {0.0f, 1.0f, 0.0f};

    test_tris[2].v[0] = {-7.0f, 0.0f, -1.0f};
    test_tris[2].v[1] = {-5.0f, 0.0f, -1.0f};
    test_tris[2].v[2] = {-6.0f, 0.0f,  1.0f};
    test_tris[2].n[0] = test_tris[2].n[1] = test_tris[2].n[2] = {0.0f, 1.0f, 0.0f};

    BvhTree tree = build_bvh(test_tris, 1);  // leaf_max=1 forces interior nodes

    // Verify basic structure
    if (tree.nodes.empty()) throw std::runtime_error("BVH self-test FAILED: no nodes built");

    // Root must have hit_link pointing to a valid child (not itself)
    const BvhNode& root = tree.nodes[0];
    if (root.tri_start < 0 && (root.hit_link <= 0 || root.hit_link >= (int)tree.nodes.size())) {
        throw std::runtime_error("BVH self-test FAILED: root hit_link invalid: " +
                                 std::to_string(root.hit_link));
    }

    // CPU ray-march through BVH to find triangle 0 (ray from above, pointing down).
    // Ray origin: (0, 5, 0), direction: (0, -1, 0)
    std::array<float,3> ro  = {0.0f, 5.0f, 0.0f};
    std::array<float,3> rd  = {0.0f, -1.0f, 0.0f};
    std::array<float,3> rdi = {safe_rcp(rd[0]), safe_rcp(rd[1]), safe_rcp(rd[2])};

    // Stackless BVH traversal (mirrors the GPU kernel logic)
    int   node_idx  = 0;
    int   hit_leaf  = -1;
    float t_min     = 1e30f;

    while (node_idx >= 0 && node_idx < (int)tree.nodes.size()) {
        const BvhNode& n = tree.nodes[node_idx];
        Aabb aabb_n;
        for (int i = 0; i < 3; ++i) { aabb_n.lo[i] = n.aabb_lo[i]; aabb_n.hi[i] = n.aabb_hi[i]; }

        bool hit_box = aabb_n.intersect(ro, rdi, t_min);

        if (hit_box) {
            if (n.tri_start >= 0) {
                // Leaf: test triangles
                for (int i = n.tri_start; i < n.tri_start + n.tri_count; ++i) {
                    float t = moller_trumbore(ro, rd, tree.sorted_tris[i]);
                    if (t > 0.0f && t < t_min) {
                        t_min    = t;
                        hit_leaf = i;
                    }
                }
                node_idx = n.hit_link;  // for leaf, hit_link == miss_link
            } else {
                node_idx = n.hit_link;  // descend into left child
            }
        } else {
            node_idx = n.miss_link;
        }
    }

    if (hit_leaf < 0) {
        throw std::runtime_error("BVH self-test FAILED: ray missed triangle 0 — miss_link bug");
    }

    // Verify the hit is on triangle 0 (original index 0 in sorted_tris)
    // (the sorted_tris may be reordered, but original_index tracks identity)
    int orig_idx = tree.sorted_tris[hit_leaf].original_index;
    if (orig_idx != 0) {
        throw std::runtime_error(
            "BVH self-test FAILED: hit wrong triangle (original_index=" +
            std::to_string(orig_idx) + ", expected 0)");
    }

    // Verify t is approximately 5.0 (ray travels 5 units from y=5 to y=0 plane)
    if (std::fabs(t_min - 5.0f) > 0.01f) {
        throw std::runtime_error(
            "BVH self-test FAILED: hit distance incorrect (t=" +
            std::to_string(t_min) + ", expected ~5.0)");
    }
}

// ---------------------------------------------------------------------------
// GPU SoA buffers packed from a BvhTree.
// WHY SoA: coalesced reads — when all threads in a warp read v0x for different
// triangles, a single cache line fetch covers 16 consecutive float values.
// AoS would interleave v0x, v0y, v0z, v1x, ... forcing 3× more cache lines.
// ---------------------------------------------------------------------------
struct TriangleSoa {
    // Vertex positions (9 floats × N triangles in SoA)
    std::vector<float> v0x, v0y, v0z;
    std::vector<float> v1x, v1y, v1z;
    std::vector<float> v2x, v2y, v2z;
    // Per-vertex normals (9 floats × N)
    std::vector<float> n0x, n0y, n0z;
    std::vector<float> n1x, n1y, n1z;
    std::vector<float> n2x, n2y, n2z;

    int count = 0;
};

inline TriangleSoa build_triangle_soa(const std::vector<TriangleCpu>& tris) {
    TriangleSoa soa;
    soa.count = static_cast<int>(tris.size());
    soa.v0x.reserve(soa.count); soa.v0y.reserve(soa.count); soa.v0z.reserve(soa.count);
    soa.v1x.reserve(soa.count); soa.v1y.reserve(soa.count); soa.v1z.reserve(soa.count);
    soa.v2x.reserve(soa.count); soa.v2y.reserve(soa.count); soa.v2z.reserve(soa.count);
    soa.n0x.reserve(soa.count); soa.n0y.reserve(soa.count); soa.n0z.reserve(soa.count);
    soa.n1x.reserve(soa.count); soa.n1y.reserve(soa.count); soa.n1z.reserve(soa.count);
    soa.n2x.reserve(soa.count); soa.n2y.reserve(soa.count); soa.n2z.reserve(soa.count);

    for (const auto& t : tris) {
        soa.v0x.push_back(t.v[0][0]); soa.v0y.push_back(t.v[0][1]); soa.v0z.push_back(t.v[0][2]);
        soa.v1x.push_back(t.v[1][0]); soa.v1y.push_back(t.v[1][1]); soa.v1z.push_back(t.v[1][2]);
        soa.v2x.push_back(t.v[2][0]); soa.v2y.push_back(t.v[2][1]); soa.v2z.push_back(t.v[2][2]);
        soa.n0x.push_back(t.n[0][0]); soa.n0y.push_back(t.n[0][1]); soa.n0z.push_back(t.n[0][2]);
        soa.n1x.push_back(t.n[1][0]); soa.n1y.push_back(t.n[1][1]); soa.n1z.push_back(t.n[1][2]);
        soa.n2x.push_back(t.n[2][0]); soa.n2y.push_back(t.n[2][1]); soa.n2z.push_back(t.n[2][2]);
    }
    return soa;
}
