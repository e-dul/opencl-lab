// main.cpp — B3 Dynamic Scene: BVH Rebuild vs Refit Benchmark
//
// Loads an OBJ scene once, then animates it by rotating triangle vertices
// each frame (Y-axis rigid rotation). Compares three BVH strategies:
//
//   rebuild : Full SAH-BVH reconstruction every frame (most accurate, slowest)
//   refit   : Bottom-up AABB expansion without re-sorting (fast, slight quality loss)
//   static  : No BVH update — intentionally stale BVH shows rendering artifacts
//
// After --frames frames, prints a timing table (BVH build, GPU upload, GPU render).
// Saves final frame as --output render.bmp.
//
// WHY CLI11 first: X11/Xlib.h (pulled in transitively) defines `#define Success 0`,
// colliding with CLI11 enum member. Including CLI11 before any X11-bearing header avoids.
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

#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Camera (fixed in benchmark mode)
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
// OBJ loader
// ---------------------------------------------------------------------------
static std::vector<TriangleCpu> load_obj(const std::string& path) {
    tinyobj::ObjReaderConfig cfg;
    cfg.triangulate = true;
    tinyobj::ObjReader reader;
    if (!reader.ParseFromFile(path, cfg))
        throw std::runtime_error("tinyobjloader failed: " + reader.Error());
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
                // §7.1: cast to size_t before multiply
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
                    if (len < 1e-8f) return {0.0f, 1.0f, 0.0f};
                    return {a[0]/len, a[1]/len, a[2]/len};
                };
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
// Rotate all triangle vertices around the Y-axis by angle_rad.
//
// WHY immutable source: the original OBJ triangles are loaded once. Each frame
// a working copy is produced by this function so we can return to frame 0
// cleanly without accumulating floating-point drift across frames.
// ---------------------------------------------------------------------------
static std::vector<TriangleCpu> rotate_vertices(const std::vector<TriangleCpu>& src,
                                                  float angle_rad)
{
    float cos_a = std::cos(angle_rad);
    float sin_a = std::sin(angle_rad);

    std::vector<TriangleCpu> dst = src;  // copy normals and original_index

    for (auto& tri : dst) {
        for (int v = 0; v < 3; ++v) {
            float x = tri.v[v][0];
            float z = tri.v[v][2];
            // Y-axis rotation: x' = x*cos - z*sin, z' = x*sin + z*cos
            tri.v[v][0] = x * cos_a - z * sin_a;
            tri.v[v][2] = x * sin_a + z * cos_a;
        }
        // Rotate normals too so shading remains correct
        for (int v = 0; v < 3; ++v) {
            float nx = tri.n[v][0];
            float nz = tri.n[v][2];
            tri.n[v][0] = nx * cos_a - nz * sin_a;
            tri.n[v][2] = nx * sin_a + nz * cos_a;
        }
    }
    return dst;
}


// ---------------------------------------------------------------------------
// SoA buffer container for GPU
// ---------------------------------------------------------------------------
struct SoaBuffers {
    cl::Buffer v0x, v0y, v0z;
    cl::Buffer v1x, v1y, v1z;
    cl::Buffer v2x, v2y, v2z;
    cl::Buffer n0x, n0y, n0z;
    cl::Buffer n1x, n1y, n1z;
    cl::Buffer n2x, n2y, n2z;
};

