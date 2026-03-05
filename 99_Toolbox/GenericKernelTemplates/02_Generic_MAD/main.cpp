// 02_generic_mad — Generic MAD kernel: same source compiled for uchar AND float.
//
// WHY this sub-step exists:
//   Sub-step 01 hard-coded a single type. Here we compile mad_kernel.cl twice
//   (one cl::Program per type), run both, and compare timing. This is the
//   key insight of GenericKernelTemplates: one source file → multiple type
//   specialisations with zero code duplication in the kernel.
//
// Two-row timing table:
//   Type    | Kernel Time (ms)
//   --------|------------------
//   uchar   |            0.312
//   float   |            0.487
//
// output.bmp is written from the float variant (full-precision result).

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image.h>
#include <stb_image_write.h>

#include "opencl_utils.hpp"
#include "ocl_wrapper.hpp"
#include "image_utils.hpp"

#include <CLI/CLI.hpp>

#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// build_program — compile kernel source with given options.
// Prints build log and rethrows on error so callers get a clean message.
// ---------------------------------------------------------------------------
static cl::Program build_program(const cl::Context& ctx,
                                 const cl::Device&  dev,
                                 const std::string& src,
                                 const std::string& options)
{
    cl::Program prog(ctx, cl::Program::Sources{src});
    try {
        prog.build({dev}, options.c_str());
    } catch (const cl::Error&) {
        std::cerr << "Build log (" << options << "):\n"
                  << prog.getBuildInfo<CL_PROGRAM_BUILD_LOG>(dev) << "\n";
        throw;
    }
    return prog;
}

// ---------------------------------------------------------------------------
// run_variant — dispatch mad_kernel, return cl::Event kernel time in ms.
// WHY template on element size: uchar is 1 byte, float is 4 bytes, so the
// buffer byte size differs even though pixel count is identical.
// ---------------------------------------------------------------------------
static double run_variant(cl::CommandQueue& queue,
                          cl::Program&      prog,
                          cl::Buffer&       buf_in,   // typed input on device
                          cl::Buffer&       buf_out,  // typed output on device
                          float             contrast,
                          float             brightness,
                          cl_uint           n_elements)
{
    cl::Kernel kernel(prog, "mad_kernel");
    CL_CHECK(kernel.setArg(0, buf_out));
    CL_CHECK(kernel.setArg(1, buf_in));
    CL_CHECK(kernel.setArg(2, contrast));
    CL_CHECK(kernel.setArg(3, brightness));
    CL_CHECK(kernel.setArg(4, n_elements));

    cl::Event evt;
    CL_CHECK(queue.enqueueNDRangeKernel(
        kernel,
        cl::NullRange,
        cl::NDRange(static_cast<size_t>(n_elements)),
        cl::NullRange,
        nullptr,
        &evt));
    CL_CHECK(queue.finish());

    return duration_ms(evt);
}

