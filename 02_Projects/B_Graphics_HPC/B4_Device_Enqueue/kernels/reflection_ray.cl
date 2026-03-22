// reflection_ray.cl — CPU-dispatched multi-bounce reflection kernel.
//
// This kernel is dispatched by the HOST for each bounce in --mode cpu.
// It reads reflected ray data written by the previous pass (primary_ray or
// its own previous invocation) from ray_buf, performs BVH traversal, shades
// the hit, and adds the reflected contribution to the framebuffer.
//
// In --mode gpu (OpenCL 2.0 with device enqueue), this kernel file is still
// compiled but the reflection logic runs via the inline block inside primary_ray.cl.
// Having a separate kernel here allows benchmarking the CPU-dispatch alternative.
//
// WHY separate file: CMake copies all kernels/; having two files makes the
// architectural split (primary vs bounce) explicit for educational purposes.

// ---------------------------------------------------------------------------
// BVH node layout (identical to primary_ray.cl — must match host struct)
// ---------------------------------------------------------------------------
typedef struct {
    float aabb_lo[3];
    float aabb_hi[3];
    int   hit_link;
    int   miss_link;
    int   tri_start;
    int   tri_count;
    int   pad[2];
} BvhNode;

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

static float intersect_triangle(float3 ro, float3 rd,
                                 float3 v0, float3 v1, float3 v2,
                                 float* out_u, float* out_v)
{
    float3 edge1 = v1 - v0;
    float3 edge2 = v2 - v0;
    float3 h     = cross(rd, edge2);
    float  det   = dot(edge1, h);
    if (fabs(det) < 1e-8f) return -1.0f;
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

static float3 safe_normalize(float3 v) {
    float len = sqrt(v.x*v.x + v.y*v.y + v.z*v.z);
    return (len < 1e-8f) ? (float3)(0.0f, 1.0f, 0.0f) : v / len;
}

// ---------------------------------------------------------------------------
// Reflection ray kernel
//
// ray_buf layout per pixel (10 floats — written by primary_ray or prev bounce):
//   [0-2]  ray origin xyz
//   [3-5]  ray direction xyz
//   [6-8]  accumulated attenuated color (carried for multi-bounce blending)
//   [9]    reflectance weight (0 = skip this pixel, not reflective)
//
// Output: adds reflected contribution to framebuffer (additive blend).
//         Updates ray_buf for next bounce (if host dispatches again).
// ---------------------------------------------------------------------------
__kernel void reflection_ray(
    __global float*          framebuffer,
    __global const BvhNode*  nodes,
    int                      num_nodes,
    __global const float*    v0x, __global const float* v0y, __global const float* v0z,
    __global const float*    v1x, __global const float* v1y, __global const float* v1z,
    __global const float*    v2x, __global const float* v2y, __global const float* v2z,
    __global const float*    n0x, __global const float* n0y, __global const float* n0z,
    __global const float*    n1x, __global const float* n1y, __global const float* n1z,
    __global const float*    n2x, __global const float* n2y, __global const float* n2z,
    __global const float*    reflectance,
    __global float*          ray_buf,    // read current bounce, write next bounce
    int                      pixel_count)
{
    size_t gid = get_global_id(0);
    if (gid >= (size_t)pixel_count) return;

    // Read current-bounce ray data
    size_t rb = gid * 10;
    float  weight = ray_buf[rb + 9];

    // WHY early exit: skip pixels that are not on reflective surfaces.
    // Avoids unnecessary BVH traversal for the majority of pixels.
    if (weight <= 0.0f) return;

    float3 ro = (float3)(ray_buf[rb+0], ray_buf[rb+1], ray_buf[rb+2]);
    float3 rd = (float3)(ray_buf[rb+3], ray_buf[rb+4], ray_buf[rb+5]);

    // ── BVH traversal ─────────────────────────────────────────────────────────
    float3 rd_inv = (float3)(
        (fabs(rd.x) < 1e-8f) ? 1e30f : 1.0f / rd.x,
        (fabs(rd.y) < 1e-8f) ? 1e30f : 1.0f / rd.y,
        (fabs(rd.z) < 1e-8f) ? 1e30f : 1.0f / rd.z);

    float t_min   = 1e30f;
    int   hit_tri = -1;
    float hit_u   = 0.0f, hit_v = 0.0f;
    int   node_idx = 0;

    while (node_idx >= 0 && node_idx < num_nodes) {
        __global const BvhNode* n = nodes + node_idx;
        float3 lo = (float3)(n->aabb_lo[0], n->aabb_lo[1], n->aabb_lo[2]);
        float3 hi = (float3)(n->aabb_hi[0], n->aabb_hi[1], n->aabb_hi[2]);

        if (ray_aabb(ro, rd_inv, lo, hi, t_min)) {
            if (n->tri_start >= 0) {
                for (int i = n->tri_start; i < n->tri_start + n->tri_count; ++i) {
                    float3 v0 = (float3)(v0x[i], v0y[i], v0z[i]);
                    float3 v1 = (float3)(v1x[i], v1y[i], v1z[i]);
                    float3 v2 = (float3)(v2x[i], v2y[i], v2z[i]);
                    float u = 0.0f, v = 0.0f;
                    float t = intersect_triangle(ro, rd, v0, v1, v2, &u, &v);
                    if (t > 0.0f && t < t_min) {
                        t_min = t; hit_tri = i; hit_u = u; hit_v = v;
                    }
                }
                node_idx = n->hit_link;
            } else {
                node_idx = n->hit_link;
            }
        } else {
            node_idx = n->miss_link;
        }
    }

    // ── Shading ───────────────────────────────────────────────────────────────
    float3 bounce_color;
    float  next_weight    = 0.0f;
    float3 next_ro        = (float3)(0.0f);
    float3 next_rd        = (float3)(0.0f);

    if (hit_tri >= 0) {
        float3 hit_pt = ro + t_min * rd;
        float  w  = 1.0f - hit_u - hit_v;
        float3 n0 = (float3)(n0x[hit_tri], n0y[hit_tri], n0z[hit_tri]);
        float3 n1 = (float3)(n1x[hit_tri], n1y[hit_tri], n1z[hit_tri]);
        float3 n2 = (float3)(n2x[hit_tri], n2y[hit_tri], n2z[hit_tri]);
        float3 normal = safe_normalize(w * n0 + hit_u * n1 + hit_v * n2);
        if (dot(normal, -rd) < 0.0f) normal = -normal;

        float3 light_pos = (float3)(0.0f, 1.8f, 0.0f);
        float3 L    = safe_normalize(light_pos - hit_pt);
        float  diff = fmax(dot(normal, L), 0.0f);
        float3 view = safe_normalize(-rd);
        float3 half_vec = safe_normalize(L + view);
        float  spec = pow(fmax(dot(normal, half_vec), 0.0f), 32.0f);

        float  refl = reflectance[hit_tri];
        float3 base_col = (refl > 0.0f) ? (float3)(0.8f,0.9f,1.0f) : (float3)(0.85f,0.85f,0.85f);
        float  c = clamp(0.15f + 0.70f * diff + 0.3f * spec, 0.0f, 1.0f);
        bounce_color = base_col * c;

        // Setup next bounce ray (if this surface is also reflective)
        next_weight = refl;
        next_rd     = safe_normalize(rd - 2.0f * dot(rd, normal) * normal);
        next_ro     = hit_pt + normal * 1e-3f;
    } else {
        // Sky
        float t_sky = 0.5f * (rd.y + 1.0f);
        bounce_color = (float3)(1.0f-0.3f*t_sky, 1.0f-0.1f*t_sky, 1.0f);
    }

    // Additive blend into framebuffer (primary color already written by primary_ray)
    size_t fb = gid * 4;
    framebuffer[fb+0] = clamp(framebuffer[fb+0] + bounce_color.x * weight, 0.0f, 1.0f);
    framebuffer[fb+1] = clamp(framebuffer[fb+1] + bounce_color.y * weight, 0.0f, 1.0f);
    framebuffer[fb+2] = clamp(framebuffer[fb+2] + bounce_color.z * weight, 0.0f, 1.0f);

    // Update ray_buf for next CPU-dispatched bounce
    ray_buf[rb+0] = next_ro.x;
    ray_buf[rb+1] = next_ro.y;
    ray_buf[rb+2] = next_ro.z;
    ray_buf[rb+3] = next_rd.x;
    ray_buf[rb+4] = next_rd.y;
    ray_buf[rb+5] = next_rd.z;
    ray_buf[rb+6] = bounce_color.x;
    ray_buf[rb+7] = bounce_color.y;
    ray_buf[rb+8] = bounce_color.z;
    ray_buf[rb+9] = next_weight;
}
