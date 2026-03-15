// main.cpp — B3 Advanced Ray Tracer with Stackless BVH
//
// Loads an OBJ scene, builds a SAH-BVH on the CPU, uploads it to the GPU,
// then renders with stackless BVH traversal (hit_link / miss_link).
//
// Modes:
//   Headless (default): renders to cl::Image2D, writes output BMP.
//   Live (--live):      shared framebuffer via cl_khr_gl_sharing + GLFW window.
//
// CLI args: --width, --height, --scene, --output, --frames, --live
//
// WHY CLI11 first: X11/Xlib.h (pulled in transitively by glfw3native.h)
// defines `#define Success 0`, colliding with CLI11 enum member.
// Including CLI11 before GLFW avoids the preprocessor collision.
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

#ifndef NO_GL_INTEROP
// WHY all EXPOSE_NATIVE_* before the single glfw3native.h include:
// The file uses a global include guard (_glfw3_native_h_); a second include
// after adding more EXPOSE_NATIVE defines would be a no-op.
#  define GLFW_EXPOSE_NATIVE_X11
#  define GLFW_EXPOSE_NATIVE_GLX
#  ifdef HAS_EGL
#    define GLFW_EXPOSE_NATIVE_EGL
#  endif
#  include <GLFW/glfw3.h>
#  include <GLFW/glfw3native.h>
#  ifdef HAS_EGL
#    include <EGL/egl.h>
#  endif
// WHY #undef: X11/Xlib.h defines `Success 0` which shadows C++ identifiers.
#  ifdef Success
#    undef Success
#  endif
#  if defined(__APPLE__)
#    include <OpenGL/gl.h>
#  else
#    include <GL/gl.h>
#  endif
#  include <CL/cl_gl.h>
#endif

#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Camera state (mutable in live mode, fixed in headless mode)
// ---------------------------------------------------------------------------
struct Camera {
    float azimuth   =  0.3f;   // radians, orbit horizontal
    float elevation =  0.3f;   // radians, orbit vertical
    float distance  =  0.3f;   // units from target
    float target[3] = {0.0f, 0.05f, 0.0f};  // look-at point (bunny centroid ~y=0.05)
    float fov_deg   = 45.0f;

    // Derive camera position from spherical coordinates
    void get_pos(float& px, float& py, float& pz) const {
        px = target[0] + distance * std::cos(elevation) * std::sin(azimuth);
        py = target[1] + distance * std::sin(elevation);
        pz = target[2] + distance * std::cos(elevation) * std::cos(azimuth);
    }
};

// ---------------------------------------------------------------------------
// OBJ scene loader — produces TriangleCpu list with normals.
// Returns empty vector on failure (caller must check).
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
    tris.reserve(100000);

    for (const auto& shape : shapes) {
        size_t index_offset = 0;
        for (size_t f = 0; f < shape.mesh.num_face_vertices.size(); ++f) {
            int fv = shape.mesh.num_face_vertices[f];
            if (fv != 3) { index_offset += fv; continue; }  // only triangles

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
                    // Compute face normal when OBJ lacks vertex normals
                    // (filled in below after all 3 vertices are read)
                    tri.n[vv] = {0.0f, 1.0f, 0.0f};  // placeholder
                }
            }

            // Recompute face normal when normals were not provided
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

    if (tris.empty()) throw std::runtime_error("OBJ loaded 0 triangles from: " + path);
    return tris;
}


