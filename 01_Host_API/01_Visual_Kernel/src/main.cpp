/*
 * Applied OpenCL Lab — Module 1, Phase 1: Visual Kernel (MAD filter)
 *
 * Flow:
 *   1. Parse CLI args (--contrast, --brightness, --input, --kernel)  [main.cpp ~30]
 *   2. Load image OR generate 256×256 RGB gradient                   [main.cpp ~47]
 *   3. Allocate src/dst cl::Buffer, build mad.cl, enqueue NDRange     [main.cpp ~65]
 *   4. Read back result → output.bmp                                  [main.cpp ~115]
 */

// stb — single-header image IO (implementations compiled here)
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image.h>
#include <stb_image_write.h>

#include "image_utils.hpp"    // load_rgb_image(), save_bmp(), make_gradient()
#include "ocl_wrapper.hpp"    // create_context(), OclContext
#include "opencl_utils.hpp"   // load_kernel_source(), build_program(), CL_CHECK

#include <CLI/CLI.hpp>

#include <climits>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

int main(int argc, char* argv[]) {
    try {
        // Step 1 — Parse CLI args [main.cpp ~30]
        float       contrast   = 1.0f;
        int         brightness = 0;
        std::string input_path;
        std::string kvariant   = "scalar";  // "scalar" | "vec3"

        CLI::App app{"MAD image filter (contrast/brightness)"};
        app.add_option("-c,--contrast",   contrast,    "Contrast multiplier (default: 1.0)");
        app.add_option("-b,--brightness", brightness,  "Brightness addend   (default: 0)");
        app.add_option("-i,--input",      input_path,  "Source image (BMP/PNG/JPG)");
        app.add_option("-k,--kernel",     kvariant,    "Kernel variant: scalar (default) or vec3");
        CLI11_PARSE(app, argc, argv);

        // Step 2 — Load or generate source image (always RGB, 3 channels — no alpha) [main.cpp ~47]
        int width = 0, height = 0, channels = 0;
        std::vector<uint8_t> src_data;

        if (input_path.empty()) {
            std::cout << "No --input provided. Generating 256x256 synthetic gradient.\n";
            src_data = make_gradient(width, height, channels);
            // Save the unmodified gradient so the user can diff before/after visually.
            save_bmp("gradient_input.bmp", src_data, width, height, channels);
            std::cout << "Written: gradient_input.bmp (pre-filter reference)\n";
        } else {
            src_data = load_rgb_image(input_path, width, height, channels);
            std::cout << "Loaded: " << input_path
                      << " (" << width << "x" << height << ")\n";
        }

        // WHY promote width first: width * height * channels as plain int
        // multiplication overflows before the cast on large images (e.g. 4K).
        // Promoting the first operand to size_t makes the entire expression size_t.
        const size_t total_bytes = static_cast<size_t>(width) * height * channels;

        // WHY INT_MAX guard: the kernel receives the count as cl_int (signed).
        // A silent truncation would dispatch the wrong NDRange on very large images.
        if (total_bytes > static_cast<size_t>(INT_MAX)) {
            throw std::runtime_error("Image too large: total_bytes exceeds INT_MAX");
        }

        std::cout << "Contrast=" << contrast
                  << "  Brightness=" << brightness
                  << "  Kernel=" << kvariant << "\n";

        // Step 3 — OpenCL context (GPU-first, CPU fallback) [main.cpp ~65]
        OclContext ocl = create_context();

        // Step 4 — Allocate buffers
        // WHY CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR together:
        //   CL_MEM_READ_ONLY     — kernel cannot accidentally write to this buffer;
        //                          the driver may place it in constant/texture memory.
        //   CL_MEM_COPY_HOST_PTR — driver copies src_data into device memory inside
        //                          the cl::Buffer() constructor; no separate
        //                          enqueueWriteBuffer command is needed.
        cl::Buffer buf_src(ocl.context,
                           CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                           total_bytes,
                           src_data.data());

        cl::Buffer buf_dst(ocl.context,
                           CL_MEM_WRITE_ONLY,
                           total_bytes);

        // Step 5 — Build program [main.cpp ~80]
        // WHY build_program(): centralises the try/catch + getBuildInfo log so every
        // module surfaces the same diagnostic on kernel compile errors.
        cl::Program program = build_program(ocl.context, ocl.device, "kernels/mad.cl");

        // Step 6 — Set kernel arguments and dispatch [main.cpp ~90]
        //    scalar: one work-item per byte  → NDRange = total_bytes
        //    vec3:   one work-item per pixel → NDRange = width * height
        const bool   use_vec3  = (kvariant == "vec3");
        const char*  kname     = use_vec3 ? "mad_vec_kernel" : "mad_kernel";
        const size_t work_size = use_vec3
            ? static_cast<size_t>(width) * height
            : total_bytes;

        cl::Kernel kernel(program, kname);
        CL_CHECK(kernel.setArg(0, buf_src));
        CL_CHECK(kernel.setArg(1, buf_dst));
        CL_CHECK(kernel.setArg(2, contrast));
        CL_CHECK(kernel.setArg(3, brightness));
        // WHY cast to cl_int: kernel parameter is declared as int; work_size holds
        // the exact element count for the chosen variant (bytes or pixels).
        CL_CHECK(kernel.setArg(4, static_cast<cl_int>(work_size)));

        CL_CHECK(ocl.queue.enqueueNDRangeKernel(kernel,
                                                 cl::NullRange,
                                                 cl::NDRange(work_size),
                                                 cl::NullRange));
        CL_CHECK(ocl.queue.finish());

        // Step 7 — Read back and save [main.cpp ~115]
        std::vector<uint8_t> dst_data(total_bytes);
        CL_CHECK(ocl.queue.enqueueReadBuffer(buf_dst, CL_TRUE, 0, total_bytes, dst_data.data()));

        save_bmp("output.bmp", dst_data, width, height, channels);
        std::cout << "Written: output.bmp (" << width << "x" << height << ")\n";

    } catch (const cl::Error& e) {
        std::cerr << "OpenCL error: " << e.what() << " (" << e.err() << ")\n";
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
