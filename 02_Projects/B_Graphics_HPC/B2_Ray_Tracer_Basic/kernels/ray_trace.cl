// ray_trace.cl — Single-kernel ray tracer: ray generation + sphere intersection + Phong shading.
//
// Each work item processes one pixel (gx, gy).
// Sphere data layout: [cx, cy, cz, radius, R, G, B, shininess]  — 8 floats per sphere.

// --- Camera (edit here to experiment) ---
#define CAM_POS_X    0.0f
#define CAM_POS_Y    0.0f
#define CAM_POS_Z   -5.0f
#define CAM_TARGET_X 0.0f
#define CAM_TARGET_Y 0.0f
#define CAM_TARGET_Z  0.0f
#define CAM_UP_Y     1.0f   // world up
#define CAM_FOV_DEG  60.0f

// --- Fixed point light ---
#define LIGHT_X  3.0f
#define LIGHT_Y  5.0f
#define LIGHT_Z -3.0f

// --- Shading coefficients ---
#define AMBIENT  0.15f
#define DIFFUSE  0.75f
#define SPECULAR 0.6f

#define PI 3.14159265358979f
#define FLOATS_PER_SPHERE 8

// WHY custom normalize3/dot3 instead of OpenCL builtins normalize()/dot():
// Older AMD/Mesa drivers widen component-wise operations on float3 to float4
// internally, which can produce NaN for the w-component and pollute subsequent
// vector math. Explicit component-wise math avoids that implicit widening path
// entirely. This also makes the arithmetic transparent to students reading the code.
static float3 normalize3(float3 v) {
    float len = sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    // WHY guard: a zero-length vector (degenerate ray or coincident geometry)
    // would cause a divide-by-zero, producing NaN pixels. Return a safe fallback
    // direction (0,0,1) instead of crashing or producing undefined output.
    if (len < 1e-8f) return (float3)(0.0f, 0.0f, 1.0f);
    return (float3)(v.x / len, v.y / len, v.z / len);
}

