// main.cpp — B2 Basic Ray Tracer
//
// Dispatches a single OpenCL kernel that ray-traces a sphere scene using
// Phong shading. Two output modes:
//   Headless (default): renders to a cl::Image2D, reads back, writes output.bmp
//   Live (--live):      shares the framebuffer with OpenGL via cl_khr_gl_sharing
//
// Build: cmake -B build && cmake --build build
// Run:   ./build/ray_tracer [--width W] [--height H] [--output FILE] [--live]

// WHY CLI11 first: X11/Xlib.h (pulled in by glfw3native.h) defines
// `#define Success 0` as a C macro, which collides with CLI11's
// ExitCodes::Success enum member. Including CLI11 before Xlib avoids
// the preprocessor collision entirely.
#include <CLI/CLI.hpp>

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image.h>
#include <stb_image_write.h>

#include "opencl_utils.hpp"   // CL_CHECK, load_kernel_source, duration_ms
#include "ocl_wrapper.hpp"    // create_context()
#include "image_utils.hpp"    // save_bmp
#include "graphics_hpc_utils.hpp"

#ifndef NO_GL_INTEROP
// WHY GLFW_EXPOSE_NATIVE_*: these macros unlock GLFW's native platform
// accessors (glfwGetGLXContext, glfwGetX11Display) needed to share a GL
// context with OpenCL. Must be defined before glfw3native.h.
#  define GLFW_EXPOSE_NATIVE_X11
#  define GLFW_EXPOSE_NATIVE_GLX
// WHY GLFW_EXPOSE_NATIVE_EGL before the first include of glfw3native.h:
// glfw3native.h uses a file-level include guard (_glfw3_native_h_), so a
// second include after defining GLFW_EXPOSE_NATIVE_EGL would be a no-op.
// All EXPOSE_NATIVE_* defines must precede the single include.
#  ifdef HAS_EGL
#    define GLFW_EXPOSE_NATIVE_EGL
#  endif
#  include <GLFW/glfw3.h>
#  include <GLFW/glfw3native.h>   // glfwGetGLXContext/X11Display + EGL when HAS_EGL
#  ifdef HAS_EGL
#    include <EGL/egl.h>
#  endif
// WHY #undef Success: X11/Xlib.h (included transitively by glfw3native.h)
// defines `Success` as the integer 0. This conflicts with any later C++
// identifier named Success. Undefining it here avoids silent shadowing bugs.
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
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Scene data: 16 spheres, each 8 floats: [cx, cy, cz, radius, R, G, B, shin]
// Positions spread across a wide frustum visible from the default camera.
// ---------------------------------------------------------------------------
static const std::array<float, 16 * 8> SPHERES = {{
    // cx,   cy,   cz,   r,    R,    G,    B,   shin
     0.0f,  0.0f,  2.0f, 0.8f, 0.9f, 0.2f, 0.2f, 32.0f,  // red
    -1.8f,  0.0f,  1.5f, 0.6f, 0.2f, 0.5f, 0.9f, 64.0f,  // blue
     1.8f,  0.0f,  1.5f, 0.6f, 0.2f, 0.9f, 0.3f, 48.0f,  // green
     0.0f, -1.5f,  2.0f, 0.5f, 0.9f, 0.6f, 0.1f, 16.0f,  // yellow
     0.0f,  1.5f,  2.0f, 0.5f, 0.7f, 0.2f, 0.8f, 80.0f,  // purple
    -3.2f,  0.5f,  3.0f, 0.7f, 1.0f, 0.4f, 0.1f, 32.0f,  // orange
     3.2f,  0.5f,  3.0f, 0.7f, 0.1f, 0.8f, 0.8f, 64.0f,  // cyan
    -1.0f, -1.2f,  1.0f, 0.4f, 0.9f, 0.9f, 0.9f, 128.0f, // white-shiny
     1.0f, -1.2f,  1.0f, 0.4f, 0.5f, 0.3f, 0.1f, 8.0f,   // brown-matte
    -2.5f, -0.8f,  2.5f, 0.55f,0.3f, 0.9f, 0.5f, 40.0f,  // mint
     2.5f, -0.8f,  2.5f, 0.55f,0.9f, 0.3f, 0.6f, 40.0f,  // pink
     0.0f,  0.0f,  5.0f, 1.2f, 0.4f, 0.4f, 0.9f, 24.0f,  // big blue-bg
    -0.8f,  0.8f,  0.8f, 0.35f,1.0f, 1.0f, 0.2f, 96.0f,  // lime-shiny
     0.8f,  0.8f,  0.8f, 0.35f,0.8f, 0.1f, 0.1f, 96.0f,  // dark-red-shiny
    -1.5f,  1.5f,  3.5f, 0.65f,0.6f, 0.6f, 0.6f, 32.0f,  // grey
     1.5f,  1.5f,  3.5f, 0.65f,0.2f, 0.7f, 0.9f, 56.0f,  // sky-blue
}};