// ---------------------------------------------------------------------------
// Upload SoA triangle data to 18 cl::Buffer objects (v0x..n2z).
// Returns the buffers in order matching the kernel arg indices.
// WHY 18 separate buffers: each SoA channel uploads independently, so each
// read in the kernel is a contiguous strided access — maximises L1 cache hits.
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
// Set kernel args for the BVH kernel (matches kernel signature exactly).
// Returns arg index after the last argument set.
// ---------------------------------------------------------------------------
static void set_bvh_kernel_args(cl::Kernel& k,
                                 cl::Image2D& fb,   // or ImageGL, same base
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

static void set_naive_kernel_args(cl::Kernel& k,
                                   cl::Image2D& fb,
                                   const SoaBuffers& soa,
                                   int num_tris,
                                   int width, int height,
                                   const Camera& cam)
{
    float px, py, pz;
    cam.get_pos(px, py, pz);

    int a = 0;
    CL_CHECK(k.setArg(a++, fb));
    CL_CHECK(k.setArg(a++, soa.v0x)); CL_CHECK(k.setArg(a++, soa.v0y)); CL_CHECK(k.setArg(a++, soa.v0z));
    CL_CHECK(k.setArg(a++, soa.v1x)); CL_CHECK(k.setArg(a++, soa.v1y)); CL_CHECK(k.setArg(a++, soa.v1z));
    CL_CHECK(k.setArg(a++, soa.v2x)); CL_CHECK(k.setArg(a++, soa.v2y)); CL_CHECK(k.setArg(a++, soa.v2z));
    CL_CHECK(k.setArg(a++, soa.n0x)); CL_CHECK(k.setArg(a++, soa.n0y)); CL_CHECK(k.setArg(a++, soa.n0z));
    CL_CHECK(k.setArg(a++, soa.n1x)); CL_CHECK(k.setArg(a++, soa.n1y)); CL_CHECK(k.setArg(a++, soa.n1z));
    CL_CHECK(k.setArg(a++, soa.n2x)); CL_CHECK(k.setArg(a++, soa.n2y)); CL_CHECK(k.setArg(a++, soa.n2z));
    CL_CHECK(k.setArg(a++, num_tris));
    CL_CHECK(k.setArg(a++, width));
    CL_CHECK(k.setArg(a++, height));
    CL_CHECK(k.setArg(a++, px));  CL_CHECK(k.setArg(a++, py));  CL_CHECK(k.setArg(a++, pz));
    CL_CHECK(k.setArg(a++, cam.target[0])); CL_CHECK(k.setArg(a++, cam.target[1])); CL_CHECK(k.setArg(a++, cam.target[2]));
    CL_CHECK(k.setArg(a++, cam.fov_deg));
}

// ---------------------------------------------------------------------------
// Headless render path
// ---------------------------------------------------------------------------
static void render_headless(const std::string& scene_path,
                             const std::string& output_path,
                             int width, int height, int frames)
{
    // ── Load and validate integer arithmetic (§7.1) ───────────────────────────
    if (width <= 0 || height <= 0)
        throw std::runtime_error("width and height must be positive");
    if (static_cast<size_t>(width) * height > static_cast<size_t>(INT_MAX)) {
        throw std::runtime_error("Image too large (width * height > INT_MAX)");
    }

    std::cout << "Loading OBJ: " << scene_path << "\n";
    auto raw_tris = load_obj(scene_path);
    std::cout << "Loaded " << raw_tris.size() << " triangles\n";

    // ── BVH CPU self-test (must pass before GPU dispatch) ────────────────────
    std::cout << "Running BVH self-test...\n";
    auto t0_test = std::chrono::steady_clock::now();
    bvh_self_test();
    double self_test_ms = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - t0_test).count();
    std::cout << "BVH self-test PASSED (" << std::fixed << std::setprecision(3)
              << self_test_ms << " ms)\n";

    // ── Build BVH ─────────────────────────────────────────────────────────────
    std::cout << "Building SAH-BVH...\n";
    auto t0_bvh = std::chrono::steady_clock::now();
    auto tree = build_bvh(raw_tris, 4);
    double bvh_build_ms = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - t0_bvh).count();
    std::cout << "BVH nodes: " << tree.nodes.size()
              << " for " << tree.sorted_tris.size() << " triangles"
              << "  (built in " << std::fixed << std::setprecision(3)
              << bvh_build_ms << " ms)\n";

    // ── Build SoA triangle buffers ────────────────────────────────────────────
    auto soa = build_triangle_soa(tree.sorted_tris);

    // ── OpenCL setup ─────────────────────────────────────────────────────────
    auto ocl = create_context();
    cl::CommandQueue queue(ocl.context, ocl.device, CL_QUEUE_PROFILING_ENABLE);
    auto prog = build_program(ocl.context, ocl.device,
        (get_binary_dir() / "kernels" / "ray_trace_bvh.cl").string());

    cl::Kernel bvh_kernel(prog, "ray_trace_bvh");
    cl::Kernel naive_kernel(prog, "ray_trace_naive");

    // Upload BVH node buffer
    cl::Buffer nodes_buf(ocl.context,
                          CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                          tree.nodes.size() * sizeof(BvhNode),
                          const_cast<BvhNode*>(tree.nodes.data()));

    // Upload SoA triangle data
    auto soa_bufs = upload_soa(ocl.context, soa);

    cl::ImageFormat fmt(CL_RGBA, CL_FLOAT);
    cl::Image2D fb_naive(ocl.context, CL_MEM_WRITE_ONLY, fmt, width, height);
    cl::Image2D fb_bvh  (ocl.context, CL_MEM_WRITE_ONLY, fmt, width, height);

    Camera cam;  // default fixed camera pose

    constexpr size_t TILE = 16;
    cl::NDRange global(round_up(static_cast<size_t>(width),  TILE),
                       round_up(static_cast<size_t>(height), TILE));
    cl::NDRange local(TILE, TILE);

    // ── Naive baseline (one warmup + one timed frame) ─────────────────────────
    std::cout << "Running naive baseline render...\n";
    set_naive_kernel_args(naive_kernel, fb_naive, soa_bufs, soa.count, width, height, cam);
    // Warmup
    cl::Event ev_warmup;
    CL_CHECK(queue.enqueueNDRangeKernel(naive_kernel, cl::NullRange, global, local, nullptr, &ev_warmup));
    CL_CHECK(queue.finish());
    // Timed
    cl::Event ev_naive;
    CL_CHECK(queue.enqueueNDRangeKernel(naive_kernel, cl::NullRange, global, local, nullptr, &ev_naive));
    CL_CHECK(queue.finish());
    double naive_ms = duration_ms(ev_naive);

    // ── BVH render loop ───────────────────────────────────────────────────────
    std::cout << "Running BVH render (" << frames << " frame(s))...\n";
    double bvh_ms_total = 0.0;

    for (int f = 0; f < frames; ++f) {
        set_bvh_kernel_args(bvh_kernel, fb_bvh, nodes_buf, soa_bufs, soa.count,
                            static_cast<int>(tree.nodes.size()), width, height, cam);
        cl::Event ev_bvh;
        CL_CHECK(queue.enqueueNDRangeKernel(bvh_kernel, cl::NullRange, global, local, nullptr, &ev_bvh));
        CL_CHECK(queue.finish());
        bvh_ms_total += duration_ms(ev_bvh);
    }

    double bvh_ms  = bvh_ms_total / frames;
    double speedup = (bvh_ms > 0.0) ? (naive_ms / bvh_ms) : 0.0;
    double fps     = (bvh_ms > 0.0) ? (1000.0 / bvh_ms) : 0.0;

    std::cout << std::fixed << std::setprecision(3)
              << "Naive render time : " << naive_ms << " ms\n"
              << "BVH render time   : " << bvh_ms   << " ms  (avg over " << frames << " frame(s))\n"
              << "Speedup           : " << std::setprecision(1) << speedup << "x\n"
              << "FPS (BVH)         : " << std::setprecision(1) << fps << "\n";

    // ── Read back last BVH frame and save BMP ─────────────────────────────────
    // §7.1: promote before multiply
    std::vector<float> pixel_f(static_cast<size_t>(width) * height * 4);
    std::array<size_t, 3> origin = {0, 0, 0};
    std::array<size_t, 3> region = {static_cast<size_t>(width),
                                     static_cast<size_t>(height), 1};
    CL_CHECK(queue.enqueueReadImage(fb_bvh, CL_TRUE,
                                    origin, region, 0, 0,
                                    pixel_f.data()));

    std::vector<uint8_t> pixel_u8(pixel_f.size());
    for (size_t i = 0; i < pixel_f.size(); ++i) {
        float c = pixel_f[i] < 0.0f ? 0.0f : (pixel_f[i] > 1.0f ? 1.0f : pixel_f[i]);
        pixel_u8[i] = static_cast<uint8_t>(c * 255.0f + 0.5f);
    }

    save_bmp(output_path, pixel_u8, width, height, 4);
    std::cout << "Saved: " << output_path << "\n";
}

