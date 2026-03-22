// 01_basic_mad — Baseline: hardcoded float MAD kernel.
//
// Sub-step 01 demonstrates the simplest possible use of a compile-time
// type-generic kernel: build mad_kernel.cl with no -D flags (defaults to
// uchar internally, but here we rely on the TYPE default and note the
// educational point is the workflow, not the type).
//
// Actually: to keep the educational diff clear versus 02_generic_mad, we
// explicitly pass -D TYPE=float here, showing that even the "basic" version
// needs to specify a type for production use.
//
// Output: output.bmp — contrast/brightness adjusted image (or gradient if
//         no --image is provided).
//
// WHY this matters:
//   Runtime kernel compilation lets us target a specific scalar type without
//   maintaining N copies of the kernel. This file is the starting point —
//   single type, no table — so the diff to 02_generic_mad is obvious.

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image.h>
#include <stb_image_write.h>

#include "opencl_utils.hpp"  // CL_CHECK, load_kernel_source, duration_ms; pulls in cl.hpp
#include "ocl_wrapper.hpp"   // create_context(), OclContext
#include "image_utils.hpp"   // load_rgb_image, save_bmp, make_gradient

#include <CLI/CLI.hpp>

#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

int main(int argc, char** argv)
{
    // ── CLI ──────────────────────────────────────────────────────────────────
    CLI::App app{"01_basic_mad — single float MAD kernel, baseline sub-step"};

    std::string image_path;
    int         width    = 1920;
    int         height   = 1080;
    float       contrast   = 1.2f;
    float       brightness = -10.0f;

    app.add_option("--image",      image_path, "Input BMP/PNG path (optional; gradient used if absent)");
    app.add_option("--width",      width,      "Synthetic gradient width  (default 1920, ignored if --image given)");
    app.add_option("--height",     height,     "Synthetic gradient height (default 1080, ignored if --image given)");
    app.add_option("--contrast",   contrast,   "Contrast multiplier (default 1.2)");
    app.add_option("--brightness", brightness, "Brightness addend   (default -10)");

    CLI11_PARSE(app, argc, argv);

    // ── Input image ──────────────────────────────────────────────────────────
    int channels = 3;
    std::vector<uint8_t> host_in;

    if (image_path.empty()) {
        std::cout << "No --image provided; using " << width << "x" << height
                  << " synthetic gradient.\n";
        host_in = make_gradient(width, height, channels);
    } else {
        host_in = load_rgb_image(image_path, width, height, channels);
        std::cout << "Loaded: " << image_path << " (" << width << "x" << height
                  << ", " << channels << "ch)\n";
    }

    // WHY promote to size_t first: width * height * channels as int overflows
    // for >2M pixel RGB images before the cast occurs.
    const size_t n_pixels  = static_cast<size_t>(width)    * static_cast<size_t>(height);
    const size_t n_bytes   = n_pixels * static_cast<size_t>(channels);

    if (n_pixels * static_cast<size_t>(channels) > static_cast<size_t>(std::numeric_limits<cl_uint>::max())) {
        throw std::runtime_error("Image too large: element count exceeds cl_uint range");
    }

    // ── OpenCL setup ─────────────────────────────────────────────────────────
    OclContext ocl = create_context();

    // WHY new queue with CL_QUEUE_PROFILING_ENABLE: create_context() returns a
    // plain queue; cl::Event timestamps are only accurate on a profiling queue.
    cl::CommandQueue prof_queue(ocl.context, ocl.device, CL_QUEUE_PROFILING_ENABLE);

    // ── Buffers ───────────────────────────────────────────────────────────────
    // CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR: upload host_in to device once.
    cl::Buffer buf_in(ocl.context,
                      CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                      n_bytes,
                      host_in.data());

    // Output buffer: device-only, same size.
    cl::Buffer buf_out(ocl.context, CL_MEM_WRITE_ONLY, n_bytes);

    // ── Kernel build — hardcoded float ────────────────────────────────────────
    // WHY -D TYPE=float: we hard-code the type here. Sub-step 02 will show
    // how to build the same source twice with different -D values.
    const std::string src = load_kernel_source("kernels/mad_kernel.cl");
    cl::Program program(ocl.context, cl::Program::Sources{src});

    try {
        program.build({ocl.device}, "-D TYPE=float");
    } catch (const cl::Error&) {
        std::cerr << "Build log:\n"
                  << program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(ocl.device) << "\n";
        throw;
    }

    cl::Kernel kernel(program, "mad_kernel");

    // ── Kernel arguments ─────────────────────────────────────────────────────
    CL_CHECK(kernel.setArg(0, buf_out));
    CL_CHECK(kernel.setArg(1, buf_in));
    CL_CHECK(kernel.setArg(2, contrast));
    CL_CHECK(kernel.setArg(3, brightness));
    CL_CHECK(kernel.setArg(4, static_cast<cl_uint>(n_pixels * static_cast<size_t>(channels))));

    // ── Dispatch ─────────────────────────────────────────────────────────────
    cl::Event evt;
    CL_CHECK(prof_queue.enqueueNDRangeKernel(
        kernel,
        cl::NullRange,
        cl::NDRange(n_pixels * channels),
        cl::NullRange,
        nullptr,
        &evt));

    CL_CHECK(prof_queue.finish());  // ensure event timestamps are committed

    const double kernel_ms = duration_ms(evt);
    std::cout << std::fixed << std::setprecision(3)
              << "Kernel time (float): " << kernel_ms << " ms\n";

    // ── Read back & save ──────────────────────────────────────────────────────
    std::vector<uint8_t> host_out(n_bytes);
    CL_CHECK(prof_queue.enqueueReadBuffer(buf_out, CL_TRUE, 0, n_bytes, host_out.data()));

    save_bmp("output.bmp", host_out, width, height, channels);
    std::cout << "Saved: output.bmp\n";

    return 0;
}
