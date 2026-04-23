// traverse.cl — Stack-based ray traversal over an LBVH Karras binary tree.
//
// Each work-item processes one pixel (gx, gy).
// Stack: __local int stack[WG_SIZE][STACK_DEPTH] — per-work-item stack in fast
// shared local memory.
//
// WHY stack (not hit/miss links): the Karras 2012 kernel outputs left_child and
// right_child pointers, not hit/miss link pairs. Converting to hit/miss links
// would require an additional post-processing kernel (Option A in design doc).
// Stack traversal is the natural complement to parent/child topology — it only
// requires STACK_DEPTH entries per work-item, which is small for log2(N) trees.
//
// WG_SIZE and STACK_DEPTH are compile-time constants injected via -D flags from
// lbvh_builder.hpp, sourced from the --wg-size / --stack-depth CLI args.
//
// WHY STACK_DEPTH=32: LBVH over N leaves has depth at most log2(N) for
// uniformly distributed Morton codes. bunny.obj (~70k triangles):
// log2(70000) ≈ 16.1 → depth 17. Stack of 32 is 2× headroom.
// Pathological degenerate inputs (many identical Morton codes) may exceed this —
// documented as a known limitation in RayTracerLBVH.md.

#ifndef WG_SIZE
#define WG_SIZE 64
#endif
#ifndef STACK_DEPTH
#define STACK_DEPTH 32
#endif

// Must match lbvh_types.hpp LbvhNode layout (48 bytes)
typedef struct {
    int   left_child;
    int   right_child;
    int   parent;
    int   pad;
    float aabb_lo[3];
    float aabb_lo_pad;
    float aabb_hi[3];
    float aabb_hi_pad;
} LbvhNode;

// ---------------------------------------------------------------------------
// Ray-AABB slab test.
// WHY precomputed rd_inv: avoids 3 divisions per node (3 multiplies instead).
// Returns true when a hit exists with t in [0, t_max].
// ---------------------------------------------------------------------------
static bool ray_aabb(float3 ro, float3 rd_inv, float3 lo, float3 hi, float t_max) {
    float3 t0 = (lo - ro) * rd_inv;
    float3 t1 = (hi - ro) * rd_inv;
    float3 tmin3 = fmin(t0, t1);
    float3 tmax3 = fmax(t0, t1);
    float  tmin  = fmax(fmax(tmin3.x, tmin3.y), fmax(tmin3.z, 0.0f));
    float  tmax  = fmin(fmin(tmax3.x, tmax3.y), fmin(tmax3.z, t_max));
    return tmin <= tmax;
}

// ---------------------------------------------------------------------------
// Möller–Trumbore triangle intersection.
// Returns t > 0 on hit, -1 on miss.
// WHY back-face culling: discards ~50% of triangles at ~0 cost.
// bunny.obj is a closed mesh — back-face culling is safe.
// ---------------------------------------------------------------------------
static float intersect_triangle(float3 ro, float3 rd,
                                 float3 v0, float3 v1, float3 v2,
                                 float* out_u, float* out_v)
{
    float3 edge1 = v1 - v0;
    float3 edge2 = v2 - v0;
    float3 h     = cross(rd, edge2);
    float  det   = dot(edge1, h);
    if (det < 1e-8f) return -1.0f;  // back-face or parallel
    float  inv_det = 1.0f / det;
    float3 s = ro - v0;
    float  u = dot(s, h) * inv_det;
    if (u < 0.0f || u > 1.0f) return -1.0f;
    float3 q = cross(s, edge1);
    float  v = dot(rd, q) * inv_det;
    if (v < 0.0f || u + v > 1.0f) return -1.0f;
    float t = dot(edge2, q) * inv_det;
    if (t < 1e-4f) return -1.0f;
    *out_u = u; *out_v = v;
    return t;
}

static float3 safe_normalize(float3 v) {
    float len = sqrt(v.x*v.x + v.y*v.y + v.z*v.z);
    return (len < 1e-8f) ? (float3)(0.0f, 1.0f, 0.0f) : v / len;
}

