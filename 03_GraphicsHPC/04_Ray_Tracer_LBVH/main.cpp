// main.cpp — B4 GPU LBVH Ray Tracer
//
// Builds a Linear BVH entirely on the GPU each frame using 6 OpenCL kernels:
//   scene_bounds → morton → radix_sort → lbvh_build → lbvh_aabb → traverse
//
// No CPU round-trip between build and render — the GPU is autonomous.
//
// Benchmark: averages per-kernel timing over --frames frames, prints a table,
// and saves the final frame as --output (BMP).
//
// WHY CLI11 first: transitively included headers (stb, tinyobjloader) sometimes
// drag in X11 headers that #define Success 0, colliding with CLI11 enum member.
// Including CLI11 before any such header prevents the collision.
#include <CLI/CLI.hpp>

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image.h>
#include <stb_image_write.h>

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#include "opencl_utils.hpp"
#include "ocl_wrapper.hpp"
#include "graphics_hpc_utils.hpp"  // save_framebuffer, save_bmp (includes image_utils.hpp)
#include "lbvh_builder.hpp"

#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <stdexcept>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Camera (fixed orbit — no interactive mode in this benchmark)
// ---------------------------------------------------------------------------
struct Camera {
    float azimuth   =  0.3f;
    float elevation =  0.3f;
    float distance  =  0.3f;
    float target[3] = {0.0f, 0.05f, 0.0f};
    float fov_deg   = 45.0f;

    void get_pos(float& px, float& py, float& pz) const {
        px = target[0] + distance * std::cos(elevation) * std::sin(azimuth);
        py = target[1] + distance * std::sin(elevation);
        pz = target[2] + distance * std::cos(elevation) * std::cos(azimuth);
    }
};