int main(int argc, char** argv)
{
    // ── CLI ──────────────────────────────────────────────────────────────────
    CLI::App app{"02_generic_mad — uchar vs float MAD kernel timing comparison"};

    std::string image_path;
    int         width      = 1920;
    int         height     = 1080;
    float       contrast   = 1.2f;
    float       brightness = -10.0f;

    app.add_option("--image",      image_path, "Input BMP/PNG path (gradient used if absent)");
    app.add_option("--width",      width,      "Gradient width  (default 1920)");
    app.add_option("--height",     height,     "Gradient height (default 1080)");
    app.add_option("--contrast",   contrast,   "Contrast multiplier (default 1.2)");
    app.add_option("--brightness", brightness, "Brightness addend   (default -10)");

    CLI11_PARSE(app, argc, argv);

    // ── Input image ──────────────────────────────────────────────────────────
    int channels = 3;
    std::vector<uint8_t> host_in_u8;

    if (image_path.empty()) {
        std::cout << "No --image provided; using " << width << "x" << height
                  << " synthetic gradient.\n";
        host_in_u8 = make_gradient(width, height, channels);
    } else {
        host_in_u8 = load_rgb_image(image_path, width, height, channels);
        std::cout << "Loaded: " << image_path << " (" << width << "x" << height
                  << ", " << channels << "ch)\n";
    }

    const size_t n_pixels   = static_cast<size_t>(width) * static_cast<size_t>(height);
    const size_t n_elements = n_pixels * static_cast<size_t>(channels);  // one scalar per channel

    if (n_elements > static_cast<size_t>(std::numeric_limits<cl_uint>::max())) {
        throw std::runtime_error("Image too large: element count exceeds cl_uint range");
    }
    const cl_uint n_elem_int = static_cast<cl_uint>(n_elements);

    // WHY float copy from uchar: the float kernel operates on float values.
    // We convert the host image to float so both variants receive equivalent data.
    std::vector<float> host_in_f32(n_elements);
    for (size_t i = 0; i < n_elements; ++i) {
        host_in_f32[i] = static_cast<float>(host_in_u8[i]);
    }

    // ── OpenCL setup ─────────────────────────────────────────────────────────
    OclContext ocl = create_context();
    cl::CommandQueue prof_queue(ocl.context, ocl.device, CL_QUEUE_PROFILING_ENABLE);

    // ── Device buffers ───────────────────────────────────────────────────────
    // uchar buffers: 1 byte per element
    const size_t bytes_u8 = n_elements * sizeof(uint8_t);
    cl::Buffer buf_in_u8(ocl.context,
                         CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                         bytes_u8,
                         host_in_u8.data());
    cl::Buffer buf_out_u8(ocl.context, CL_MEM_WRITE_ONLY, bytes_u8);

    // float buffers: 4 bytes per element
    const size_t bytes_f32 = n_elements * sizeof(float);
    cl::Buffer buf_in_f32(ocl.context,
                          CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                          bytes_f32,
                          host_in_f32.data());
    cl::Buffer buf_out_f32(ocl.context, CL_MEM_WRITE_ONLY, bytes_f32);

    // ── Load kernel source (shared) ───────────────────────────────────────────
    const std::string src = load_kernel_source("kernels/mad_kernel.cl");

    // ── Build uchar variant ───────────────────────────────────────────────────
    cl::Program prog_u8 = build_program(ocl.context, ocl.device, src, "-D TYPE=uchar");

    // ── Build float variant ───────────────────────────────────────────────────
    cl::Program prog_f32 = build_program(ocl.context, ocl.device, src, "-D TYPE=float");

    // ── Run both variants ─────────────────────────────────────────────────────
    std::cout << "\nRunning uchar variant...\n";
    const double ms_u8 = run_variant(prof_queue, prog_u8,
                                     buf_in_u8, buf_out_u8,
                                     contrast, brightness, n_elem_int);

    std::cout << "Running float variant...\n";
    const double ms_f32 = run_variant(prof_queue, prog_f32,
                                      buf_in_f32, buf_out_f32,
                                      contrast, brightness, n_elem_int);

    // ── Timing table ─────────────────────────────────────────────────────────
    std::cout << "\nType    | Kernel Time (ms)\n";
    std::cout << "--------|------------------\n";
    std::cout << std::fixed << std::setprecision(3);
    std::cout << "uchar   | " << std::setw(16) << ms_u8  << "\n";
    std::cout << "float   | " << std::setw(16) << ms_f32 << "\n";

    // ── Read back float result & save ─────────────────────────────────────────
    std::vector<float> host_out_f32(n_elements);
    CL_CHECK(prof_queue.enqueueReadBuffer(buf_out_f32, CL_TRUE, 0,
                                          bytes_f32, host_out_f32.data()));

    // Convert float result back to uint8 for BMP output.
    std::vector<uint8_t> host_out_u8(n_elements);
    for (size_t i = 0; i < n_elements; ++i) {
        const float clamped = std::max(0.0f, std::min(255.0f, host_out_f32[i]));
        host_out_u8[i] = static_cast<uint8_t>(clamped);
    }

    save_bmp("output.bmp", host_out_u8, width, height, channels);
    std::cout << "\nSaved: output.bmp (from float variant)\n";

    return 0;
}