// ---------------------------------------------------------------------------
// Live render path (GL interop). Compiled only when NO_GL_INTEROP is not set.
// ---------------------------------------------------------------------------
#ifndef NO_GL_INTEROP

// Per-window input state (stored as GLFW user pointer)
struct InputState {
    Camera* cam;
    bool    left_down   = false;
    bool    middle_down = false;
    double  last_x = 0.0, last_y = 0.0;
    int     width = 0, height = 0;
};

static void mouse_button_cb(GLFWwindow* w, int button, int action, int /*mods*/) {
    auto* s = static_cast<InputState*>(glfwGetWindowUserPointer(w));
    double cx, cy;
    glfwGetCursorPos(w, &cx, &cy);
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        s->left_down = (action == GLFW_PRESS);
        s->last_x = cx; s->last_y = cy;
    }
    if (button == GLFW_MOUSE_BUTTON_MIDDLE) {
        s->middle_down = (action == GLFW_PRESS);
        s->last_x = cx; s->last_y = cy;
    }
}

static void cursor_pos_cb(GLFWwindow* w, double cx, double cy) {
    auto* s = static_cast<InputState*>(glfwGetWindowUserPointer(w));
    double dx = cx - s->last_x;
    double dy = cy - s->last_y;
    s->last_x = cx; s->last_y = cy;

    if (s->left_down) {
        // Orbit: drag maps to spherical coords
        s->cam->azimuth   += static_cast<float>(dx) * 0.005f;
        s->cam->elevation += static_cast<float>(dy) * 0.005f;
        // Clamp elevation to avoid gimbal lock at poles
        const float EL_MAX = 1.5f;
        s->cam->elevation = std::max(-EL_MAX, std::min(EL_MAX, s->cam->elevation));
    }
    if (s->middle_down) {
        // Pan: move the look-at target in the view plane
        float scale = s->cam->distance * 0.001f;
        // Compute right/up vectors from current camera state
        float px, py, pz;
        s->cam->get_pos(px, py, pz);
        float fx = s->cam->target[0] - px;
        float fy = s->cam->target[1] - py;
        float fz = s->cam->target[2] - pz;
        float flen = std::sqrt(fx*fx+fy*fy+fz*fz);
        if (flen > 1e-8f) { fx/=flen; fy/=flen; fz/=flen; }
        // right = forward × up (world up = (0,1,0))
        float rx = fy*0.0f - fz*1.0f;
        float ry = fz*0.0f - fx*0.0f;
        float rz = fx*1.0f - fy*0.0f;
        float rlen = std::sqrt(rx*rx+ry*ry+rz*rz);
        if (rlen > 1e-8f) { rx/=rlen; ry/=rlen; rz/=rlen; }
        // camera up = right × forward
        float ux = ry*fz - rz*fy;
        float uy = rz*fx - rx*fz;
        float uz = rx*fy - ry*fx;

        s->cam->target[0] -= static_cast<float>(dx) * scale * rx
                           +  static_cast<float>(dy) * scale * ux;
        s->cam->target[1] -= static_cast<float>(dx) * scale * ry
                           +  static_cast<float>(dy) * scale * uy;
        s->cam->target[2] -= static_cast<float>(dx) * scale * rz
                           +  static_cast<float>(dy) * scale * uz;
    }
}