// Initial upload — creates GPU buffers (CL_MEM_COPY_HOST_PTR).
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
// Re-upload SoA position channels from mutated TriangleSoa into existing buffers.
// Returns upload time in ms measured via cl::Event (GPU profiling).
// WHY only positions: normals also rotate, so all 18 channels are re-uploaded.
// WHY enqueueWriteBuffer with events: satisfies §6 timing instrumentation.
//
// WHY 18 sequential enqueueWriteBuffer + finish calls (not batched):
//   Each channel is an independent std::vector<float> in host memory, so there
//   is no single contiguous source pointer for a batched transfer. Batching would
//   require either interleaving the SoA data into AoS layout (defeating the
//   coalescing benefit) or adding a staging buffer and an additional copy per
//   frame. The 18-call serialisation is a deliberate trade-off: it keeps the
//   host-side SoA layout simple and the per-channel events individually
//   profileable, at the cost of 18× queue-submission overhead (~0.01 ms each
//   on discrete GPU). For scenes where upload dominates, a single pinned staging
//   buffer with one enqueueWriteBuffer call would reduce that overhead.
// ---------------------------------------------------------------------------
static double reupload_soa(cl::CommandQueue& q,
                            SoaBuffers& bufs,
                            const TriangleSoa& soa)
{
    auto write = [&](cl::Buffer& buf, const std::vector<float>& v) -> double {
        cl::Event ev;
        CL_CHECK(q.enqueueWriteBuffer(buf, CL_FALSE, 0,
                                       v.size() * sizeof(float),
                                       v.data(), nullptr, &ev));
        CL_CHECK(q.finish());
        return duration_ms(ev);
    };

    double ms = 0.0;
    ms += write(bufs.v0x, soa.v0x); ms += write(bufs.v0y, soa.v0y); ms += write(bufs.v0z, soa.v0z);
    ms += write(bufs.v1x, soa.v1x); ms += write(bufs.v1y, soa.v1y); ms += write(bufs.v1z, soa.v1z);
    ms += write(bufs.v2x, soa.v2x); ms += write(bufs.v2y, soa.v2y); ms += write(bufs.v2z, soa.v2z);
    ms += write(bufs.n0x, soa.n0x); ms += write(bufs.n0y, soa.n0y); ms += write(bufs.n0z, soa.n0z);
    ms += write(bufs.n1x, soa.n1x); ms += write(bufs.n1y, soa.n1y); ms += write(bufs.n1z, soa.n1z);
    ms += write(bufs.n2x, soa.n2x); ms += write(bufs.n2y, soa.n2y); ms += write(bufs.n2z, soa.n2z);
    return ms;
}

// Re-upload BVH nodes buffer. Returns upload time in ms.
static double reupload_nodes(cl::CommandQueue& q, cl::Buffer& buf,
                              const std::vector<BvhNode>& nodes)
{
    cl::Event ev;
    CL_CHECK(q.enqueueWriteBuffer(buf, CL_FALSE, 0,
                                   nodes.size() * sizeof(BvhNode),
                                   nodes.data(), nullptr, &ev));
    CL_CHECK(q.finish());
    return duration_ms(ev);
}

// ---------------------------------------------------------------------------
// Set all kernel args for ray_trace_bvh
// ---------------------------------------------------------------------------
static void set_bvh_kernel_args(cl::Kernel& k,
                                 cl::Image2D& fb,
                                 const cl::Buffer& nodes_buf,
                                 const SoaBuffers& soa,
                                 int num_tris,
                                 int num_nodes,
                                 int width, int height,
                                 const Camera& cam)
{
    float px, py, pz;
    cam.get_pos(px, py, pz);

    int a = 0;
    CL_CHECK(k.setArg(a++, fb));
    CL_CHECK(k.setArg(a++, nodes_buf));
    CL_CHECK(k.setArg(a++, soa.v0x)); CL_CHECK(k.setArg(a++, soa.v0y)); CL_CHECK(k.setArg(a++, soa.v0z));
    CL_CHECK(k.setArg(a++, soa.v1x)); CL_CHECK(k.setArg(a++, soa.v1y)); CL_CHECK(k.setArg(a++, soa.v1z));
    CL_CHECK(k.setArg(a++, soa.v2x)); CL_CHECK(k.setArg(a++, soa.v2y)); CL_CHECK(k.setArg(a++, soa.v2z));
    CL_CHECK(k.setArg(a++, soa.n0x)); CL_CHECK(k.setArg(a++, soa.n0y)); CL_CHECK(k.setArg(a++, soa.n0z));
    CL_CHECK(k.setArg(a++, soa.n1x)); CL_CHECK(k.setArg(a++, soa.n1y)); CL_CHECK(k.setArg(a++, soa.n1z));
    CL_CHECK(k.setArg(a++, soa.n2x)); CL_CHECK(k.setArg(a++, soa.n2y)); CL_CHECK(k.setArg(a++, soa.n2z));
    CL_CHECK(k.setArg(a++, num_tris));
    CL_CHECK(k.setArg(a++, num_nodes));
    CL_CHECK(k.setArg(a++, width));
    CL_CHECK(k.setArg(a++, height));
    CL_CHECK(k.setArg(a++, px));  CL_CHECK(k.setArg(a++, py));  CL_CHECK(k.setArg(a++, pz));
    CL_CHECK(k.setArg(a++, cam.target[0])); CL_CHECK(k.setArg(a++, cam.target[1])); CL_CHECK(k.setArg(a++, cam.target[2]));
    CL_CHECK(k.setArg(a++, cam.fov_deg));
}