// ---------------------------------------------------------------------------
// Headless render: writes output_path BMP and prints kernel time.
// ---------------------------------------------------------------------------
static void render_headless(int width, int height,
                             const std::string& output_path) {
    auto ocl = create_context();

    // Profiling-enabled queue — mandatory for cl::Event timing (§6).
    cl::CommandQueue queue(ocl.context, ocl.device, CL_QUEUE_PROFILING_ENABLE);

    cl::Program prog = build_program(ocl.context, ocl.device,
        (get_binary_dir() / "kernels" / "ray_trace.cl").string());
    cl::Kernel  kernel(prog, "ray_trace");

    // WHY cl::Image2D with CL_MEM_WRITE_ONLY: the kernel only writes pixels;
    // read-back happens via enqueueReadImage on the host side.
    cl::ImageFormat fmt(CL_RGBA, CL_FLOAT);
    cl::Image2D framebuffer(ocl.context, CL_MEM_WRITE_ONLY, fmt, width, height);

    cl::Buffer sphere_buf(ocl.context,
                          CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                          SPHERES.size() * sizeof(float),
                          const_cast<float*>(SPHERES.data()));

    int num_spheres = static_cast<int>(SPHERES.size() / 8);
    CL_CHECK(kernel.setArg(0, framebuffer));
    CL_CHECK(kernel.setArg(1, sphere_buf));
    CL_CHECK(kernel.setArg(2, num_spheres));
    CL_CHECK(kernel.setArg(3, width));
    CL_CHECK(kernel.setArg(4, height));

    // WHY padded global: OpenCL requires global to be a multiple of local when
    // local is specified. Padding to ceil(dim/TILE)*TILE lets us use a fixed
    // 16×16 tile; the kernel guard (gid >= width|height) discards excess items.
    constexpr size_t TILE = 16;
    cl::NDRange global(round_up(static_cast<size_t>(width),  TILE),
                       round_up(static_cast<size_t>(height), TILE));
    cl::NDRange local(TILE, TILE);

    cl::Event ev;
    CL_CHECK(queue.enqueueNDRangeKernel(kernel, cl::NullRange, global, local, nullptr, &ev));
    CL_CHECK(queue.finish());

    double ms = duration_ms(ev);
    std::cout << std::fixed << std::setprecision(3)
              << "Frame kernel time: " << ms << " ms\n";

    // Read back the Image2D to a host buffer and tonemap float→uint8.
    // WHY inline here rather than a shared helper: the headless path uses
    // cl::Image2D (required so the same framebuffer can be handed to GL in
    // live mode), while the shared save_framebuffer helper in
    // graphics_hpc_utils.hpp accepts a flat cl::Buffer. Inlining the three
    // lines avoids adding an Image2D overload to the shared header.
    std::array<size_t, 3> origin = {0, 0, 0};
    std::array<size_t, 3> region = {static_cast<size_t>(width),
                                    static_cast<size_t>(height), 1};
    // §7.1: promote to size_t before multiply to avoid signed overflow
    std::vector<float> pixel_f(static_cast<size_t>(width) * height * 4);
    CL_CHECK(queue.enqueueReadImage(framebuffer, CL_TRUE,
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
// Live render (OpenGL interop). Compiled only when NO_GL_INTEROP is NOT set.
// ---------------------------------------------------------------------------
#ifndef NO_GL_INTEROP
static void render_live(int width, int height, const std::string& output_path) {
    if (!glfwInit()) {
        throw std::runtime_error("glfwInit failed");
    }

    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    GLFWwindow* window = glfwCreateWindow(width, height, "B2 Ray Tracer", nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        throw std::runtime_error("glfwCreateWindow failed");
    }
    glfwMakeContextCurrent(window);

    // ── Find the CL device backing the current GL context and create shared ctx ─
    // WHY clGetGLContextInfoKHR instead of extension string scan:
    // A device may advertise cl_khr_gl_sharing in its extension string but still
    // fail clCreateContext if it is not the device backing the current GL context.
    // clGetGLContextInfoKHR(CL_CURRENT_DEVICE_FOR_GL_CONTEXT_KHR) returns exactly
    // the device the driver has associated with the active GLX context, guaranteeing
    // that the subsequent clCreateContext with GL props will succeed.
    using clGetGLContextInfoKHR_fn_t =
        cl_int(*)(const cl_context_properties*, cl_gl_context_info,
                  size_t, void*, size_t*);

    std::vector<cl::Platform> platforms;
    cl::Platform::get(&platforms);

    cl::Context  cl_ctx;
    cl::Device   gl_device;
    bool         sharing_ok = false;
    bool         egl_path   = false;  // true when EGL fallback succeeded

    // ── Attempt 1: GLX path (preserves NVIDIA behaviour) ─────────────────────
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
            cl_ctx    = cl::Context(gl_device, props);
            sharing_ok = true;
            break;
        } catch (const cl::Error&) {
            continue;
        }
    }

#ifdef HAS_EGL
    // ── Attempt 2: EGL path (Intel NEO fallback) ─────────────────────────────
    // WHY second window with GLFW_EGL_CONTEXT_API:
    // Intel NEO (intel-opencl-icd) implements cl_khr_gl_sharing only for EGL-backed GL
    // contexts; it does not implement the GLX variant. Destroying and re-creating the
    // GLFW window with GLFW_EGL_CONTEXT_API is the minimal change that gives NEO a
    // valid EGL display handle without altering the NVIDIA/GLX path (Attempt 1).
    if (!sharing_ok) {
        glfwDestroyWindow(window);
        window = nullptr;
        glfwWindowHint(GLFW_CONTEXT_CREATION_API, GLFW_EGL_CONTEXT_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
        window = glfwCreateWindow(width, height, "B2 Ray Tracer", nullptr, nullptr);
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
                } catch (const cl::Error&) {
                    continue;
                }
            }
        }
    }