static void scroll_cb(GLFWwindow* w, double /*dx*/, double dy) {
    auto* s = static_cast<InputState*>(glfwGetWindowUserPointer(w));
    // WHY multiplicative zoom: keeps sensitivity proportional to distance
    float factor = 1.0f - static_cast<float>(dy) * 0.05f;
    s->cam->distance = std::max(0.001f, s->cam->distance * factor);
}

static void render_live(const std::string& scene_path,
                         const std::string& output_path,
                         int width, int height)
{
    // ── Load OBJ, BVH self-test, build BVH ───────────────────────────────────
    std::cout << "Loading OBJ: " << scene_path << "\n";
    auto raw_tris = load_obj(scene_path);
    std::cout << "Loaded " << raw_tris.size() << " triangles\n";

    std::cout << "Running BVH self-test...\n";
    auto t0_test = std::chrono::steady_clock::now();
    bvh_self_test();
    double self_test_ms = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - t0_test).count();
    std::cout << "BVH self-test PASSED (" << std::fixed << std::setprecision(3)
              << self_test_ms << " ms)\n";

    std::cout << "Building SAH-BVH...\n";
    auto t0_bvh = std::chrono::steady_clock::now();
    auto tree = build_bvh(raw_tris, 4);
    double bvh_build_ms = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - t0_bvh).count();
    std::cout << "BVH nodes: " << tree.nodes.size()
              << "  (built in " << std::fixed << std::setprecision(3)
              << bvh_build_ms << " ms)\n";
    auto soa = build_triangle_soa(tree.sorted_tris);

    // ── GLFW init ─────────────────────────────────────────────────────────────
    if (!glfwInit()) throw std::runtime_error("glfwInit failed");

    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    GLFWwindow* window = glfwCreateWindow(width, height, "B3 BVH Ray Tracer", nullptr, nullptr);
    if (!window) { glfwTerminate(); throw std::runtime_error("glfwCreateWindow failed"); }
    glfwMakeContextCurrent(window);

    // ── Find CL device backed by current GL context ───────────────────────────
    using clGetGLContextInfoKHR_fn_t =
        cl_int(*)(const cl_context_properties*, cl_gl_context_info,
                  size_t, void*, size_t*);

    std::vector<cl::Platform> platforms;
    cl::Platform::get(&platforms);

    cl::Context cl_ctx;
    cl::Device  gl_device;
    bool        sharing_ok = false;
    bool        egl_path   = false;

    // ── Attempt 1: GLX path (NVIDIA) ─────────────────────────────────────────
    for (auto& p : platforms) {
        cl_context_properties props[] = {
            CL_GL_CONTEXT_KHR,   (cl_context_properties)glfwGetGLXContext(window),
            CL_GLX_DISPLAY_KHR,  (cl_context_properties)glfwGetX11Display(),
            CL_CONTEXT_PLATFORM, (cl_context_properties)(cl_platform_id)p(),
            0
        };
        auto fn = reinterpret_cast<clGetGLContextInfoKHR_fn_t>(
            clGetExtensionFunctionAddressForPlatform(p(), "clGetGLContextInfoKHR"));
        if (!fn) continue;
        cl_device_id dev_id = nullptr;
        cl_int err = fn(props, CL_CURRENT_DEVICE_FOR_GL_CONTEXT_KHR,
                        sizeof(dev_id), &dev_id, nullptr);
        if (err != CL_SUCCESS || !dev_id) continue;
        gl_device = cl::Device(dev_id);
        try {
            cl_ctx     = cl::Context(gl_device, props);
            sharing_ok = true;
            break;
        } catch (const cl::Error&) { continue; }
    }

