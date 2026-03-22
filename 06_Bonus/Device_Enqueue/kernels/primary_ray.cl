// primary_ray.cl — B4 Device Enqueue primary + multi-bounce ray tracer.
//
// ARCHITECTURE:
//   CPU-dispatched mode: host dispatches this kernel for primary rays, then
//   dispatches reflection_ray.cl for each bounce (explicit host round-trip).
//
//   GPU-spawned mode (OpenCL 2.0 only): primary_ray calls enqueue_kernel with
//   a block that performs the reflection pass inline on the GPU. No host
//   round-trip between bounces — the GPU schedules its own follow-up work.
//
// WHY 1D global: one work item = one pixel. Simpler index math than 2D for the
// device-enqueue path where spawned kernels also need a 1D pixel index.

// ---------------------------------------------------------------------------
// BVH node layout (must match host BvhNode struct: 48 bytes)
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

// ---------------------------------------------------------------------------
// Ray-AABB slab test with precomputed reciprocal direction.
// WHY precomputed rd_inv: replaces 3 divisions per node with 3 multiplies.
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
// Möller–Trumbore triangle intersection. Returns t > 0 on hit, -1 on miss.
// WHY no back-face culling: Cornell box has inward-facing walls; culling
// back-faces would make all walls invisible.
// ---------------------------------------------------------------------------
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
// Stackless BVH traversal (mirrors CPU self-test logic in bvh_builder.hpp).
// Returns hit triangle index (-1 on miss); writes t, u, v on hit.
// ---------------------------------------------------------------------------
static int bvh_traverse(
    float3 ro, float3 rd,
    __global const BvhNode* nodes, int num_nodes,
    __global const float* v0x, __global const float* v0y, __global const float* v0z,
    __global const float* v1x, __global const float* v1y, __global const float* v1z,
    __global const float* v2x, __global const float* v2y, __global const float* v2z,
    float* t_out, float* u_out, float* v_out)
{
    float3 rd_inv = (float3)(
        (fabs(rd.x) < 1e-8f) ? 1e30f : 1.0f / rd.x,
        (fabs(rd.y) < 1e-8f) ? 1e30f : 1.0f / rd.y,
        (fabs(rd.z) < 1e-8f) ? 1e30f : 1.0f / rd.z);

    float t_min   = 1e30f;
    int   hit_tri = -1;
    float hit_u = 0.0f, hit_v = 0.0f;
    int node_idx = 0;

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
    *t_out = t_min; *u_out = hit_u; *v_out = hit_v;
    return hit_tri;
}