// ---------------------------------------------------------------------------
// LBVH stack-based ray traversal kernel
// ---------------------------------------------------------------------------
__kernel void traverse_lbvh(
    __global float*            framebuffer,  // [width * height * 4] RGBA floats
    __global const LbvhNode*   nodes,        // [2N-1]: LBVH node array
    __global const uint*       sorted_idx,   // [N]: sorted triangle indices
    // SoA vertex positions
    __global const float* v0x, __global const float* v0y, __global const float* v0z,
    __global const float* v1x, __global const float* v1y, __global const float* v1z,
    __global const float* v2x, __global const float* v2y, __global const float* v2z,
    // SoA vertex normals
    __global const float* n0x, __global const float* n0y, __global const float* n0z,
    __global const float* n1x, __global const float* n1y, __global const float* n1z,
    __global const float* n2x, __global const float* n2y, __global const float* n2z,
    int   N,            // number of triangles
    int   width,
    int   height,
    float cam_pos_x,    float cam_pos_y,    float cam_pos_z,
    float cam_target_x, float cam_target_y, float cam_target_z,
    float fov_deg,
    // Per-work-item traversal stack in local memory.
    // WHY __local: local memory is ~100× faster than global. Stack accesses
    // are irregular (push/pop) so caching in global would thrash L1.
    __local int* lstack)   // [WG_SIZE * STACK_DEPTH]: caller provides
{
    size_t gx = get_global_id(0);
    size_t gy = get_global_id(1);
    if ((int)gx >= width || (int)gy >= height) return;

    // Per-work-item stack slice: lstack[lid * STACK_DEPTH .. (lid+1)*STACK_DEPTH - 1]
    size_t lid = get_local_id(0) + get_local_id(1) * get_local_size(0);
    __local int* my_stack = lstack + lid * STACK_DEPTH;
    int stack_top = 0;

    // ── Camera setup ──────────────────────────────────────────────────────────
    float3 cam_pos    = (float3)(cam_pos_x,    cam_pos_y,    cam_pos_z);
    float3 cam_target = (float3)(cam_target_x, cam_target_y, cam_target_z);
    float3 world_up   = (float3)(0.0f, 1.0f, 0.0f);

    float3 fwd   = safe_normalize(cam_target - cam_pos);
    float3 right = safe_normalize(cross(fwd, world_up));
    float3 up    = cross(right, fwd);

    float aspect = (float)width / (float)height;
    float half_h = tan(fov_deg * 0.5f * 3.14159265358979f / 180.0f);
    float half_w = aspect * half_h;

    float ndc_x = (2.0f * ((float)gx + 0.5f) / (float)width  - 1.0f) * half_w;
    float ndc_y = (1.0f - 2.0f * ((float)gy + 0.5f) / (float)height) * half_h;

    float3 rd     = safe_normalize(fwd + ndc_x * right + ndc_y * up);
    float3 rd_inv = (float3)(
        (fabs(rd.x) < 1e-8f) ? 1e30f : 1.0f / rd.x,
        (fabs(rd.y) < 1e-8f) ? 1e30f : 1.0f / rd.y,
        (fabs(rd.z) < 1e-8f) ? 1e30f : 1.0f / rd.z);

    // ── Stack-based LBVH traversal ────────────────────────────────────────────
    float t_min   = 1e30f;
    int   hit_tri = -1;
    float hit_u   = 0.0f, hit_v = 0.0f;

    // Push root
    my_stack[stack_top++] = 0;

    while (stack_top > 0) {
        int node_idx = my_stack[--stack_top];
        __global const LbvhNode* n = nodes + node_idx;

        float3 lo = (float3)(n->aabb_lo[0], n->aabb_lo[1], n->aabb_lo[2]);
        float3 hi = (float3)(n->aabb_hi[0], n->aabb_hi[1], n->aabb_hi[2]);

        if (!ray_aabb(cam_pos, rd_inv, lo, hi, t_min)) continue;

        // Is this a leaf? Leaf nodes are [N-1 .. 2N-2]
        if (node_idx >= N - 1) {
            int leaf_slot = node_idx - (N - 1);
            int tri_idx   = (int)sorted_idx[leaf_slot];

            float3 v0 = (float3)(v0x[tri_idx], v0y[tri_idx], v0z[tri_idx]);
            float3 v1 = (float3)(v1x[tri_idx], v1y[tri_idx], v1z[tri_idx]);
            float3 v2 = (float3)(v2x[tri_idx], v2y[tri_idx], v2z[tri_idx]);

            float u = 0.0f, v = 0.0f;
            float t = intersect_triangle(cam_pos, rd, v0, v1, v2, &u, &v);
            if (t > 0.0f && t < t_min) {
                t_min   = t;
                hit_tri = tri_idx;
                hit_u   = u;
                hit_v   = v;
            }
        } else {
            // Internal node: push children. WHY right first, left second:
            // left child is popped first (LIFO) → left-first traversal,
            // which tends to visit spatially closer nodes sooner for front-to-back rays.
            if (stack_top < STACK_DEPTH - 1) {
                if (n->right_child >= 0) my_stack[stack_top++] = n->right_child;
                if (n->left_child  >= 0) my_stack[stack_top++] = n->left_child;
            }
            // WHY no overflow guard: STACK_DEPTH=32 comfortably covers LBVH
            // depth ≈ log2(70k) ≈ 17 for bunny.obj. See RayTracerLBVH.md gotchas
            // for adversarial input caveats.
        }
    }

    // ── Shading ───────────────────────────────────────────────────────────────
    float r, g, b, a;

    if (hit_tri >= 0) {
        float3 hit_pt = cam_pos + t_min * rd;
        float  w      = 1.0f - hit_u - hit_v;
        float3 n0 = (float3)(n0x[hit_tri], n0y[hit_tri], n0z[hit_tri]);
        float3 n1 = (float3)(n1x[hit_tri], n1y[hit_tri], n1z[hit_tri]);
        float3 n2 = (float3)(n2x[hit_tri], n2y[hit_tri], n2z[hit_tri]);
        float3 normal = safe_normalize(w * n0 + hit_u * n1 + hit_v * n2);
        if (dot(normal, -rd) < 0.0f) normal = -normal;

        float3 light_pos = (float3)(2.0f, 5.0f, 2.0f);
        float3 L         = safe_normalize(light_pos - hit_pt);
        float  diff      = fmax(dot(normal, L), 0.0f);

        float3 view     = safe_normalize(cam_pos - hit_pt);
        float3 half_vec = safe_normalize(L + view);
        float  spec     = pow(fmax(dot(normal, half_vec), 0.0f), 32.0f);

        float c = clamp(0.15f + 0.75f * diff + 0.4f * spec, 0.0f, 1.0f);
        r = c; g = c; b = c; a = 1.0f;
    } else {
        // Sky gradient
        float t_sky = 0.5f * (rd.y + 1.0f);
        r = 1.0f - 0.7f * t_sky;
        g = 1.0f - 0.4f * t_sky;
        b = 1.0f;
        a = 1.0f;
    }

    // Write to flat RGBA float buffer (not image2d_t — allows size_t indexing)
    size_t pixel_idx = (gy * (size_t)width + gx) * 4;
    framebuffer[pixel_idx + 0] = r;
    framebuffer[pixel_idx + 1] = g;
    framebuffer[pixel_idx + 2] = b;
    framebuffer[pixel_idx + 3] = a;
}