#ifdef HAS_EGL
    // ── Attempt 2: EGL path (Intel NEO fallback) ──────────────────────────────
    // WHY second window: Intel NEO supports cl_khr_gl_sharing only for EGL-backed
    // contexts. Re-creating with GLFW_EGL_CONTEXT_API gives NEO a valid EGL handle.
    if (!sharing_ok) {
        glfwDestroyWindow(window);
        window = nullptr;
        glfwWindowHint(GLFW_CONTEXT_CREATION_API, GLFW_EGL_CONTEXT_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
        window = glfwCreateWindow(width, height, "B3 BVH Ray Tracer", nullptr, nullptr);
        if (window) {
            glfwMakeContextCurrent(window);
            for (auto& p : platforms) {
                auto fn = reinterpret_cast<clGetGLContextInfoKHR_fn_t>(
                    clGetExtensionFunctionAddressForPlatform(p(), "clGetGLContextInfoKHR"));
                if (!fn) continue;
                cl_context_properties egl_props[] = {
                    CL_GL_CONTEXT_KHR,   (cl_context_properties)glfwGetEGLContext(window),
                    CL_EGL_DISPLAY_KHR,  (cl_context_properties)glfwGetEGLDisplay(),
                    CL_CONTEXT_PLATFORM, (cl_context_properties)(cl_platform_id)p(),
                    0
                };
                cl_device_id dev_id = nullptr;
                cl_int err = fn(egl_props, CL_CURRENT_DEVICE_FOR_GL_CONTEXT_KHR,
                                sizeof(dev_id), &dev_id, nullptr);
                if (err != CL_SUCCESS || !dev_id) continue;
                gl_device = cl::Device(dev_id);
                try {
                    cl_ctx     = cl::Context(gl_device, egl_props);
                    sharing_ok = true;
                    egl_path   = true;
                    std::cout << "[GL-interop] EGL fallback succeeded (Intel NEO path)\n";
                    break;
                } catch (const cl::Error&) { continue; }
            }
        }
    }
#endif  // HAS_EGL

    if (!sharing_ok) {
        std::cout << "[Notice] cl_khr_gl_sharing unavailable on all platforms — "
                     "falling back to headless mode.\n";
        if (window) glfwDestroyWindow(window);
        glfwTerminate();
        render_headless(scene_path, output_path, width, height, 1);
        return;
    }

    // ── GL texture ────────────────────────────────────────────────────────────
    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    if (egl_path) {
        // Intel NEO: clCreateFromGLTexture does not support GL_RGBA32F via EGL.
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height,
                     0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    } else {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, width, height,
                     0, GL_RGBA, GL_FLOAT, nullptr);
    }
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFinish();  // WHY: ensure GL texture allocation completes before CL uses it

    // WHY explicit scope: all CL objects referencing the GL texture must be
    // destroyed before glDeleteTextures / glfwTerminate (§7.6).
    {
        cl::CommandQueue queue(cl_ctx, gl_device, CL_QUEUE_PROFILING_ENABLE);
        auto prog = build_program(cl_ctx, gl_device,
        (get_binary_dir() / "kernels" / "ray_trace_bvh.cl").string());
        cl::Kernel bvh_kernel(prog, "ray_trace_bvh");

        cl::Buffer nodes_buf(cl_ctx,
                              CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                              tree.nodes.size() * sizeof(BvhNode),
                              const_cast<BvhNode*>(tree.nodes.data()));
        auto soa_bufs = upload_soa(cl_ctx, soa);

        // ── CL framebuffer — two strategies (GLX vs EGL — matches B2 pattern) ──
        cl_int cl_err = CL_SUCCESS;
        cl::ImageGL             cl_gl_img;
        cl::Image2D             cl_plain_img;
        std::vector<cl::Memory> gl_objects;
        std::vector<uint8_t>    readback;

        if (egl_path) {
            cl::ImageFormat fmt2(CL_RGBA, CL_UNORM_INT8);
            cl_plain_img = cl::Image2D(cl_ctx, CL_MEM_WRITE_ONLY, fmt2,
                                       width, height, 0, nullptr, &cl_err);
            CL_CHECK(cl_err);
            readback.resize(static_cast<size_t>(width) * height * 4);
        } else {
            cl_gl_img = cl::ImageGL(cl_ctx, CL_MEM_WRITE_ONLY, GL_TEXTURE_2D, 0, tex, &cl_err);
            CL_CHECK(cl_err);
            gl_objects = {cl_gl_img};
        }

        constexpr size_t TILE = 16;
        cl::NDRange global(round_up(static_cast<size_t>(width),  TILE),
                           round_up(static_cast<size_t>(height), TILE));
        cl::NDRange local(TILE, TILE);

        // ── Input state / callbacks ───────────────────────────────────────────
        Camera cam;
        InputState input_state;
        input_state.cam    = &cam;
        input_state.width  = width;
        input_state.height = height;
        // TODO: camera "movements" are average, could be better
        glfwSetWindowUserPointer(window, &input_state);
        glfwSetMouseButtonCallback(window, mouse_button_cb);
        glfwSetCursorPosCallback(window, cursor_pos_cb);
        glfwSetScrollCallback(window, scroll_cb);

        // ── Render loop ───────────────────────────────────────────────────────
        while (!glfwWindowShouldClose(window)) {
            if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) break;

            // Upload camera per-frame (interactive state changes each frame).
            cl::Image2D* active_fb = egl_path ? &cl_plain_img : nullptr;
            if (!egl_path) {
                // Use a temporary Image2D reference from ImageGL base class.
                // We rely on ImageGL being layout-compatible with Image2D for setArg.
                // WHY: the kernel signature takes image2d_t; cl::ImageGL inherits from
                // cl::Image2D so passing cl_gl_img as the arg is type-safe.
                // We can't easily cast here, so we use a cast via Memory base.
                CL_CHECK(queue.enqueueAcquireGLObjects(&gl_objects));
                CL_CHECK(bvh_kernel.setArg(0, cl_gl_img));
            } else {
                CL_CHECK(bvh_kernel.setArg(0, cl_plain_img));
            }

            // Set remaining args (camera may change each frame)
            {
                float px, py, pz;
                cam.get_pos(px, py, pz);
                int a = 1;
                CL_CHECK(bvh_kernel.setArg(a++, nodes_buf));
                CL_CHECK(bvh_kernel.setArg(a++, soa_bufs.v0x)); CL_CHECK(bvh_kernel.setArg(a++, soa_bufs.v0y)); CL_CHECK(bvh_kernel.setArg(a++, soa_bufs.v0z));
                CL_CHECK(bvh_kernel.setArg(a++, soa_bufs.v1x)); CL_CHECK(bvh_kernel.setArg(a++, soa_bufs.v1y)); CL_CHECK(bvh_kernel.setArg(a++, soa_bufs.v1z));
                CL_CHECK(bvh_kernel.setArg(a++, soa_bufs.v2x)); CL_CHECK(bvh_kernel.setArg(a++, soa_bufs.v2y)); CL_CHECK(bvh_kernel.setArg(a++, soa_bufs.v2z));
                CL_CHECK(bvh_kernel.setArg(a++, soa_bufs.n0x)); CL_CHECK(bvh_kernel.setArg(a++, soa_bufs.n0y)); CL_CHECK(bvh_kernel.setArg(a++, soa_bufs.n0z));
                CL_CHECK(bvh_kernel.setArg(a++, soa_bufs.n1x)); CL_CHECK(bvh_kernel.setArg(a++, soa_bufs.n1y)); CL_CHECK(bvh_kernel.setArg(a++, soa_bufs.n1z));
                CL_CHECK(bvh_kernel.setArg(a++, soa_bufs.n2x)); CL_CHECK(bvh_kernel.setArg(a++, soa_bufs.n2y)); CL_CHECK(bvh_kernel.setArg(a++, soa_bufs.n2z));
                CL_CHECK(bvh_kernel.setArg(a++, soa.count));
                CL_CHECK(bvh_kernel.setArg(a++, static_cast<int>(tree.nodes.size())));
                CL_CHECK(bvh_kernel.setArg(a++, width));
                CL_CHECK(bvh_kernel.setArg(a++, height));
                CL_CHECK(bvh_kernel.setArg(a++, px));  CL_CHECK(bvh_kernel.setArg(a++, py));  CL_CHECK(bvh_kernel.setArg(a++, pz));
                CL_CHECK(bvh_kernel.setArg(a++, cam.target[0])); CL_CHECK(bvh_kernel.setArg(a++, cam.target[1])); CL_CHECK(bvh_kernel.setArg(a++, cam.target[2]));
                CL_CHECK(bvh_kernel.setArg(a++, cam.fov_deg));
            }

            cl::Event ev;
            CL_CHECK(queue.enqueueNDRangeKernel(bvh_kernel, cl::NullRange, global, local, nullptr, &ev));

            if (!egl_path) {
                CL_CHECK(queue.enqueueReleaseGLObjects(&gl_objects));
            }
            CL_CHECK(queue.finish());

            double ms = duration_ms(ev);
            double fps = (ms > 0.0) ? (1000.0 / ms) : 0.0;
            std::cout << std::fixed << std::setprecision(3)
                      << "BVH: " << ms << " ms  ("
                      << std::setprecision(1) << fps << " FPS)\r" << std::flush;

            if (egl_path) {
                std::array<size_t,3> origin = {0,0,0};
                std::array<size_t,3> region = {(size_t)width,(size_t)height,1};
                CL_CHECK(queue.enqueueReadImage(cl_plain_img, CL_TRUE,
                                                origin, region, 0, 0, readback.data()));
                glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height,
                                GL_RGBA, GL_UNSIGNED_BYTE, readback.data());
            }

            glClear(GL_COLOR_BUFFER_BIT);
            glEnable(GL_TEXTURE_2D);
            glBindTexture(GL_TEXTURE_2D, tex);
            glBegin(GL_QUADS);
                glTexCoord2f(0,0); glVertex2f(-1,-1);
                glTexCoord2f(1,0); glVertex2f( 1,-1);
                glTexCoord2f(1,1); glVertex2f( 1, 1);
                glTexCoord2f(0,1); glVertex2f(-1, 1);
            glEnd();

            glfwSwapBuffers(window);
            glfwPollEvents();
        }
        std::cout << "\n";
        CL_CHECK(queue.finish());
    }  // ← all CL objects destroyed here while GL context is still alive (§7.6)

    // WHY explicit reset: cl_ctx holds a clReleaseContext ref touching GL interop
    // state. Assign default-constructed Context (refcount → 0) before glfwTerminate.
    cl_ctx = cl::Context();

    glDeleteTextures(1, &tex);
    glfwDestroyWindow(window);
    glfwTerminate();
}