// ---------------------------------------------------------------------------
// Primary ray kernel — dispatched by host for every pixel.
//
// Args 0..N follow the kernel signature below.
// Output: framebuffer RGBA float (width*height*4), ray_buf for CPU bounce path.
//
// ray_buf layout per pixel (10 floats):
//   [0-2] reflected ray origin (xyz)
//   [3-5] reflected ray direction (xyz)
//   [6-8] accumulated attenuated color (rgb, 0 if not reflective)
//   [9]   reflectance weight (0 = not reflective, skip in reflection pass)
// ---------------------------------------------------------------------------
__kernel void primary_ray(
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
    __global float*          ray_buf,
    int                      width,
    int                      height,
    float                    cam_pos_x,    float cam_pos_y,    float cam_pos_z,
    float                    cam_target_x, float cam_target_y, float cam_target_z,
    float                    fov_deg,
    int                      max_bounces)
{
    size_t gid = get_global_id(0);
    if (gid >= (size_t)width * (size_t)height) return;

    int gx = (int)(gid % (size_t)width);
    int gy = (int)(gid / (size_t)width);

    // ── Camera basis ──────────────────────────────────────────────────────────
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

    float3 ro = cam_pos;
    float3 rd = safe_normalize(fwd + ndc_x * right + ndc_y * up);

    // ── BVH traversal ─────────────────────────────────────────────────────────
    float t_hit = 1e30f, u_hit = 0.0f, v_hit = 0.0f;
    int hit_tri = bvh_traverse(ro, rd, nodes, num_nodes,
                               v0x, v0y, v0z, v1x, v1y, v1z, v2x, v2y, v2z,
                               &t_hit, &u_hit, &v_hit);

    // ── Shading ───────────────────────────────────────────────────────────────
    float3 color;
    float  refl_weight = 0.0f;
    float3 reflect_origin = (float3)(0.0f);
    float3 reflect_dir    = (float3)(0.0f);

    if (hit_tri >= 0) {
        float3 hit_pt = ro + t_hit * rd;
        float  w  = 1.0f - u_hit - v_hit;
        float3 n0 = (float3)(n0x[hit_tri], n0y[hit_tri], n0z[hit_tri]);
        float3 n1 = (float3)(n1x[hit_tri], n1y[hit_tri], n1z[hit_tri]);
        float3 n2 = (float3)(n2x[hit_tri], n2y[hit_tri], n2z[hit_tri]);
        float3 normal = safe_normalize(w * n0 + u_hit * n1 + v_hit * n2);
        if (dot(normal, -rd) < 0.0f) normal = -normal;

        float3 light_pos = (float3)(0.0f, 1.8f, 0.0f);
        float3 L = safe_normalize(light_pos - hit_pt);
        float  diff = fmax(dot(normal, L), 0.0f);
        float3 view = safe_normalize(-rd);
        float3 half_vec = safe_normalize(L + view);
        float  spec = pow(fmax(dot(normal, half_vec), 0.0f), 32.0f);

        refl_weight = reflectance[hit_tri];
        // WHY offset: nudge reflected ray origin along normal to avoid re-hitting
        // the same surface (self-intersection artifact).
        reflect_dir    = safe_normalize(rd - 2.0f * dot(rd, normal) * normal);
        reflect_origin = hit_pt + normal * 1e-3f;

        float3 base_col = (refl_weight > 0.0f)
            ? (float3)(0.8f, 0.9f, 1.0f)   // reflective: slight blue tint
            : (float3)(0.85f, 0.85f, 0.85f); // diffuse: grey
        float c = clamp(0.15f + 0.70f * diff + 0.3f * spec, 0.0f, 1.0f);
        color = base_col * c;

        // Attenuate primary contribution where reflection will add on top
        color = color * (1.0f - refl_weight);
    } else {
        // Sky gradient (no reflection)
        float t_sky = 0.5f * (rd.y + 1.0f);
        color = (float3)(1.0f - 0.3f * t_sky, 1.0f - 0.1f * t_sky, 1.0f);
    }

    // Write primary color to framebuffer
    size_t fb = gid * 4;
    framebuffer[fb + 0] = color.x;
    framebuffer[fb + 1] = color.y;
    framebuffer[fb + 2] = color.z;
    framebuffer[fb + 3] = 1.0f;

    // Write reflected ray to ray_buf for CPU-dispatched bounce path
    // WHY always write: CPU path reads this buffer to dispatch the next bounce.
    size_t rb = gid * 10;
    ray_buf[rb + 0] = reflect_origin.x;
    ray_buf[rb + 1] = reflect_origin.y;
    ray_buf[rb + 2] = reflect_origin.z;
    ray_buf[rb + 3] = reflect_dir.x;
    ray_buf[rb + 4] = reflect_dir.y;
    ray_buf[rb + 5] = reflect_dir.z;
    ray_buf[rb + 6] = color.x;    // attenuated primary color for additive blend
    ray_buf[rb + 7] = color.y;
    ray_buf[rb + 8] = color.z;
    ray_buf[rb + 9] = refl_weight;

#if __OPENCL_C_VERSION__ >= 200
    // ── OpenCL 2.0: GPU-spawned bounce (device enqueue) ───────────────────────
    // WHY device enqueue: avoids CPU read-back + dispatch latency between bounces.
    // Each primary hit on a reflective surface enqueues ONE block that iterates
    // all bounces internally. This correctly handles any max_bounces value
    // without hard-coding nesting depth.
    //
    // WHY iterative instead of recursive enqueue: recursive enqueue (block
    // enqueuing another block) is depth-limited by driver sub-kernel queue
    // depth and silently truncates at 2 on most implementations. A while-loop
    // inside one block is simpler, correct, and still demonstrates device enqueue.
    if (refl_weight > 0.0f && max_bounces > 0) {
        queue_t   dq = get_default_queue();
        ndrange_t nr = ndrange_1D(1);

        // Capture initial bounce state (private copies; blocks capture by value).
        size_t  pixel_idx = gid;
        int     br        = max_bounces;       // bounces remaining
        float3  b_ro      = reflect_origin;    // current bounce ray origin
        float3  b_rd      = reflect_dir;       // current bounce ray direction
        float   b_weight  = refl_weight;       // accumulated reflectance weight

        // Scene data pointers — captured so the block can call bvh_traverse.
        __global float*          fb_ptr = framebuffer;
        __global const BvhNode*  nd_ptr = nodes;
        int                      n_nodes = num_nodes;
        __global const float*    pv0x = v0x, *pv0y = v0y, *pv0z = v0z;
        __global const float*    pv1x = v1x, *pv1y = v1y, *pv1z = v1z;
        __global const float*    pv2x = v2x, *pv2y = v2y, *pv2z = v2z;
        __global const float*    pn0x = n0x, *pn0y = n0y, *pn0z = n0z;
        __global const float*    pn1x = n1x, *pn1y = n1y, *pn1z = n1z;
        __global const float*    pn2x = n2x, *pn2y = n2y, *pn2z = n2z;
        __global const float*    refl = reflectance;

        // WHY: enqueue_kernel returns an int error code, but device-side errors
        // cannot propagate back to the host — there is no exception mechanism in
        // OpenCL C. Capturing the return and casting to void documents the
        // intentional discard rather than leaving a silent drop.
        int eq_err = enqueue_kernel(dq, CLK_ENQUEUE_FLAGS_WAIT_KERNEL, nr,
        ^{
            // WHY local mutable copies: OpenCL C block captures are read-only
            // (by-value). We need mutable loop variables for the bounce state.
            float3 cur_ro     = b_ro;
            float3 cur_rd     = b_rd;
            float  cur_weight = b_weight;
            int    bounces_rem = br;

            while (bounces_rem > 0) {
                float bt = 1e30f, bu = 0.0f, bv = 0.0f;
                int bh = bvh_traverse(cur_ro, cur_rd, nd_ptr, n_nodes,
                                      pv0x, pv0y, pv0z,
                                      pv1x, pv1y, pv1z,
                                      pv2x, pv2y, pv2z,
                                      &bt, &bu, &bv);

                float3 bounce_color;
                float  next_refl  = 0.0f;
                float3 next_ro    = (float3)(0.0f);
                float3 next_rd    = (float3)(0.0f);

                if (bh >= 0) {
                    float3 hp   = cur_ro + bt * cur_rd;
                    float  bw   = 1.0f - bu - bv;
                    float3 bn0  = (float3)(pn0x[bh], pn0y[bh], pn0z[bh]);
                    float3 bn1  = (float3)(pn1x[bh], pn1y[bh], pn1z[bh]);
                    float3 bn2  = (float3)(pn2x[bh], pn2y[bh], pn2z[bh]);
                    float3 bnrm = safe_normalize(bw*bn0 + bu*bn1 + bv*bn2);
                    if (dot(bnrm, -cur_rd) < 0.0f) bnrm = -bnrm;

                    float3 lp   = (float3)(0.0f, 1.8f, 0.0f);
                    float3 bL   = safe_normalize(lp - hp);
                    float  bd   = fmax(dot(bnrm, bL), 0.0f);
                    float3 bv3  = safe_normalize(-cur_rd);
                    float3 bhv  = safe_normalize(bL + bv3);
                    float  bs   = pow(fmax(dot(bnrm, bhv), 0.0f), 32.0f);

                    float  br2  = refl[bh];
                    float3 bc   = (br2 > 0.0f) ? (float3)(0.8f,0.9f,1.0f)
                                               : (float3)(0.85f,0.85f,0.85f);
                    float  bcs  = clamp(0.15f + 0.70f*bd + 0.3f*bs, 0.0f, 1.0f);
                    bounce_color = bc * bcs;

                    // Prepare next bounce ray if this surface is also reflective.
                    // WHY next_refl * cur_weight: each successive reflection
                    // is attenuated by the product of all prior reflectances.
                    next_refl = br2;
                    next_rd   = safe_normalize(cur_rd - 2.0f * dot(cur_rd, bnrm) * bnrm);
                    next_ro   = hp + bnrm * 1e-3f;
                } else {
                    // Sky hit — terminate the bounce chain.
                    float ts = 0.5f * (cur_rd.y + 1.0f);
                    bounce_color = (float3)(1.0f-0.3f*ts, 1.0f-0.1f*ts, 1.0f);
                }

                // Additive blend into framebuffer.
                // WHY atomic-free: each pixel is processed by exactly one
                // primary work-item, so its enqueued block is the sole writer.
                size_t fb2 = pixel_idx * 4;
                fb_ptr[fb2+0] = clamp(fb_ptr[fb2+0] + bounce_color.x * cur_weight, 0.0f, 1.0f);
                fb_ptr[fb2+1] = clamp(fb_ptr[fb2+1] + bounce_color.y * cur_weight, 0.0f, 1.0f);
                fb_ptr[fb2+2] = clamp(fb_ptr[fb2+2] + bounce_color.z * cur_weight, 0.0f, 1.0f);

                // Advance to next bounce or exit if surface is non-reflective.
                bounces_rem--;
                if (next_refl <= 0.0f) break;
                cur_ro     = next_ro;
                cur_rd     = next_rd;
                cur_weight = cur_weight * next_refl;
            }
        });
        (void)eq_err;
    }
#endif  // __OPENCL_C_VERSION__ >= 200
}