// ---------------------------------------------------------------------------
// Main benchmark path (headless only in this module)
// ---------------------------------------------------------------------------
static void run_benchmark(const std::string& scene_path,
                           const std::string& output_path,
                           const std::string& strategy,
                           int width, int height,
                           int frames,
                           int max_depth)
{
    if (width <= 0 || height <= 0)
        throw std::runtime_error("width and height must be positive");
    // §7.1: guard integer overflow before passing pixel count as cl_int
    if (static_cast<size_t>(width) * height > static_cast<size_t>(INT_MAX))
        throw std::runtime_error("Image too large (width * height > INT_MAX)");

    std::cout << "Loading OBJ: " << scene_path << "\n";
    const auto original_tris = load_obj(scene_path);
    std::cout << "Loaded " << original_tris.size() << " triangles\n";

    // ── Run CPU self-tests ────────────────────────────────────────────────────
    std::cout << "Running BVH self-test...\n";
    {
        auto t0 = std::chrono::steady_clock::now();
        bvh_self_test();
        double ms = std::chrono::duration<double,std::milli>(
            std::chrono::steady_clock::now() - t0).count();
        std::cout << "BVH self-test PASSED (" << std::fixed << std::setprecision(3) << ms << " ms)\n";
    }
    std::cout << "Running BVH refit self-test...\n";
    {
        auto t0 = std::chrono::steady_clock::now();
        bvh_refit_self_test();
        double ms = std::chrono::duration<double,std::milli>(
            std::chrono::steady_clock::now() - t0).count();
        std::cout << "BVH refit self-test PASSED (" << std::fixed << std::setprecision(3) << ms << " ms)\n";
    }

    // ── Build initial BVH from original pose ─────────────────────────────────
    std::cout << "Building initial SAH-BVH (leaf_max=4, max_depth=" << max_depth << ")...\n";
    auto t0_bvh = std::chrono::steady_clock::now();
    auto tree = build_bvh(std::vector<TriangleCpu>(original_tris), 4, max_depth);
    double initial_build_ms = std::chrono::duration<double,std::milli>(
        std::chrono::steady_clock::now() - t0_bvh).count();
    std::cout << "BVH nodes: " << tree.nodes.size()
              << " for " << tree.sorted_tris.size() << " triangles"
              << "  (built in " << std::fixed << std::setprecision(3)
              << initial_build_ms << " ms)\n";

    // ── OpenCL setup ─────────────────────────────────────────────────────────
    auto ocl = create_context();
    cl::CommandQueue queue(ocl.context, ocl.device, CL_QUEUE_PROFILING_ENABLE);
    auto prog = build_program(ocl.context, ocl.device,
        (get_binary_dir() / "kernels" / "ray_trace_bvh.cl").string());
    cl::Kernel bvh_kernel(prog, "ray_trace_bvh");

    // Pre-allocate GPU BVH node buffer with worst-case capacity.
    // WHY 2*N upper bound: a SAH-BVH over N triangles produces at most 2*N-1 nodes
    // (complete binary tree). Rebuild may produce a different node count each frame,
    // so we size the buffer once to the upper bound and write only
    // working_tree.nodes.size() bytes per frame — always within capacity.
    // WHY no CL_MEM_COPY_HOST_PTR: the host vector covers only tree.nodes.size()
    // elements; passing the larger max_node_capacity as size with COPY_HOST_PTR would
    // read past the end of the vector (UB). We upload the initial data explicitly.
    const size_t max_node_capacity = 2 * original_tris.size() * sizeof(BvhNode);
    cl::Buffer nodes_buf(ocl.context, CL_MEM_READ_ONLY, max_node_capacity);
    CL_CHECK(queue.enqueueWriteBuffer(nodes_buf, CL_TRUE, 0,
                                       tree.nodes.size() * sizeof(BvhNode),
                                       tree.nodes.data()));

    // Build initial SoA and upload
    auto soa_init = build_triangle_soa(tree.sorted_tris);
    auto soa_bufs = upload_soa(ocl.context, soa_init);

    // Framebuffer
    cl::ImageFormat fmt(CL_RGBA, CL_FLOAT);
    cl::Image2D fb(ocl.context, CL_MEM_WRITE_ONLY, fmt, width, height);

    Camera cam;
    constexpr size_t TILE = 16;
    cl::NDRange global(round_up(static_cast<size_t>(width),  TILE),
                       round_up(static_cast<size_t>(height), TILE));
    cl::NDRange local(TILE, TILE);

    // ── Benchmark loop ────────────────────────────────────────────────────────
    // Accumulate per-frame timings for averaging.
    double build_ms_total  = 0.0;
    double upload_ms_total = 0.0;
    double render_ms_total = 0.0;

    // For static strategy: build_ms and upload_ms are always 0 (BVH never updated).
    // The static BVH was already uploaded above — no re-upload per frame.
    const bool is_rebuild = (strategy == "rebuild");
    const bool is_refit   = (strategy == "refit");
    const bool is_static  = (strategy == "static");
    (void)is_static;  // suppress unused-variable warning when no explicit branch taken

    std::cout << "Benchmarking strategy='" << strategy << "' over " << frames << " frame(s)...\n";

    // Warm-up frame (not counted in timing)
    {
        set_bvh_kernel_args(bvh_kernel, fb, nodes_buf, soa_bufs,
                            soa_init.count, static_cast<int>(tree.nodes.size()),
                            width, height, cam);
        cl::Event ev_warmup;
        CL_CHECK(queue.enqueueNDRangeKernel(bvh_kernel, cl::NullRange, global, local, nullptr, &ev_warmup));
        CL_CHECK(queue.finish());
    }

    // Keep a working copy of sorted_tris for refit/rebuild (mutated each frame).
    // For static: tree.sorted_tris is unused after the first upload.
    BvhTree working_tree = tree;  // deep copy of nodes + sorted_tris

    for (int f = 0; f < frames; ++f) {
        // WHY half-revolution: full 2π/frames returns geometry to origin at frame N,
        // realigning with the static BVH and hiding divergence artifacts.
        // π/frames gives 180° total rotation — maximum divergence at the final frame.
        float angle = static_cast<float>(f) * (3.14159265358979f / frames);
        auto rotated = rotate_vertices(original_tris, angle);

        double build_ms  = 0.0;
        double upload_ms = 0.0;

        if (is_rebuild) {
            // ── Full SAH rebuild ────────────────────────────────────────────
            auto t0 = std::chrono::steady_clock::now();
            working_tree = build_bvh(rotated, 4, max_depth);
            build_ms = std::chrono::duration<double,std::milli>(
                std::chrono::steady_clock::now() - t0).count();

            // Upload new BVH topology + new triangle positions
            auto soa_frame = build_triangle_soa(working_tree.sorted_tris);
            upload_ms += reupload_nodes(queue, nodes_buf, working_tree.nodes);
            upload_ms += reupload_soa(queue, soa_bufs, soa_frame);

            // Update kernel arg for num_nodes (may change with different leaf_max paths)
            set_bvh_kernel_args(bvh_kernel, fb, nodes_buf, soa_bufs,
                                soa_frame.count, static_cast<int>(working_tree.nodes.size()),
                                width, height, cam);

        } else if (is_refit) {
            // ── Refit: update sorted_tris in-place, then refit BVH topology ──
            // WHY we must also rotate sorted_tris (not just original_tris):
            // sorted_tris stores the per-BVH-leaf ordering; refit reads from it.
            // We re-derive sorted_tris by applying the rotation to the original
            // tris mapped through the existing leaf ordering (original_index).
            auto t0 = std::chrono::steady_clock::now();

            // Rebuild the sorted_tris by rotating the source triangles in leaf order.
            // working_tree.sorted_tris[i].original_index tells us which source tri.
            for (size_t i = 0; i < working_tree.sorted_tris.size(); ++i) {
                int src_idx = working_tree.sorted_tris[i].original_index;
                working_tree.sorted_tris[i] = rotated[src_idx];
                working_tree.sorted_tris[i].original_index = src_idx;
            }

            // Refit AABBs bottom-up
            refit(working_tree);
            build_ms = std::chrono::duration<double,std::milli>(
                std::chrono::steady_clock::now() - t0).count();

            // Upload updated BVH nodes + triangle positions
            auto soa_frame = build_triangle_soa(working_tree.sorted_tris);
            upload_ms += reupload_nodes(queue, nodes_buf, working_tree.nodes);
            upload_ms += reupload_soa(queue, soa_bufs, soa_frame);

            set_bvh_kernel_args(bvh_kernel, fb, nodes_buf, soa_bufs,
                                soa_frame.count, static_cast<int>(working_tree.nodes.size()),
                                width, height, cam);
        } else {
            // ── Static: BVH stays fixed, but triangle positions ARE updated each frame.
            // WHY: the educational point is that the BVH (node AABBs) no longer encloses
            // the actual triangles — divergence causes missed intersections (black patches).
            // If we also skip uploading rotated positions, both BVH and geometry stay at
            // frame 0 and the kernel always renders the same image with no artifacts.
            // WHY tree.sorted_tris order: BVH leaf nodes index into the SoA buffer using
            // sorted_tris positions. Must apply rotation through the same ordering.
            std::vector<TriangleCpu> static_sorted(tree.sorted_tris.size());
            for (size_t i = 0; i < tree.sorted_tris.size(); ++i) {
                int src_idx = tree.sorted_tris[i].original_index;
                static_sorted[i] = rotated[src_idx];
                static_sorted[i].original_index = src_idx;
            }
            auto soa_rotated = build_triangle_soa(static_sorted);
            reupload_soa(queue, soa_bufs, soa_rotated);      // upload_ms intentionally not timed
            set_bvh_kernel_args(bvh_kernel, fb, nodes_buf, soa_bufs,
                                soa_rotated.count, static_cast<int>(tree.nodes.size()),
                                width, height, cam);
        }

        // ── GPU render ────────────────────────────────────────────────────────
        cl::Event ev_render;
        CL_CHECK(queue.enqueueNDRangeKernel(bvh_kernel, cl::NullRange, global, local,
                                             nullptr, &ev_render));
        CL_CHECK(queue.finish());
        double render_ms = duration_ms(ev_render);

        build_ms_total  += build_ms;
        upload_ms_total += upload_ms;
        render_ms_total += render_ms;
    }

    // ── Compute averages and print table ─────────────────────────────────────
    double build_avg  = build_ms_total  / frames;
    double upload_avg = upload_ms_total / frames;
    double render_avg = render_ms_total / frames;
    double total_avg  = build_avg + upload_avg + render_avg;
    double fps        = (total_avg > 0.0) ? (1000.0 / total_avg) : 0.0;

    // Depth label: show actual leaf_max-derived cap or "unlimited"
    std::string depth_label = (max_depth > 0) ? std::to_string(max_depth) : "unlimited";

    std::cout << "\n";
    std::cout << "Strategy | Depth     | BVH Build (ms) | Upload (ms) | Render (ms) | Total (ms) | FPS\n";
    std::cout << "---------|-----------|----------------|-------------|-------------|------------|----\n";
    std::cout << std::left  << std::setw(9) << strategy << "| "
              << std::right << std::setw(9) << depth_label << " | "
              << std::fixed << std::setprecision(2) << std::setw(14) << build_avg  << " | "
              << std::setw(11) << upload_avg << " | "
              << std::setw(11) << render_avg << " | "
              << std::setw(10) << total_avg  << " | "
              << std::setw(3)  << static_cast<int>(fps) << "\n";
    std::cout << "\n";

    // ── Save final frame BMP ─────────────────────────────────────────────────
    // §7.1: promote before multiply
    std::vector<float> pixel_f(static_cast<size_t>(width) * height * 4);
    std::array<size_t, 3> origin = {0, 0, 0};
    std::array<size_t, 3> region = {static_cast<size_t>(width),
                                     static_cast<size_t>(height), 1};
    CL_CHECK(queue.enqueueReadImage(fb, CL_TRUE, origin, region, 0, 0, pixel_f.data()));

    std::vector<uint8_t> pixel_u8(pixel_f.size());
    for (size_t i = 0; i < pixel_f.size(); ++i) {
        float c = pixel_f[i] < 0.0f ? 0.0f : (pixel_f[i] > 1.0f ? 1.0f : pixel_f[i]);
        pixel_u8[i] = static_cast<uint8_t>(c * 255.0f + 0.5f);
    }

    save_bmp(output_path, pixel_u8, width, height, 4);
    std::cout << "Saved: " << output_path << "\n";
}

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------
int main(int argc, char* argv[]) {
    CLI::App app{"B3 Dynamic Scene — BVH Rebuild vs Refit Benchmark"};

    int         width     = 800;
    int         height    = 600;
    std::string scene     = "assets/bunny.obj";
    std::string output    = "render.bmp";
    std::string strategy  = "rebuild";
    int         frames    = 60;
    int         max_depth = 0;  // 0 = unlimited

    app.add_option("--width",     width,    "Image width in pixels")         ->default_val(800);
    app.add_option("--height",    height,   "Image height in pixels")        ->default_val(600);
    app.add_option("--scene",     scene,    "OBJ scene file path")           ->default_val("assets/bunny.obj");
    app.add_option("--output",    output,   "Output BMP file path")          ->default_val("render.bmp");
    app.add_option("--strategy",  strategy, "BVH strategy: rebuild|refit|static")
        ->default_val("rebuild")
        ->check(CLI::IsMember({"rebuild", "refit", "static"}));
    app.add_option("--frames",    frames,   "Number of frames to benchmark") ->default_val(60);
    app.add_option("--max-depth", max_depth,
        "Cap BVH tree depth (0 = unlimited). Shallow tree → more triangles per leaf → slower render")
        ->default_val(0);

    CLI11_PARSE(app, argc, argv);

    try {
        run_benchmark(scene, output, strategy, width, height, frames, max_depth);
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