#endif  // !NO_GL_INTEROP

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------
int main(int argc, char* argv[]) {
    CLI::App app{"B3 Advanced Ray Tracer with Stackless BVH"};

    int         width   = 1920;
    int         height  = 1080;
    std::string scene   = "assets/bunny.obj";
    std::string output  = "render.bmp";
    int         frames  = 1;
    bool        live    = false;

    app.add_option("--width",   width,  "Image width in pixels")   ->default_val(1920);
    app.add_option("--height",  height, "Image height in pixels")  ->default_val(1080);
    app.add_option("--scene",   scene,  "OBJ scene file path")     ->default_val("assets/bunny.obj");
    app.add_option("--output",  output, "Headless BMP output path") ->default_val("render.bmp");
    app.add_option("--frames",  frames, "Headless frame count (for timing average)") ->default_val(1);
    app.add_flag  ("--live",    live,
        "Open GLFW window for live rendering (requires GL interop).\n"
        "  Camera controls:\n"
        "    Left-mouse drag   — orbit (azimuth + elevation)\n"
        "    Scroll wheel      — zoom (move along view axis)\n"
        "    Middle-mouse drag — pan (translate look-at target)");

    CLI11_PARSE(app, argc, argv);

    try {
        if (live) {
#ifdef NO_GL_INTEROP
            std::cout << "[Notice] Binary compiled without GL interop (NO_GL_INTEROP). "
                         "Running headless instead.\n";
            render_headless(scene, output, width, height, frames);
#else
            render_live(scene, output, width, height);
#endif
        } else {
            render_headless(scene, output, width, height, frames);
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