// ---------------------------------------------------------------------------
// OBJ loader — returns flat vector of TriangleCpu
// ---------------------------------------------------------------------------
static std::vector<TriangleCpu> load_obj(const std::string& path) {
    tinyobj::ObjReaderConfig cfg;
    cfg.triangulate = true;
    tinyobj::ObjReader reader;
    if (!reader.ParseFromFile(path, cfg))
        throw std::runtime_error("tinyobjloader: " + reader.Error());
    if (!reader.Warning().empty())
        std::cerr << "[OBJ warning] " << reader.Warning() << "\n";

    const auto& attrib = reader.GetAttrib();
    const auto& shapes = reader.GetShapes();

    std::vector<TriangleCpu> tris;
    tris.reserve(100000);

    for (const auto& shape : shapes) {
        size_t index_offset = 0;
        for (size_t f = 0; f < shape.mesh.num_face_vertices.size(); ++f) {
            int fv = shape.mesh.num_face_vertices[f];
            if (fv != 3) { index_offset += fv; continue; }

            TriangleCpu tri;
            tri.original_index = static_cast<int>(tris.size());

            for (int vv = 0; vv < 3; ++vv) {
                tinyobj::index_t idx = shape.mesh.indices[index_offset + vv];
                // §7.1: cast to size_t before multiply to avoid signed overflow
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

            // Recompute face normal when vertex normals absent
            if (shape.mesh.indices[index_offset].normal_index < 0) {
                auto norm3 = [](std::array<float,3> a) -> std::array<float,3> {
                    float len = std::sqrt(a[0]*a[0]+a[1]*a[1]+a[2]*a[2]);
                    if (len < 1e-8f) return {0.0f,1.0f,0.0f};
                    return {a[0]/len,a[1]/len,a[2]/len};
                };
                auto fn = norm3(cross3(sub3(tri.v[1],tri.v[0]), sub3(tri.v[2],tri.v[0])));
                tri.n[0] = tri.n[1] = tri.n[2] = fn;
            }

            tris.push_back(tri);
            index_offset += fv;
        }
    }
    if (tris.empty()) throw std::runtime_error("OBJ has 0 triangles: " + path);
    return tris;
}

// ---------------------------------------------------------------------------
// SoA GPU buffer container
// ---------------------------------------------------------------------------
struct SoaGpuBuffers {
    cl::Buffer v0x, v0y, v0z;
    cl::Buffer v1x, v1y, v1z;
    cl::Buffer v2x, v2y, v2z;
    cl::Buffer n0x, n0y, n0z;
    cl::Buffer n1x, n1y, n1z;
    cl::Buffer n2x, n2y, n2z;
};

static SoaGpuBuffers make_soa_buffers(const cl::Context& ctx, const TriangleSoa& soa) {
    auto mk = [&](const std::vector<float>& v) {
        return cl::Buffer(ctx, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                          v.size() * sizeof(float),
                          const_cast<float*>(v.data()));
    };
    return {
        mk(soa.v0x), mk(soa.v0y), mk(soa.v0z),
        mk(soa.v1x), mk(soa.v1y), mk(soa.v1z),
        mk(soa.v2x), mk(soa.v2y), mk(soa.v2z),
        mk(soa.n0x), mk(soa.n0y), mk(soa.n0z),
        mk(soa.n1x), mk(soa.n1y), mk(soa.n1z),
        mk(soa.n2x), mk(soa.n2y), mk(soa.n2z),
    };
}

// ---------------------------------------------------------------------------
// Set traversal kernel args
// ---------------------------------------------------------------------------
static void set_traverse_args(cl::Kernel& k,
                               cl::Buffer& fb_buf,
                               const cl::Buffer& node_buf,
                               const cl::Buffer& sorted_idx_buf,
                               const SoaGpuBuffers& soa,
                               int N, int width, int height,
                               const Camera& cam,
                               size_t wg_x, size_t wg_y,
                               int stack_depth)
{
    float px, py, pz;
    cam.get_pos(px, py, pz);

    int a = 0;
    CL_CHECK(k.setArg(a++, fb_buf));
    CL_CHECK(k.setArg(a++, node_buf));
    CL_CHECK(k.setArg(a++, sorted_idx_buf));
    // SoA positions
    CL_CHECK(k.setArg(a++, soa.v0x)); CL_CHECK(k.setArg(a++, soa.v0y)); CL_CHECK(k.setArg(a++, soa.v0z));
    CL_CHECK(k.setArg(a++, soa.v1x)); CL_CHECK(k.setArg(a++, soa.v1y)); CL_CHECK(k.setArg(a++, soa.v1z));
    CL_CHECK(k.setArg(a++, soa.v2x)); CL_CHECK(k.setArg(a++, soa.v2y)); CL_CHECK(k.setArg(a++, soa.v2z));
    // SoA normals
    CL_CHECK(k.setArg(a++, soa.n0x)); CL_CHECK(k.setArg(a++, soa.n0y)); CL_CHECK(k.setArg(a++, soa.n0z));
    CL_CHECK(k.setArg(a++, soa.n1x)); CL_CHECK(k.setArg(a++, soa.n1y)); CL_CHECK(k.setArg(a++, soa.n1z));
    CL_CHECK(k.setArg(a++, soa.n2x)); CL_CHECK(k.setArg(a++, soa.n2y)); CL_CHECK(k.setArg(a++, soa.n2z));
    CL_CHECK(k.setArg(a++, N));
    CL_CHECK(k.setArg(a++, width));
    CL_CHECK(k.setArg(a++, height));
    CL_CHECK(k.setArg(a++, px));  CL_CHECK(k.setArg(a++, py));  CL_CHECK(k.setArg(a++, pz));
    CL_CHECK(k.setArg(a++, cam.target[0]));
    CL_CHECK(k.setArg(a++, cam.target[1]));
    CL_CHECK(k.setArg(a++, cam.target[2]));
    CL_CHECK(k.setArg(a++, cam.fov_deg));
    // Local memory stack: wg_x * wg_y * stack_depth * sizeof(int) bytes
    CL_CHECK(k.setArg(a++, cl::Local(wg_x * wg_y * static_cast<size_t>(stack_depth) * sizeof(cl_int))));
}

// Forward declaration (defined after run())
static double duration_ms_traverse(double total_ms, int frames);

// ---------------------------------------------------------------------------
// Pixel sanity check (task 9.6)
// ---------------------------------------------------------------------------
static void check_framebuffer(const std::vector<float>& pixels, int width, int height) {
    // §7.1: size_t promotion before multiply
    size_t n_pixels = static_cast<size_t>(width) * height;
    double sum_r = 0.0, sum_r2 = 0.0;

    for (size_t i = 0; i < n_pixels; ++i) {
        // Use only red channel (grey image — R == G == B for bunny)
        float v = pixels[i * 4];
        sum_r  += v;
        sum_r2 += static_cast<double>(v) * v;
    }

    double mean = sum_r / static_cast<double>(n_pixels);
    double var  = sum_r2 / static_cast<double>(n_pixels) - mean * mean;

    std::cout << "Framebuffer stats: mean=" << std::fixed << std::setprecision(4)
              << mean << " variance=" << var << "\n";

    if (mean < 0.01)
        throw std::runtime_error(
            "Sanity check FAILED: framebuffer is all black (mean=" +
            std::to_string(mean) + ") — BVH traversal broken");
    if (mean > 0.99)
        throw std::runtime_error(
            "Sanity check FAILED: framebuffer is all white (mean=" +
            std::to_string(mean) + ") — shading overflow");
    if (var < 0.001)
        throw std::runtime_error(
            "Sanity check FAILED: flat colour (variance=" +
            std::to_string(var) + ") — geometry not reached");
}

// ---------------------------------------------------------------------------
// Main benchmark
// ---------------------------------------------------------------------------
static void run(const std::string& scene_path,
                const std::string& output_path,
                int width, int height, int frames,
                int wg_size, int stack_depth)
{
    if (width <= 0 || height <= 0)
        throw std::runtime_error("width and height must be positive");
    // §7.1: guard integer overflow
    if (static_cast<size_t>(width) * height > static_cast<size_t>(INT_MAX))
        throw std::runtime_error("Image too large");

    std::cout << "Loading OBJ: " << scene_path << "\n";
    auto tris = load_obj(scene_path);
    const int N = static_cast<int>(tris.size());
    std::cout << "Loaded " << N << " triangles\n";

    // Build SoA from loaded triangles (original order — LBVH sorts internally)
    auto soa = build_triangle_soa(tris);

    // ── OpenCL setup ──────────────────────────────────────────────────────────
    auto ocl   = create_context();
    cl::CommandQueue queue(ocl.context, ocl.device, CL_QUEUE_PROFILING_ENABLE);

    // ── LBVH builder (loads and compiles all 6 kernels) ───────────────────────
    std::cout << "Compiling LBVH kernels...\n";
    LbvhBuilder builder(ocl.context, ocl.device, queue, wg_size, stack_depth);
    std::cout << "Kernels compiled OK\n";

    // ── GPU SoA buffers (static — not re-uploaded per frame for static scenes)
    SoaGpuBuffers soa_gpu = make_soa_buffers(ocl.context, soa);

    // ── Framebuffer: flat float RGBA buffer
    // WHY cl::Buffer (not image2d_t): traverse.cl uses plain indexing
    // (compatible with all devices); image2d_t requires write_imagef + sampler support.
    // §7.1: size_t promotion
    size_t fb_bytes = static_cast<size_t>(width) * height * 4 * sizeof(float);
    cl::Buffer fb_buf(ocl.context, CL_MEM_WRITE_ONLY, fb_bytes);

    // ── Traversal kernel setup ────────────────────────────────────────────────
    cl::Kernel traverse_k(builder.traverse_program(), "traverse_lbvh");

    // WHY derive TILE_Y from wg_size: NDRange local size must equal WG_SIZE
    // used in the kernel's __local stack declaration to avoid under/over-allocation.
    const size_t TILE_X = 8;
    const size_t TILE_Y = static_cast<size_t>(wg_size) / TILE_X;
    cl::NDRange global(round_up(static_cast<size_t>(width),  TILE_X),
                       round_up(static_cast<size_t>(height), TILE_Y));
    cl::NDRange local(TILE_X, TILE_Y);

    Camera cam;
    LbvhTree tree;

    // ── Accumulate per-frame timings ──────────────────────────────────────────
    double bvh_ms_total    = 0.0;
    double traverse_ms_total = 0.0;

    // Warm-up: build + render once (not counted)
    {
        builder.build(soa, N, tree, /*do_self_test=*/true);
        set_traverse_args(traverse_k, fb_buf, tree.node_buf, tree.sorted_idx_buf,
                          soa_gpu, N, width, height, cam, TILE_X, TILE_Y, stack_depth);
        cl::Event ev;
        CL_CHECK(queue.enqueueNDRangeKernel(traverse_k, cl::NullRange, global, local,
                                             nullptr, &ev));
        CL_CHECK(queue.finish());
    }

    std::cout << "Benchmarking " << frames << " frame(s)...\n";

    for (int f = 0; f < frames; ++f) {
        // ── GPU BVH build (all 5 stages) ─────────────────────────────────────
        TimingBreakdown timing = builder.build(soa, N, tree, /*do_self_test=*/false);
        bvh_ms_total += timing.bvh_build_total_ms();

        // ── GPU traversal / render ────────────────────────────────────────────
        set_traverse_args(traverse_k, fb_buf, tree.node_buf, tree.sorted_idx_buf,
                          soa_gpu, N, width, height, cam, TILE_X, TILE_Y, stack_depth);
        cl::Event ev_traverse;
        CL_CHECK(queue.enqueueNDRangeKernel(traverse_k, cl::NullRange, global, local,
                                             nullptr, &ev_traverse));
        CL_CHECK(queue.finish());

        double t_ms = duration_ms(ev_traverse);
        traverse_ms_total += t_ms;
    }

    // ── Print timing table (task 9.4) ─────────────────────────────────────────
    double bvh_avg      = bvh_ms_total     / frames;
    double traverse_avg = traverse_ms_total / frames;
    double total_avg    = bvh_avg + traverse_avg;
    double fps          = (total_avg > 0.0) ? (1000.0 / total_avg) : 0.0;

    // Also get per-stage breakdown for last frame
    TimingBreakdown last = builder.build(soa, N, tree, false);

    std::cout << "\n";
    std::cout << "Strategy | BVH Build (ms) | Upload (ms) | Render (ms) | Total (ms) | FPS\n";
    std::cout << "---------|----------------|-------------|-------------|------------|----\n";
    std::cout << std::left  << std::setw(9) << "GPU LBVH" << "| "
              << std::right << std::fixed << std::setprecision(3)
              << std::setw(14) << bvh_avg << " | "
              << std::setw(11) << 0.0 << " | "
              << std::setw(11) << traverse_avg << " | "
              << std::setw(10) << total_avg << " | "
              << std::setw(3)  << static_cast<int>(fps) << "\n";
    std::cout << "\n";

    // Per-stage breakdown
    std::cout << "Per-stage breakdown (last frame):\n";
    std::cout << std::fixed << std::setprecision(3);
    std::cout << "  scene_bounds : " << last.scene_bounds_ms << " ms\n";
    std::cout << "  morton       : " << last.morton_ms       << " ms\n";
    std::cout << "  radix_sort   : " << last.sort_ms         << " ms\n";
    std::cout << "  lbvh_build   : " << last.build_ms        << " ms\n";
    std::cout << "  lbvh_aabb    : " << last.aabb_ms         << " ms\n";
    std::cout << "  traverse     : " << duration_ms_traverse(traverse_ms_total, frames) << " ms (avg)\n";
    std::cout << "\n";

    // ── Read back and save framebuffer (task 9.5) ──────────────────────────────
    // §7.1: promote to size_t before multiply
    std::vector<float> pixel_f(static_cast<size_t>(width) * height * 4);
    CL_CHECK(queue.enqueueReadBuffer(fb_buf, CL_TRUE, 0,
                                     pixel_f.size() * sizeof(float),
                                     pixel_f.data()));

    // Task 9.6: pixel sanity checks
    check_framebuffer(pixel_f, width, height);

    // Tonemap float → uint8 and save BMP
    std::vector<uint8_t> pixel_u8(pixel_f.size());
    for (size_t i = 0; i < pixel_f.size(); ++i) {
        float c = pixel_f[i] < 0.0f ? 0.0f : (pixel_f[i] > 1.0f ? 1.0f : pixel_f[i]);
        pixel_u8[i] = static_cast<uint8_t>(c * 255.0f + 0.5f);
    }
    save_bmp(output_path, pixel_u8, width, height, 4);
    std::cout << "Saved: " << output_path << "\n";
}

// Helper: average traverse time — keeps the per-stage line readable
static double duration_ms_traverse(double total_ms, int frames) {
    return (frames > 0) ? total_ms / frames : 0.0;
}

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------
int main(int argc, char* argv[]) {
    CLI::App app{"B4 GPU LBVH Ray Tracer — full GPU BVH build pipeline"};

    int         width       = 800;
    int         height      = 600;
    std::string scene       = "assets/bunny.obj";
    std::string output      = "render.bmp";
    int         frames      = 60;
    int         wg_size     = 64;
    int         stack_depth = 32;
    app.add_option("--width",       width,       "Image width in pixels")                   ->default_val(800);
    app.add_option("--height",      height,      "Image height in pixels")                  ->default_val(600);
    app.add_option("--scene",       scene,       "OBJ scene file path")                     ->default_val("assets/bunny.obj");
    app.add_option("--output",      output,      "Output BMP file path")                    ->default_val("render.bmp");
    app.add_option("--frames",      frames,      "Frames to benchmark")                     ->default_val(60);
    app.add_option("--wg-size",     wg_size,     "Traversal work-group size (multiple of 8)")->default_val(64);
    app.add_option("--stack-depth", stack_depth, "BVH traversal stack depth per work-item") ->default_val(32);

    CLI11_PARSE(app, argc, argv);

    try {
        run(scene, output, width, height, frames, wg_size, stack_depth);
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