static float dot3(float3 a, float3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

// Intersect ray(origin + t*dir) with sphere; returns t > 0 on hit, -1 on miss.
// WHY analytic: sphere intersection is a quadratic equation — fast, exact, no mesh needed.
static float intersect_sphere(float3 origin, float3 dir,
                               float3 center, float radius) {
    float3 oc = (float3)(origin.x - center.x,
                         origin.y - center.y,
                         origin.z - center.z);
    float a = dot3(dir, dir);
    float b = 2.0f * dot3(oc, dir);
    float c = dot3(oc, oc) - radius * radius;
    float disc = b * b - 4.0f * a * c;
    if (disc < 0.0f) return -1.0f;
    float sqrt_disc = sqrt(disc);
    float t0 = (-b - sqrt_disc) / (2.0f * a);
    float t1 = (-b + sqrt_disc) / (2.0f * a);
    float t_min = (t0 > 0.001f) ? t0 : ((t1 > 0.001f) ? t1 : -1.0f);
    return t_min;
}

__kernel void ray_trace(__write_only image2d_t framebuffer,
                        __global const float* spheres,
                        int num_spheres,
                        int width,
                        int height)
{
    // WHY size_t: get_global_id returns size_t; using int would cause
    // signed/unsigned comparison warnings on the guard below.
    size_t gx = get_global_id(0);
    size_t gy = get_global_id(1);
    if (gx >= (size_t)width || gy >= (size_t)height) return;

    // ── Build camera basis ───────────────────────────────────────────────────
    float3 cam_pos    = (float3)(CAM_POS_X, CAM_POS_Y, CAM_POS_Z);
    float3 cam_target = (float3)(CAM_TARGET_X, CAM_TARGET_Y, CAM_TARGET_Z);
    float3 world_up   = (float3)(0.0f, CAM_UP_Y, 0.0f);

    float3 forward = normalize3((float3)(cam_target.x - cam_pos.x,
                                         cam_target.y - cam_pos.y,
                                         cam_target.z - cam_pos.z));
    // right = forward × up
    float3 right = normalize3((float3)(
        forward.y * world_up.z - forward.z * world_up.y,
        forward.z * world_up.x - forward.x * world_up.z,
        forward.x * world_up.y - forward.y * world_up.x));
    // camera up = right × forward (recompute to ensure orthogonality)
    float3 up = (float3)(
        right.y * forward.z - right.z * forward.y,
        right.z * forward.x - right.x * forward.z,
        right.x * forward.y - right.y * forward.x);

    // ── Compute ray direction ─────────────────────────────────────────────────
    float aspect = (float)width / (float)height;
    // half_h: tan of half-FOV, maps pixel coords to view-plane coords
    float half_h = tan((CAM_FOV_DEG * 0.5f) * PI / 180.0f);
    float half_w = aspect * half_h;

    // NDC: [-1, 1] with (0,0) at image centre; flip y so +y is up on screen
    float ndc_x = (2.0f * ((float)gx + 0.5f) / (float)width  - 1.0f) * half_w;
    float ndc_y = (1.0f - 2.0f * ((float)gy + 0.5f) / (float)height) * half_h;

    float3 ray_dir = normalize3((float3)(
        forward.x + ndc_x * right.x + ndc_y * up.x,
        forward.y + ndc_x * right.y + ndc_y * up.y,
        forward.z + ndc_x * right.z + ndc_y * up.z));

    // ── Intersect all spheres (brute-force O(N)) ──────────────────────────────
    float  t_min   = 1e30f;
    int    hit_idx = -1;

    for (int i = 0; i < num_spheres; ++i) {
        int base   = i * FLOATS_PER_SPHERE;
        float3 cen = (float3)(spheres[base+0], spheres[base+1], spheres[base+2]);
        float  rad = spheres[base+3];

        float t = intersect_sphere(cam_pos, ray_dir, cen, rad);
        if (t > 0.0f && t < t_min) {
            t_min   = t;
            hit_idx = i;
        }
    }

    // ── Shading ───────────────────────────────────────────────────────────────
    float4 color;

    if (hit_idx >= 0) {
        int    base  = hit_idx * FLOATS_PER_SPHERE;
        float3 cen   = (float3)(spheres[base+0], spheres[base+1], spheres[base+2]);
        float3 albedo= (float3)(spheres[base+4], spheres[base+5], spheres[base+6]);
        float  shin  = spheres[base+7];

        // Hit point and surface normal
        float3 hit_pt = (float3)(cam_pos.x + t_min * ray_dir.x,
                                  cam_pos.y + t_min * ray_dir.y,
                                  cam_pos.z + t_min * ray_dir.z);
        float3 normal = normalize3((float3)(hit_pt.x - cen.x,
                                             hit_pt.y - cen.y,
                                             hit_pt.z - cen.z));

        // Light direction
        float3 light_pos = (float3)(LIGHT_X, LIGHT_Y, LIGHT_Z);
        float3 L = normalize3((float3)(light_pos.x - hit_pt.x,
                                        light_pos.y - hit_pt.y,
                                        light_pos.z - hit_pt.z));

        // Phong: ambient + diffuse + specular
        float diff = fmax(dot3(normal, L), 0.0f);

        // Reflect L about normal for specular highlight
        float dot_nl = dot3(normal, L);
        float3 reflect = (float3)(2.0f * dot_nl * normal.x - L.x,
                                   2.0f * dot_nl * normal.y - L.y,
                                   2.0f * dot_nl * normal.z - L.z);
        float3 view = normalize3((float3)(-ray_dir.x, -ray_dir.y, -ray_dir.z));
        float spec = pow(fmax(dot3(reflect, view), 0.0f), shin);

        float3 lit = (float3)(
            clamp(albedo.x * (AMBIENT + DIFFUSE * diff) + SPECULAR * spec, 0.0f, 1.0f),
            clamp(albedo.y * (AMBIENT + DIFFUSE * diff) + SPECULAR * spec, 0.0f, 1.0f),
            clamp(albedo.z * (AMBIENT + DIFFUSE * diff) + SPECULAR * spec, 0.0f, 1.0f));

        color = (float4)(lit.x, lit.y, lit.z, 1.0f);
    } else {
        // Sky: vertical gradient from horizon (grey-blue) to zenith (deep blue)
        float t_sky = 0.5f * (ray_dir.y + 1.0f);
        float3 sky  = (float3)(1.0f - 0.7f * t_sky,
                                1.0f - 0.4f * t_sky,
                                1.0f);
        color = (float4)(sky.x, sky.y, sky.z, 1.0f);
    }

    write_imagef(framebuffer, (int2)((int)gx, (int)gy), color);
}
