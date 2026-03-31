// ray_trace_bvh.cl — Stackless BVH ray tracer kernel.
//
// Each work item processes one pixel (gx, gy).
// Triangle data is in SoA layout for coalesced memory access.
// BVH nodes are in a flat array with hit_link / miss_link for stackless traversal.
//
// Camera args passed as kernel args (not #define) to support interactive orbit/zoom/pan.

// ---------------------------------------------------------------------------
// BVH node layout (must match host BvhNode struct exactly)
// ---------------------------------------------------------------------------
// WHY no __attribute__((packed)): explicit pad[2] achieves the 48-byte natural
// alignment — packing would be redundant and diverges from the host struct layout.
typedef struct {
    float aabb_lo[3];
    float aabb_hi[3];
    int   hit_link;    // node idx to visit on AABB hit
    int   miss_link;   // node idx to visit on AABB miss
    int   tri_start;   // -1 for interior nodes
    int   tri_count;   // 0 for interior nodes
    int   pad[2];
} BvhNode;

// ---------------------------------------------------------------------------
// Ray-AABB slab test.
// WHY precomputed rd_inv: avoids 3 divisions per node (replaced by 3 muls).
// Returns true when hit distance t_hit <= t_max.
// ---------------------------------------------------------------------------
static bool ray_aabb(float3 ro, float3 rd_inv,
                     float3 lo, float3 hi, float t_max)
{
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
// Returns t > 0 on hit, -1 on miss. barycentric u, v written on hit.
// ---------------------------------------------------------------------------
static float intersect_triangle(float3 ro, float3 rd,
                                  float3 v0, float3 v1, float3 v2,
                                  float* out_u, float* out_v)
{
    float3 edge1 = v1 - v0;
    float3 edge2 = v2 - v0;
    float3 h     = cross(rd, edge2);
    float  det   = dot(edge1, h);

    // WHY culling: back-face culling discards ~50% of triangles,
    // nearly doubling ray throughput at the cost of single-sided geometry.
    // Bunny is closed so back-face culling is safe.
    if (det < 1e-8f) return -1.0f;

    float  inv_det = 1.0f / det;
    float3 s       = ro - v0;
    float  u       = dot(s, h) * inv_det;
    if (u < 0.0f || u > 1.0f) return -1.0f;

    float3 q = cross(s, edge1);
    float  v = dot(rd, q) * inv_det;
    if (v < 0.0f || u + v > 1.0f) return -1.0f;

    float t = dot(edge2, q) * inv_det;
    if (t < 1e-4f) return -1.0f;

    *out_u = u;
    *out_v = v;
    return t;
}

// ---------------------------------------------------------------------------
// Safe normalize (guards against zero-length vectors from degenerate geometry).
// ---------------------------------------------------------------------------
static float3 safe_normalize(float3 v) {
    float len = sqrt(v.x*v.x + v.y*v.y + v.z*v.z);
    return (len < 1e-8f) ? (float3)(0.0f, 1.0f, 0.0f) : v / len;
}

// ---------------------------------------------------------------------------
// BVH ray tracer kernel
//
// Arg names are self-documenting — see the parameter list below.
// SoA layout: each triangle component is a separate float array for coalesced
// access across the warp (avoids AoS stride penalty on triangle vertex reads).
// ---------------------------------------------------------------------------
__kernel void ray_trace_bvh(
    __write_only image2d_t framebuffer,
    __global const BvhNode* nodes,
    // SoA vertex positions
    __global const float* v0x, __global const float* v0y, __global const float* v0z,
    __global const float* v1x, __global const float* v1y, __global const float* v1z,
    __global const float* v2x, __global const float* v2y, __global const float* v2z,
    // SoA vertex normals
    __global const float* n0x, __global const float* n0y, __global const float* n0z,
    __global const float* n1x, __global const float* n1y, __global const float* n1z,
    __global const float* n2x, __global const float* n2y, __global const float* n2z,
    int   num_tris,
    int   num_nodes,
    int   width,
    int   height,
    float cam_pos_x,    float cam_pos_y,    float cam_pos_z,
    float cam_target_x, float cam_target_y, float cam_target_z,
    float fov_deg)
{
    size_t gx = get_global_id(0);
    size_t gy = get_global_id(1);
    if (gx >= (size_t)width || gy >= (size_t)height) return;

    // ── Camera basis ──────────────────────────────────────────────────────────
    float3 cam_pos    = (float3)(cam_pos_x,    cam_pos_y,    cam_pos_z);
    float3 cam_target = (float3)(cam_target_x, cam_target_y, cam_target_z);
    float3 world_up   = (float3)(0.0f, 1.0f, 0.0f);

    float3 fwd   = safe_normalize(cam_target - cam_pos);
    float3 right = safe_normalize(cross(fwd, world_up));
    float3 up    = cross(right, fwd);  // already normalised if fwd and right are

    // ── Ray direction ─────────────────────────────────────────────────────────
    float aspect = (float)width / (float)height;
    float half_h = tan(fov_deg * 0.5f * 3.14159265358979f / 180.0f);
    float half_w = aspect * half_h;

    float ndc_x = (2.0f * ((float)gx + 0.5f) / (float)width  - 1.0f) * half_w;
    float ndc_y = (1.0f - 2.0f * ((float)gy + 0.5f) / (float)height) * half_h;

    float3 rd     = safe_normalize(fwd + ndc_x * right + ndc_y * up);
    // WHY precomputed reciprocal: AABB slab test uses multiply instead of divide.
    // Guard against near-zero components to avoid +/-Inf that breaks slab test.
    float3 rd_inv = (float3)(
        (fabs(rd.x) < 1e-8f) ? 1e30f : 1.0f / rd.x,
        (fabs(rd.y) < 1e-8f) ? 1e30f : 1.0f / rd.y,
        (fabs(rd.z) < 1e-8f) ? 1e30f : 1.0f / rd.z);

    // ── Stackless BVH traversal ───────────────────────────────────────────────
    // WHY stackless: GPU threads have no call stack. hit_link / miss_link replace
    // recursive descent with O(1) integer loads — canonical production BVH trick.
    float t_min   = 1e30f;
    int   hit_tri = -1;
    float hit_u   = 0.0f, hit_v = 0.0f;

    int node_idx = 0;
    // WHY upper bound: guards against corrupted BVH links producing out-of-bounds
    // reads or infinite loops on pathological inputs.
    while (node_idx >= 0 && node_idx < num_nodes) {
        __global const BvhNode* n = nodes + node_idx;

        float3 lo = (float3)(n->aabb_lo[0], n->aabb_lo[1], n->aabb_lo[2]);
        float3 hi = (float3)(n->aabb_hi[0], n->aabb_hi[1], n->aabb_hi[2]);

        if (ray_aabb(cam_pos, rd_inv, lo, hi, t_min)) {
            if (n->tri_start >= 0) {
                // Leaf: test triangles in this leaf
                for (int i = n->tri_start; i < n->tri_start + n->tri_count; ++i) {
                    float3 v0 = (float3)(v0x[i], v0y[i], v0z[i]);
                    float3 v1 = (float3)(v1x[i], v1y[i], v1z[i]);
                    float3 v2 = (float3)(v2x[i], v2y[i], v2z[i]);
                    float u = 0.0f, v = 0.0f;
                    float t = intersect_triangle(cam_pos, rd, v0, v1, v2, &u, &v);
                    if (t > 0.0f && t < t_min) {
                        t_min   = t;
                        hit_tri = i;
                        hit_u   = u;
                        hit_v   = v;
                    }
                }
                node_idx = n->hit_link;  // for leaf, hit_link == miss_link
            } else {
                node_idx = n->hit_link;  // descend into left child
            }
        } else {
            node_idx = n->miss_link;
        }
    }

    // ── Shading ───────────────────────────────────────────────────────────────
    float4 color;

    if (hit_tri >= 0) {
        float3 hit_pt = cam_pos + t_min * rd;

        // Interpolate normal using barycentric coordinates (u, v, 1-u-v).
        float w = 1.0f - hit_u - hit_v;
        float3 n0 = (float3)(n0x[hit_tri], n0y[hit_tri], n0z[hit_tri]);
        float3 n1 = (float3)(n1x[hit_tri], n1y[hit_tri], n1z[hit_tri]);
        float3 n2 = (float3)(n2x[hit_tri], n2y[hit_tri], n2z[hit_tri]);
        float3 normal = safe_normalize(w * n0 + hit_u * n1 + hit_v * n2);

        // Ensure normal faces the camera (handle degenerate winding)
        if (dot(normal, -rd) < 0.0f) normal = -normal;

        // Fixed point light above-right of scene
        float3 light_pos = (float3)(2.0f, 5.0f, 2.0f);
        float3 L = safe_normalize(light_pos - hit_pt);

        float diff = fmax(dot(normal, L), 0.0f);

        // Specular: Blinn-Phong half-vector
        float3 view = safe_normalize(cam_pos - hit_pt);
        float3 half_vec = safe_normalize(L + view);
        float spec = pow(fmax(dot(normal, half_vec), 0.0f), 32.0f);

        // Grey material (bunny is colourless) with Phong coefficients
        float ambient  = 0.15f;
        float diffuse  = 0.75f;
        float specular = 0.4f;
        float c = clamp(ambient + diffuse * diff + specular * spec, 0.0f, 1.0f);
        color = (float4)(c, c, c, 1.0f);
    } else {
        // Sky: vertical gradient
        float t_sky = 0.5f * (rd.y + 1.0f);
        float3 sky  = (float3)(1.0f - 0.7f * t_sky,
                                1.0f - 0.4f * t_sky,
                                1.0f);
        color = (float4)(sky.x, sky.y, sky.z, 1.0f);
    }

    write_imagef(framebuffer, (int2)((int)gx, (int)gy), color);
}

// ---------------------------------------------------------------------------
// Naive brute-force kernel (for timing comparison vs BVH).
// Same shading, O(N) triangle loop — no BVH.
// ---------------------------------------------------------------------------
__kernel void ray_trace_naive(
    __write_only image2d_t framebuffer,
    // SoA vertex positions
    __global const float* v0x, __global const float* v0y, __global const float* v0z,
    __global const float* v1x, __global const float* v1y, __global const float* v1z,
    __global const float* v2x, __global const float* v2y, __global const float* v2z,
    // SoA vertex normals
    __global const float* n0x, __global const float* n0y, __global const float* n0z,
    __global const float* n1x, __global const float* n1y, __global const float* n1z,
    __global const float* n2x, __global const float* n2y, __global const float* n2z,
    int   num_tris,
    int   width,
    int   height,
    float cam_pos_x,    float cam_pos_y,    float cam_pos_z,
    float cam_target_x, float cam_target_y, float cam_target_z,
    float fov_deg)
{
    size_t gx = get_global_id(0);
    size_t gy = get_global_id(1);
    if (gx >= (size_t)width || gy >= (size_t)height) return;

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

    float3 rd = safe_normalize(fwd + ndc_x * right + ndc_y * up);

    float t_min   = 1e30f;
    int   hit_tri = -1;
    float hit_u   = 0.0f, hit_v = 0.0f;

    // O(N) brute force
    for (int i = 0; i < num_tris; ++i) {
        float3 v0 = (float3)(v0x[i], v0y[i], v0z[i]);
        float3 v1 = (float3)(v1x[i], v1y[i], v1z[i]);
        float3 v2 = (float3)(v2x[i], v2y[i], v2z[i]);
        float u = 0.0f, v = 0.0f;
        float t = intersect_triangle(cam_pos, rd, v0, v1, v2, &u, &v);
        if (t > 0.0f && t < t_min) {
            t_min   = t;
            hit_tri = i;
            hit_u   = u;
            hit_v   = v;
        }
    }

    float4 color;
    if (hit_tri >= 0) {
        float3 hit_pt = cam_pos + t_min * rd;
        float  w = 1.0f - hit_u - hit_v;
        float3 n0 = (float3)(n0x[hit_tri], n0y[hit_tri], n0z[hit_tri]);
        float3 n1 = (float3)(n1x[hit_tri], n1y[hit_tri], n1z[hit_tri]);
        float3 n2 = (float3)(n2x[hit_tri], n2y[hit_tri], n2z[hit_tri]);
        float3 normal = safe_normalize(w * n0 + hit_u * n1 + hit_v * n2);
        if (dot(normal, -rd) < 0.0f) normal = -normal;
        float3 light_pos = (float3)(2.0f, 5.0f, 2.0f);
        float3 L = safe_normalize(light_pos - hit_pt);
        float diff = fmax(dot(normal, L), 0.0f);
        float3 view = safe_normalize(cam_pos - hit_pt);
        float3 half_vec = safe_normalize(L + view);
        float spec = pow(fmax(dot(normal, half_vec), 0.0f), 32.0f);
        float c = clamp(0.15f + 0.75f * diff + 0.4f * spec, 0.0f, 1.0f);
        color = (float4)(c, c, c, 1.0f);
    } else {
        float t_sky = 0.5f * (rd.y + 1.0f);
        color = (float4)(1.0f - 0.7f * t_sky, 1.0f - 0.4f * t_sky, 1.0f, 1.0f);
    }

    write_imagef(framebuffer, (int2)((int)gx, (int)gy), color);
}