#endif  // HAS_EGL

    if (!sharing_ok) {
        std::cout << "[Notice] cl_khr_gl_sharing context creation failed on all platforms — "
                     "falling back to headless mode.\n";
        if (window) glfwDestroyWindow(window);
        glfwTerminate();
        render_headless(width, height, output_path);
        return;
    }
    // ── GL texture that CL will write to directly ─────────────────────────────
    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    // Allocate storage without initial data — CL will fill it each frame.
    // WHY GL_RGBA8 on EGL path: Intel NEO clCreateFromGLTexture does not support
    // GL_RGBA32F (floating-point) internal formats via EGL interop; GL_RGBA8
    // (UNORM_INT8) is universally supported. write_imagef clamps [0,1] → [0,255].
    if (egl_path) {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height,
                     0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    } else {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, width, height,
                     0, GL_RGBA, GL_FLOAT, nullptr);
    }
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFinish();  // WHY glFinish before CL: ensures GL texture allocation is done

    // WHY explicit scope: cl::ImageGL holds a live reference to the GL texture.
    // All CL objects must be destroyed (scope exit) before glDeleteTextures /
    // glfwTerminate — otherwise clReleaseMemObject runs after the GL context
    // is gone and segfaults in the NVIDIA driver.
    {
        cl::CommandQueue queue(cl_ctx, gl_device, CL_QUEUE_PROFILING_ENABLE);

        cl::Program prog = build_program(cl_ctx, gl_device,
        (get_binary_dir() / "kernels" / "ray_trace.cl").string());
        cl::Kernel  kernel(prog, "ray_trace");

        cl::Buffer sphere_buf(cl_ctx,
                              CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                              SPHERES.size() * sizeof(float),
                              const_cast<float*>(SPHERES.data()));

        // ── CL framebuffer — two strategies depending on interop path ─────────────
        // GLX path (NVIDIA): cl::ImageGL wraps the GL texture directly — zero-copy,
        //   CL writes land in VRAM that GL reads without any PCIe transfer.
        // EGL path (Intel NEO): Intel stores GL textures in a GPU-tiled memory layout.
        //   clCreateFromGLTexture succeeds but write_imagef addresses tiles linearly,
        //   producing stripe artifacts. Zero-copy is therefore BROKEN on this driver;
        //   we fall back to cl::Image2D (CL-owned, linear) + enqueueReadImage +
        //   glTexSubImage2D per frame. Kernel time still reflects pure CL cost;
        //   the readback adds ~1–3 ms of host-GPU transfer not shown in the profiling.
        cl_int cl_err = CL_SUCCESS;
        cl::ImageGL             cl_gl_img;    // GLX path only
        cl::Image2D             cl_plain_img; // EGL path only
        std::vector<cl::Memory> gl_objects;   // empty on EGL path
        std::vector<uint8_t>    readback;     // EGL path staging buffer

        if (egl_path) {
            cl::ImageFormat fmt(CL_RGBA, CL_UNORM_INT8);
            cl_plain_img = cl::Image2D(cl_ctx, CL_MEM_WRITE_ONLY, fmt,
                                       width, height, 0, nullptr, &cl_err);
            CL_CHECK(cl_err);
            CL_CHECK(kernel.setArg(0, cl_plain_img));
            readback.resize(static_cast<size_t>(width) * height * 4);
        } else {
            // WHY ImageGL not Image2DGL: Image2DGL is deprecated in OpenCL 1.2;
            // ImageGL wraps clCreateFromGLTexture which handles both 2D and 3D.
            cl_gl_img = cl::ImageGL(cl_ctx, CL_MEM_WRITE_ONLY, GL_TEXTURE_2D, 0, tex, &cl_err);
            CL_CHECK(cl_err);
            CL_CHECK(kernel.setArg(0, cl_gl_img));
            // TODO: check if this can be fixed
            // WHY vector<Memory>: enqueueAcquireGLObjects takes a vector of cl::Memory
            // base-class objects; ImageGL is a subclass of Memory.
            gl_objects = {cl_gl_img};
        }

        int num_spheres = static_cast<int>(SPHERES.size() / 8);
        CL_CHECK(kernel.setArg(1, sphere_buf));
        CL_CHECK(kernel.setArg(2, num_spheres));
        CL_CHECK(kernel.setArg(3, width));
        CL_CHECK(kernel.setArg(4, height));

        constexpr size_t TILE = 16;
        cl::NDRange global(round_up(static_cast<size_t>(width),  TILE),
                           round_up(static_cast<size_t>(height), TILE));
        cl::NDRange local(TILE, TILE);

        // ── Render loop ───────────────────────────────────────────────────────────
        while (!glfwWindowShouldClose(window)) {
            if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) break;

            if (!egl_path) {
                // Acquire ownership of the GL texture for CL writes.
                CL_CHECK(queue.enqueueAcquireGLObjects(&gl_objects));
            }

            cl::Event ev;
            CL_CHECK(queue.enqueueNDRangeKernel(kernel, cl::NullRange, global, local, nullptr, &ev));

            if (!egl_path) {
                CL_CHECK(queue.enqueueReleaseGLObjects(&gl_objects));
            }
            CL_CHECK(queue.finish());

            double ms = duration_ms(ev);
            std::cout << std::fixed << std::setprecision(3)
                      << "Frame kernel time: " << ms << " ms\r" << std::flush;

            if (egl_path) {
                // Read CL image back and upload to the GL texture (breaks zero-copy —
                // see comment above; correctness wins over performance on this path).
                std::array<size_t, 3> origin = {0, 0, 0};
                std::array<size_t, 3> region = {(size_t)width, (size_t)height, 1};
                CL_CHECK(queue.enqueueReadImage(cl_plain_img, CL_TRUE,
                                                origin, region, 0, 0, readback.data()));
                glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height,
                                GL_RGBA, GL_UNSIGNED_BYTE, readback.data());
            }

            // Blit the CL-written texture to the full screen quad via legacy GL.
            glClear(GL_COLOR_BUFFER_BIT);
            glEnable(GL_TEXTURE_2D);
            glBindTexture(GL_TEXTURE_2D, tex);
            glBegin(GL_QUADS);
                glTexCoord2f(0, 0); glVertex2f(-1, -1);
                glTexCoord2f(1, 0); glVertex2f( 1, -1);
                glTexCoord2f(1, 1); glVertex2f( 1,  1);
                glTexCoord2f(0, 1); glVertex2f(-1,  1);
            glEnd();

            glfwSwapBuffers(window);
            glfwPollEvents();
        }
        std::cout << "\n";
        CL_CHECK(queue.finish());
    } // ← CL objects (cl_img, queue, kernel, prog, sphere_buf) destroyed here,
      //   while GL context is still alive.

    // WHY explicit reset: cl_ctx holds a clReleaseContext ref that touches
    // GL interop state. Assigning a default-constructed Context releases it
    // (refcount → 0) before glfwTerminate destroys the GL context.
    cl_ctx = cl::Context();

    glDeleteTextures(1, &tex);
    glfwDestroyWindow(window);
    glfwTerminate();
}
#endif  // NO_GL_INTEROP

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------
int main(int argc, char* argv[]) {
    CLI::App app{"B2 Basic Ray Tracer — OpenCL sphere scene renderer"};

    int         width  = 1280;
    int         height = 720;
    std::string output = "output.bmp";
    bool        live   = false;

    app.add_option("--width",  width,  "Image width in pixels")  ->default_val(1280);
    app.add_option("--height", height, "Image height in pixels") ->default_val(720);
    app.add_option("--output", output, "Output BMP file path")   ->default_val("output.bmp");
    app.add_flag  ("--live",   live,   "Open GLFW window for live rendering (requires GL interop)");

    CLI11_PARSE(app, argc, argv);

    try {
        if (live) {
#ifdef NO_GL_INTEROP
            std::cout << "[Notice] Binary was compiled without GL interop "
                         "(NO_GL_INTEROP). Running headless instead.\n";
            render_headless(width, height, output);
#else
            render_live(width, height, output);
#endif
        } else {
            render_headless(width, height, output);
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
