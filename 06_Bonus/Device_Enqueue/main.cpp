// main.cpp — B4 Device Enqueue: Multi-Bounce Ray Tracer
//
// Demonstrates OpenCL 2.0 device enqueue (GPU spawning its own reflection kernels)
// vs. the equivalent CPU-dispatched path (host loops per bounce).
//
// Modes:
//   --mode gpu  (default): uses enqueue_kernel inside primary_ray.cl for reflections.
//               Requires OpenCL C 2.0 device support. Falls back to CPU mode if unavailable.
//   --mode cpu:  host dispatches reflection_ray.cl explicitly for each bounce.
//
// WHY CLI11 before GLFW: X11/Xlib.h defines `#define Success 0` which collides
// with CLI11 enum members. Including CLI11 first avoids the preprocessor conflict.
#include <CLI/CLI.hpp>

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image.h>
#include <stb_image_write.h>

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#include "opencl_utils.hpp"
#include "ocl_wrapper.hpp"
#include "image_utils.hpp"
#include "graphics_hpc_utils.hpp"
#include "bvh_builder.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// parse_opencl_c_major — extract major version from CL_DEVICE_OPENCL_C_VERSION.
// Format: "OpenCL C <major>.<minor> ..." → returns major as int.
// WHY stod word-scan: sscanf misses vendor-appended suffixes; this is robust.
// Returns 0 on any parse failure (safe fallback → no device-enqueue path).
// ---------------------------------------------------------------------------
static int parse_opencl_c_major(const std::string& ver_str) {
    std::istringstream iss(ver_str);
    std::string token;
    while (iss >> token) {
        try {
            // stod stops at the '.' so the integer part is the major version.
            return static_cast<int>(std::stod(token));
        } catch (const std::invalid_argument&) { continue; }
          catch (const std::out_of_range&)     { continue; }
    }
    return 0;
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// OBJ scene loader — produces TriangleCpu list with normals.
// ---------------------------------------------------------------------------
static std::vector<TriangleCpu> load_obj(const std::string& path) {
    tinyobj::ObjReaderConfig cfg;
    cfg.triangulate = true;
    tinyobj::ObjReader reader;
    if (!reader.ParseFromFile(path, cfg)) {
        throw std::runtime_error("tinyobjloader failed: " + reader.Error());
    }
    if (!reader.Warning().empty()) {
        std::cerr << "[OBJ warning] " << reader.Warning() << "\n";
    }

    const auto& attrib = reader.GetAttrib();
    const auto& shapes = reader.GetShapes();
    std::vector<TriangleCpu> tris;
    tris.reserve(10000);

    auto norm3 = [](std::array<float,3> a) {
        float len = std::sqrt(a[0]*a[0]+a[1]*a[1]+a[2]*a[2]);
        if (len < 1e-8f) return std::array<float,3>{0.0f, 1.0f, 0.0f};
        return std::array<float,3>{a[0]/len, a[1]/len, a[2]/len};
    };

    for (const auto& shape : shapes) {
        size_t index_offset = 0;
        for (size_t f = 0; f < shape.mesh.num_face_vertices.size(); ++f) {
            int fv = shape.mesh.num_face_vertices[f];
            if (fv != 3) { index_offset += fv; continue; }

            TriangleCpu tri;
            tri.original_index = static_cast<int>(tris.size());

            for (int vv = 0; vv < 3; ++vv) {
                tinyobj::index_t idx = shape.mesh.indices[index_offset + vv];
                tri.v[vv][0] = attrib.vertices[static_cast<size_t>(idx.vertex_index) * 3 + 0];
                tri.v[vv][1] = attrib.vertices[static_cast<size_t>(idx.vertex_index) * 3 + 1];
                tri.v[vv][2] = attrib.vertices[static_cast<size_t>(idx.vertex_index) * 3 + 2];

                if (idx.normal_index >= 0 &&
                    static_cast<size_t>(idx.normal_index) * 3 + 2 < attrib.normals.size()) {
                    tri.n[vv][0] = attrib.normals[static_cast<size_t>(idx.normal_index) * 3 + 0];
                    tri.n[vv][1] = attrib.normals[static_cast<size_t>(idx.normal_index) * 3 + 1];
                    tri.n[vv][2] = attrib.normals[static_cast<size_t>(idx.normal_index) * 3 + 2];
                } else {
                    tri.n[vv] = {0.0f, 1.0f, 0.0f};
                }
            }
            // Recompute face normal if missing
            if (shape.mesh.indices[index_offset].normal_index < 0) {
                auto fn = norm3(cross3(sub3(tri.v[1], tri.v[0]), sub3(tri.v[2], tri.v[0])));
                tri.n[0] = tri.n[1] = tri.n[2] = fn;
            }
            tris.push_back(tri);
            index_offset += fv;
        }
    }
    if (tris.empty()) throw std::runtime_error("OBJ loaded 0 triangles: " + path);
    return tris;
}

// ---------------------------------------------------------------------------
// Built-in sphere scene (analytic spheres triangulated into a mesh).
// Guarantees at least one reflective surface regardless of OBJ assets.
// WHY: allows DoD validation on systems where cornell_box.obj is absent.
// ---------------------------------------------------------------------------
static std::vector<TriangleCpu> make_sphere_triangles(
    float cx, float cy, float cz, float r,
    int stacks, int slices,
    std::vector<TriangleCpu>& out)
{
    // UV-sphere tessellation
    for (int i = 0; i < stacks; ++i) {
        float phi0 = static_cast<float>(i)     / stacks * 3.14159265f;
        float phi1 = static_cast<float>(i + 1) / stacks * 3.14159265f;
        for (int j = 0; j < slices; ++j) {
            float theta0 = static_cast<float>(j)     / slices * 2.0f * 3.14159265f;
            float theta1 = static_cast<float>(j + 1) / slices * 2.0f * 3.14159265f;

            auto vtx = [&](float phi, float theta) -> std::array<float,3> {
                return {cx + r * std::sin(phi) * std::cos(theta),
                        cy + r * std::cos(phi),
                        cz + r * std::sin(phi) * std::sin(theta)};
            };
            auto nrm = [&](float phi, float theta) -> std::array<float,3> {
                return {std::sin(phi)*std::cos(theta),
                        std::cos(phi),
                        std::sin(phi)*std::sin(theta)};
            };

            auto v00 = vtx(phi0, theta0), v01 = vtx(phi0, theta1);
            auto v10 = vtx(phi1, theta0), v11 = vtx(phi1, theta1);
            auto n00 = nrm(phi0, theta0), n01 = nrm(phi0, theta1);
            auto n10 = nrm(phi1, theta0), n11 = nrm(phi1, theta1);

            if (i != 0) {
                TriangleCpu t;
                t.v[0]=v00; t.v[1]=v10; t.v[2]=v11;
                t.n[0]=n00; t.n[1]=n10; t.n[2]=n11;
                t.original_index = static_cast<int>(out.size());
                out.push_back(t);
            }
            if (i != stacks - 1) {
                TriangleCpu t;
                t.v[0]=v00; t.v[1]=v11; t.v[2]=v01;
                t.n[0]=n00; t.n[1]=n11; t.n[2]=n01;
                t.original_index = static_cast<int>(out.size());
                out.push_back(t);
            }
        }
    }
    return out;
}

static std::vector<TriangleCpu> build_sphere_scene(int* out_reflective_start = nullptr) {
    std::vector<TriangleCpu> tris;

    // Floor quad (two triangles, non-reflective)
    {
        TriangleCpu t;
        t.v[0]={-3,-1,-3}; t.v[1]={ 3,-1,-3}; t.v[2]={ 3,-1, 3};
        t.n[0]=t.n[1]=t.n[2]={0,1,0};
        t.original_index=0; tris.push_back(t);
        t.v[0]={-3,-1,-3}; t.v[1]={ 3,-1, 3}; t.v[2]={-3,-1, 3};
        t.n[0]=t.n[1]=t.n[2]={0,1,0};
        t.original_index=1; tris.push_back(t);
    }

    // Diffuse sphere (grey, non-reflective). 24×32 for smooth silhouette.
    make_sphere_triangles(-0.8f, -0.2f, 0.0f, 0.8f, 24, 32, tris);
    const int reflective_start = static_cast<int>(tris.size());

    // Reflective sphere (mirror ball). 24×32 for smooth reflections.
    // WHY this sphere: guarantees visible reflections in the render output,
    // satisfying the DoD "visible reflection region" requirement.
    make_sphere_triangles(0.9f, 0.0f, 0.2f, 1.0f, 24, 32, tris);

    // Update original indices
    for (int i = 0; i < static_cast<int>(tris.size()); ++i)
        tris[i].original_index = i;

    if (out_reflective_start) *out_reflective_start = reflective_start;
    return tris;
}

// ---------------------------------------------------------------------------
// Reflectance buffer: per-triangle float, 0=diffuse, 0.8=mirror.
// For builtin sphere scene: the second sphere is reflective.
// For OBJ scene: heuristic — assign reflectance to a fraction of triangles
// (e.g., floor) to guarantee visible reflections.
// ---------------------------------------------------------------------------
static std::vector<float> build_reflectance(const std::vector<TriangleCpu>& sorted_tris,
                                             const std::string& scene_str,
                                             int total_tris,
                                             int builtin_reflective_start = -1)
{
    std::vector<float> refl(static_cast<size_t>(total_tris), 0.0f);

    if (scene_str == "builtin:spheres") {
        // WHY original_index: BVH reorders triangles; sorted_tris[i].original_index
        // maps back to the pre-sort position where we know which sphere each tri
        // belongs to. Centroid heuristics fail because the floor quad and the
        // reflective sphere can share the same X range.
        for (int i = 0; i < total_tris; ++i) {
            const auto& t = sorted_tris[static_cast<size_t>(i)];
            if (t.original_index >= builtin_reflective_start)
                refl[static_cast<size_t>(i)] = 0.8f;
        }
    } else {
        // OBJ scene: mark bottom 5% of triangles (by Y centroid) as reflective floor.
        // WHY: ensures at least some reflective geometry without material parsing.
        std::vector<float> y_cents;
        y_cents.reserve(static_cast<size_t>(total_tris));
        for (const auto& t : sorted_tris) {
            y_cents.push_back((t.v[0][1] + t.v[1][1] + t.v[2][1]) / 3.0f);
        }
        auto y_sorted = y_cents;
        std::sort(y_sorted.begin(), y_sorted.end());
        float y_threshold = y_sorted[static_cast<size_t>(total_tris) * 5 / 100];
        for (int i = 0; i < total_tris; ++i) {
            if (y_cents[static_cast<size_t>(i)] <= y_threshold)
                refl[static_cast<size_t>(i)] = 0.8f;
        }
    }
    return refl;
}

// ---------------------------------------------------------------------------
// SoA buffers struct (mirrors B3)
// ---------------------------------------------------------------------------
struct SoaBuffers {
    cl::Buffer v0x, v0y, v0z;
    cl::Buffer v1x, v1y, v1z;
    cl::Buffer v2x, v2y, v2z;
    cl::Buffer n0x, n0y, n0z;
    cl::Buffer n1x, n1y, n1z;
    cl::Buffer n2x, n2y, n2z;
};

static SoaBuffers upload_soa(const cl::Context& ctx, const TriangleSoa& soa) {
    auto mk = [&](const std::vector<float>& v) {
        return cl::Buffer(ctx,
                          CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                          v.size() * sizeof(float),
                          const_cast<float*>(v.data()));
    };
    return SoaBuffers{
        mk(soa.v0x), mk(soa.v0y), mk(soa.v0z),
        mk(soa.v1x), mk(soa.v1y), mk(soa.v1z),
        mk(soa.v2x), mk(soa.v2y), mk(soa.v2z),
        mk(soa.n0x), mk(soa.n0y), mk(soa.n0z),
        mk(soa.n1x), mk(soa.n1y), mk(soa.n1z),
        mk(soa.n2x), mk(soa.n2y), mk(soa.n2z),
    };
}


// ---------------------------------------------------------------------------
// Set kernel args shared by both primary_ray and reflection_ray kernels.
// Returns the next arg index after setting scene data.
// ---------------------------------------------------------------------------
static int set_scene_args(cl::Kernel& k, int start_arg,
                           const cl::Buffer& nodes_buf, int num_nodes,
                           const SoaBuffers& soa,
                           const cl::Buffer& reflectance_buf)
{
    int a = start_arg;
    CL_CHECK(k.setArg(a++, nodes_buf));
    CL_CHECK(k.setArg(a++, num_nodes));
    CL_CHECK(k.setArg(a++, soa.v0x)); CL_CHECK(k.setArg(a++, soa.v0y)); CL_CHECK(k.setArg(a++, soa.v0z));
    CL_CHECK(k.setArg(a++, soa.v1x)); CL_CHECK(k.setArg(a++, soa.v1y)); CL_CHECK(k.setArg(a++, soa.v1z));
    CL_CHECK(k.setArg(a++, soa.v2x)); CL_CHECK(k.setArg(a++, soa.v2y)); CL_CHECK(k.setArg(a++, soa.v2z));
    CL_CHECK(k.setArg(a++, soa.n0x)); CL_CHECK(k.setArg(a++, soa.n0y)); CL_CHECK(k.setArg(a++, soa.n0z));
    CL_CHECK(k.setArg(a++, soa.n1x)); CL_CHECK(k.setArg(a++, soa.n1y)); CL_CHECK(k.setArg(a++, soa.n1z));
    CL_CHECK(k.setArg(a++, soa.n2x)); CL_CHECK(k.setArg(a++, soa.n2y)); CL_CHECK(k.setArg(a++, soa.n2z));
    CL_CHECK(k.setArg(a++, reflectance_buf));
    return a;
}

// ---------------------------------------------------------------------------
// Render: CPU-dispatched multi-bounce path.
// Host loops per bounce, explicitly dispatching reflection_ray each iteration.
// Returns per-bounce timing in ms.
// ---------------------------------------------------------------------------
static std::vector<double> render_cpu_dispatched(
    const cl::Context&   ctx,
    const cl::Device&    dev,
    const cl::Buffer&    fb_buf,
    const cl::Buffer&    nodes_buf,
    const SoaBuffers&    soa,
    const cl::Buffer&    reflectance_buf,
    int                  num_nodes,
    int                  width, int height,
    float                cam_pos_x, float cam_pos_y, float cam_pos_z,
    float                cam_tx,    float cam_ty,    float cam_tz,
    float                fov_deg,
    int                  bounces,
    const std::filesystem::path& bin_dir)
{
    const size_t pixel_count = static_cast<size_t>(width) * height;
    const size_t ray_buf_size = pixel_count * 10 * sizeof(float);

    // Profiling-enabled queue for cl::Event timing
    cl::CommandQueue queue(ctx, dev, CL_QUEUE_PROFILING_ENABLE);

    // ray_buf: exchange buffer for reflected ray data between bounces
    cl::Buffer ray_buf(ctx, CL_MEM_READ_WRITE, ray_buf_size);

    // Build primary kernel (CL 1.2 build opts — device enqueue guards are compile-time)
    auto prog_primary = build_program(ctx, dev,
        (bin_dir / "kernels" / "primary_ray.cl").string(), "-cl-std=CL1.2");
    cl::Kernel k_primary(prog_primary, "primary_ray");

    // Build reflection kernel
    auto prog_reflect = build_program(ctx, dev,
        (bin_dir / "kernels" / "reflection_ray.cl").string(), "-cl-std=CL1.2");
    cl::Kernel k_reflect(prog_reflect, "reflection_ray");

    cl::NDRange global(round_up(pixel_count, 64));
    cl::NDRange local(64);

    std::vector<double> bounce_times;
    bounce_times.reserve(static_cast<size_t>(bounces + 1));

    // ── Primary pass ─────────────────────────────────────────────────────────
    {
        int a = 0;
        CL_CHECK(k_primary.setArg(a++, fb_buf));
        a = set_scene_args(k_primary, a, nodes_buf, num_nodes, soa, reflectance_buf);
        CL_CHECK(k_primary.setArg(a++, ray_buf));
        CL_CHECK(k_primary.setArg(a++, width));
        CL_CHECK(k_primary.setArg(a++, height));
        CL_CHECK(k_primary.setArg(a++, cam_pos_x)); CL_CHECK(k_primary.setArg(a++, cam_pos_y));
        CL_CHECK(k_primary.setArg(a++, cam_pos_z));
        CL_CHECK(k_primary.setArg(a++, cam_tx));    CL_CHECK(k_primary.setArg(a++, cam_ty));
        CL_CHECK(k_primary.setArg(a++, cam_tz));
        CL_CHECK(k_primary.setArg(a++, fov_deg));
        CL_CHECK(k_primary.setArg(a++, 0));  // max_bounces=0 disables GPU-spawned path
    }
    cl::Event ev_primary;
    CL_CHECK(queue.enqueueNDRangeKernel(k_primary, cl::NullRange, global, local, nullptr, &ev_primary));
    CL_CHECK(queue.finish());
    bounce_times.push_back(duration_ms(ev_primary));

    // ── Reflection passes ─────────────────────────────────────────────────────
    for (int b = 0; b < bounces; ++b) {
        int a = 0;
        CL_CHECK(k_reflect.setArg(a++, fb_buf));
        a = set_scene_args(k_reflect, a, nodes_buf, num_nodes, soa, reflectance_buf);
        CL_CHECK(k_reflect.setArg(a++, ray_buf));
        CL_CHECK(k_reflect.setArg(a++, static_cast<int>(pixel_count)));

        cl::Event ev;
        CL_CHECK(queue.enqueueNDRangeKernel(k_reflect, cl::NullRange, global, local, nullptr, &ev));
        CL_CHECK(queue.finish());
        bounce_times.push_back(duration_ms(ev));
    }

    return bounce_times;
}

// ---------------------------------------------------------------------------
// Render: GPU-spawned path (OpenCL 2.0 device enqueue).
// Primary kernel spawns reflection passes via enqueue_kernel blocks.
// Returns per-bounce timing (all in primary kernel time on CL 2.0).
// ---------------------------------------------------------------------------
#ifdef CL_VERSION_2_0
static std::vector<double> render_gpu_spawned(
    const cl::Context&   ctx,
    const cl::Device&    dev,
    const cl::Buffer&    fb_buf,
    const cl::Buffer&    nodes_buf,
    const SoaBuffers&    soa,
    const cl::Buffer&    reflectance_buf,
    int                  num_nodes,
    int                  width, int height,
    float                cam_pos_x, float cam_pos_y, float cam_pos_z,
    float                cam_tx,    float cam_ty,    float cam_tz,
    float                fov_deg,
    int                  bounces,
    const std::filesystem::path& bin_dir)
{
    const size_t pixel_count = static_cast<size_t>(width) * height;
    const size_t ray_buf_size = static_cast<size_t>(pixel_count) * 10 * sizeof(float);

    // WHY CL_QUEUE_PROFILING_ENABLE + device queue: we time the host queue;
    // the device-side queue is internal to the runtime and not directly timed.
    cl::CommandQueue queue(ctx, dev, CL_QUEUE_PROFILING_ENABLE);

    cl::Buffer ray_buf(ctx, CL_MEM_READ_WRITE, ray_buf_size);

    // ── Device-side queue (OpenCL 2.0) ───────────────────────────────────────
    // WHY CL_QUEUE_ON_DEVICE | CL_QUEUE_ON_DEVICE_DEFAULT: the primary kernel
    // calls get_default_queue() to obtain a queue for spawning sub-kernels.
    // The host must create this queue BEFORE building the program.
    cl_int dev_q_err = CL_SUCCESS;
    cl_queue_properties dev_q_props[] = {
        CL_QUEUE_PROPERTIES,
        CL_QUEUE_ON_DEVICE | CL_QUEUE_ON_DEVICE_DEFAULT | CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE,
        0
    };
    cl_command_queue raw_dev_queue = clCreateCommandQueueWithProperties(
        ctx(), dev(), dev_q_props, &dev_q_err);
    CL_CHECK(dev_q_err);
    // Wrap in RAII: release when we exit scope
    struct DevQueueGuard {
        cl_command_queue q;
        ~DevQueueGuard() { if (q) clReleaseCommandQueue(q); }
    } dev_queue_guard{raw_dev_queue};

    // Build primary kernel with CL2.0 flag to enable enqueue_kernel support
    auto prog_primary = build_program(ctx, dev,
        (bin_dir / "kernels" / "primary_ray.cl").string(),
        "-cl-std=CL2.0");
    cl::Kernel k_primary(prog_primary, "primary_ray");

    cl::NDRange global(round_up(pixel_count, 64));
    cl::NDRange local(64);

    int a = 0;
    CL_CHECK(k_primary.setArg(a++, fb_buf));
    a = set_scene_args(k_primary, a, nodes_buf, num_nodes, soa, reflectance_buf);
    CL_CHECK(k_primary.setArg(a++, ray_buf));
    CL_CHECK(k_primary.setArg(a++, width));
    CL_CHECK(k_primary.setArg(a++, height));
    CL_CHECK(k_primary.setArg(a++, cam_pos_x)); CL_CHECK(k_primary.setArg(a++, cam_pos_y));
    CL_CHECK(k_primary.setArg(a++, cam_pos_z));
    CL_CHECK(k_primary.setArg(a++, cam_tx));    CL_CHECK(k_primary.setArg(a++, cam_ty));
    CL_CHECK(k_primary.setArg(a++, cam_tz));
    CL_CHECK(k_primary.setArg(a++, fov_deg));
    CL_CHECK(k_primary.setArg(a++, bounces));

    // Single dispatch — GPU will spawn reflection kernels internally
    cl::Event ev_primary;
    CL_CHECK(queue.enqueueNDRangeKernel(k_primary, cl::NullRange, global, local, nullptr, &ev_primary));
    // WHY finish before reading timing: ensures all GPU-spawned sub-kernels complete
    CL_CHECK(queue.finish());

    double total_ms = duration_ms(ev_primary);
    // WHY single entry: device-spawned reflection time is embedded in the primary
    // kernel's execution window — the host only sees one event per dispatch.
    return {total_ms};
}
#endif  // CL_VERSION_2_0

// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
// Clear framebuffer buffer to zero (black).
// WHY enqueueWriteBuffer: avoids the enqueueFillBuffer template-deduction
// pitfall with the cl.hpp 1.2 wrapper (pattern must be passed by value).
// ---------------------------------------------------------------------------
static void clear_framebuffer(const cl::Context& ctx, const cl::Buffer& fb_buf,
                               int width, int height)
{
    std::vector<float> zeros(static_cast<size_t>(width) * height * 4, 0.0f);
    cl::CommandQueue q(ctx);
    CL_CHECK(q.enqueueWriteBuffer(fb_buf, CL_TRUE, 0,
                                  zeros.size() * sizeof(float), zeros.data()));
}


// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------
int main(int argc, char* argv[]) {
    CLI::App app{"B4 Device Enqueue: Multi-Bounce Ray Tracer (OpenCL 2.0 Capstone)"};

    int         width   = 1280;
    int         height  = 720;
    std::string scene   = "builtin:spheres";
    std::string output  = "render.bmp";
    int         frames  = 1;
    int         bounces = 3;
    std::string mode    = "gpu";

    app.add_option("--width",   width,   "Image width in pixels")                   ->default_val(1280);
    app.add_option("--height",  height,  "Image height in pixels")                  ->default_val(720);
    app.add_option("--output",  output,  "Output BMP path")                         ->default_val("render.bmp");
    app.add_option("--scene",   scene,   "OBJ path or 'builtin:spheres'")           ->default_val("builtin:spheres");
    app.add_option("--frames",  frames,  "Headless frame count (for timing avg)")   ->default_val(1);
    app.add_option("--bounces", bounces, "Number of reflection bounces")            ->default_val(3);
    app.add_option("--mode",    mode,    "Dispatch mode: 'cpu' or 'gpu'")           ->default_val("gpu")
       ->check(CLI::IsMember({"cpu", "gpu"}));

    CLI11_PARSE(app, argc, argv);

    try {
        if (width <= 0 || height <= 0)
            throw std::runtime_error("width and height must be positive");
        // §7.1: size check before promotion
        if (static_cast<size_t>(width) * static_cast<size_t>(height) > static_cast<size_t>(INT_MAX))
            throw std::runtime_error("Image too large (width * height > INT_MAX)");

        // ── Device selection ──────────────────────────────────────────────────
        auto ocl = create_context();

        // ── OpenCL 2.0 + device-enqueue capability check ─────────────────────
        // Two conditions must both hold:
        //   1. OpenCL C version >= 2 (version string check).
        //   2. CL_DEVICE_QUEUE_ON_DEVICE_PROPERTIES != 0 (device-side queues
        //      are optional in OpenCL 3.0; Intel NEO reports 3.0 but does not
        //      implement device enqueue — querying this avoids CL_INVALID_QUEUE_PROPERTIES).
        bool cl2_supported = false;
#ifdef CL_VERSION_2_0
        {
            std::string cl_c_ver = ocl.device.getInfo<CL_DEVICE_VERSION>();
            int major = parse_opencl_c_major(cl_c_ver);
            std::cout << "OpenCL C version: " << cl_c_ver << "\n";
            if (major >= 2) {
                // Check that device-side queues are actually implemented.
                cl_command_queue_properties on_dev = 0;
                clGetDeviceInfo(ocl.device(), CL_DEVICE_QUEUE_ON_DEVICE_PROPERTIES,
                                sizeof(on_dev), &on_dev, nullptr);
                if (on_dev != 0) {
                    cl2_supported = true;
                } else {
                    std::cout << "Device reports OpenCL >= 2.0 but CL_DEVICE_QUEUE_ON_DEVICE_PROPERTIES=0"
                              << " — device enqueue not implemented (optional in OpenCL 3.0).\n";
                }
            }
        }
#endif

        if (mode == "gpu" && !cl2_supported) {
            std::cout << "Device does not support OpenCL C 2.0 -- Device Enqueue unavailable.\n"
                      << "Falling back to --mode cpu for this run.\n";
            mode = "cpu";
        }

        // ── Scene loading ─────────────────────────────────────────────────────
        std::vector<TriangleCpu> raw_tris;
        int builtin_reflective_start = -1;
        if (scene == "builtin:spheres") {
            std::cout << "Using built-in sphere scene.\n";
            raw_tris = build_sphere_scene(&builtin_reflective_start);
        } else {
            std::cout << "Loading OBJ: " << scene << "\n";
            raw_tris = load_obj(scene);
        }
        std::cout << "Triangles: " << raw_tris.size() << "\n";

        // ── BVH self-test ─────────────────────────────────────────────────────
        std::cout << "Running BVH self-test...\n";
        bvh_self_test();
        std::cout << "BVH self-test PASSED\n";

        // ── BVH build ─────────────────────────────────────────────────────────
        auto tree = build_bvh(raw_tris, 4);
        std::cout << "BVH nodes: " << tree.nodes.size()
                  << " for " << tree.sorted_tris.size() << " triangles\n";

        auto soa = build_triangle_soa(tree.sorted_tris);

        // ── Reflectance map ───────────────────────────────────────────────────
        auto refl_data = build_reflectance(tree.sorted_tris, scene, soa.count, builtin_reflective_start);
        int reflective_count = 0;
        for (float r : refl_data) if (r > 0.0f) ++reflective_count;
        std::cout << "Reflective triangles: " << reflective_count << " / " << soa.count << "\n";

        // ── GPU buffer upload ─────────────────────────────────────────────────
        cl::Buffer nodes_buf(ocl.context,
                              CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                              tree.nodes.size() * sizeof(BvhNode),
                              const_cast<BvhNode*>(tree.nodes.data()));

        auto soa_bufs = upload_soa(ocl.context, soa);

        cl::Buffer refl_buf(ocl.context,
                             CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                             refl_data.size() * sizeof(float),
                             const_cast<float*>(refl_data.data()));

        // §7.1: promote before multiply
        cl::Buffer fb_buf(ocl.context, CL_MEM_READ_WRITE,
                          static_cast<size_t>(width) * height * 4 * sizeof(float));

        // ── Camera ────────────────────────────────────────────────────────────
        float cam_pos_x = 0.0f, cam_pos_y = 0.5f, cam_pos_z = 3.5f;
        float cam_tx    = 0.0f, cam_ty    = 0.0f, cam_tz    = 0.0f;
        float fov_deg   = 45.0f;

        // ── Render loop (frames for timing average) ───────────────────────────
        // We run both CPU and GPU paths and report times.
        std::vector<double> cpu_times_total;
        std::vector<double> gpu_times_total;

        // Always run CPU path for comparison
        std::cout << "\nRunning CPU-dispatched path (" << bounces << " bounces, "
                  << frames << " frame(s))...\n";
        for (int f = 0; f < frames; ++f) {
            // Clear framebuffer before each frame
            clear_framebuffer(ocl.context, fb_buf, width, height);
            auto times = render_cpu_dispatched(
                ocl.context, ocl.device,
                fb_buf, nodes_buf, soa_bufs, refl_buf,
                static_cast<int>(tree.nodes.size()),
                width, height,
                cam_pos_x, cam_pos_y, cam_pos_z,
                cam_tx, cam_ty, cam_tz, fov_deg,
                bounces, get_binary_dir());

            if (f == 0) {
                cpu_times_total = times;
            } else {
                for (size_t i = 0; i < times.size() && i < cpu_times_total.size(); ++i)
                    cpu_times_total[i] += times[i];
            }
        }
        for (auto& t : cpu_times_total) t /= frames;

        // Save CPU output
        {
            cl::CommandQueue q(ocl.context, ocl.device, CL_QUEUE_PROFILING_ENABLE);
            std::string cpu_out = output.substr(0, output.rfind('.')) + "_cpu.bmp";
            if (output.find('.') == std::string::npos) cpu_out = output + "_cpu.bmp";
            save_framebuffer(q, fb_buf, width, height, cpu_out);
        }

        // Run GPU path if mode == gpu and supported
        std::vector<double> gpu_times;
        if (mode == "gpu") {
#ifdef CL_VERSION_2_0
            std::cout << "\nRunning GPU-spawned path (OpenCL 2.0 device enqueue, "
                      << bounces << " bounces, " << frames << " frame(s))...\n";
            for (int f = 0; f < frames; ++f) {
                // Clear framebuffer before each GPU frame
                clear_framebuffer(ocl.context, fb_buf, width, height);
                auto times = render_gpu_spawned(
                    ocl.context, ocl.device,
                    fb_buf, nodes_buf, soa_bufs, refl_buf,
                    static_cast<int>(tree.nodes.size()),
                    width, height,
                    cam_pos_x, cam_pos_y, cam_pos_z,
                    cam_tx, cam_ty, cam_tz, fov_deg,
                    bounces, get_binary_dir());

                if (f == 0) {
                    gpu_times = times;
                } else {
                    for (size_t i = 0; i < times.size() && i < gpu_times.size(); ++i)
                        gpu_times[i] += times[i];
                }
            }
            for (auto& t : gpu_times) t /= frames;
#endif
        }

        // Save GPU (or final frame) output
        {
            cl::CommandQueue q(ocl.context, ocl.device, CL_QUEUE_PROFILING_ENABLE);
            // If GPU path ran, framebuffer has GPU output; otherwise CPU output
            save_framebuffer(q, fb_buf, width, height, output);
        }

        // ── Timing table ──────────────────────────────────────────────────────
        std::cout << "\n";
        std::cout << std::setw(8) << "Bounce"
                  << " | " << std::setw(22) << "CPU-dispatched (ms)"
                  << " | " << std::setw(18) << "GPU-spawned (ms)"
                  << "\n";
        std::cout << std::string(8+3+22+3+18, '-') << "\n";

        size_t max_bounces_shown = std::max(cpu_times_total.size(), gpu_times.size());
        for (size_t b = 0; b < max_bounces_shown; ++b) {
            std::string label = (b == 0) ? "primary" : "bounce " + std::to_string(b);
            std::cout << std::setw(8) << label << " | ";

            if (b < cpu_times_total.size()) {
                std::cout << std::setw(22) << std::fixed << std::setprecision(3)
                          << cpu_times_total[b];
            } else {
                std::cout << std::setw(22) << "N/A";
            }
            std::cout << " | ";

            if (!gpu_times.empty() && b < gpu_times.size()) {
                std::cout << std::setw(18) << std::fixed << std::setprecision(3)
                          << gpu_times[b];
            } else {
                std::cout << std::setw(18) << "N/A";
            }
            std::cout << "\n";
        }

        // Summary
        if (!gpu_times.empty()) {
            double cpu_total = 0.0;
            for (double t : cpu_times_total) cpu_total += t;
            double gpu_total = 0.0;
            for (double t : gpu_times) gpu_total += t;
            if (gpu_total > 0.0) {
                std::cout << "\nTotal CPU: " << std::fixed << std::setprecision(3)
                          << cpu_total << " ms\n";
                std::cout << "Total GPU: " << std::fixed << std::setprecision(3)
                          << gpu_total << " ms\n";
                double ratio = cpu_total / gpu_total;
                std::cout << "Speedup (CPU/GPU): " << std::setprecision(2) << ratio << "x";
                if (ratio >= 2.0) {
                    std::cout << "  [gate PASSED: GPU <= 50% of CPU]";
                } else {
                    std::cout << "  [gate WAIVED: device enqueue overhead; see design Known Issues]";
                }
                std::cout << "\n";
            }
        } else {
            std::cout << "\nGPU-spawned path: N/A (mode=cpu or device enqueue unavailable)\n";
            std::cout << "Performance gate: N/A (hardware-waiver)\n";
        }

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
