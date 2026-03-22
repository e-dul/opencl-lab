/*
 * Applied OpenCL Lab — Module 1, Phase 1: Visual Kernel (MAD filter)
 *
 * Flow:
 *   1. Parse CLI args (--contrast, --brightness, --input, --kernel)
 *   2. Load image OR generate 256×256 RGB gradient
 *   3. Allocate src/dst cl::Buffer, build mad.cl, enqueue NDRange
 *   4. Read back result → output.bmp
 */

// stb — single-header image IO (implementations compiled here)
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image.h>
#include <stb_image_write.h>

#include "image_utils.hpp"    // load_rgb_image(), save_bmp(), make_gradient()
#include "ocl_wrapper.hpp"    // create_context(), OclContext
#include "opencl_utils.hpp"   // load_kernel_source(), CL_CHECK

#include <CLI/CLI.hpp>

#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

int main(int argc, char* argv[]) {
    try {
        // 1. Parse CLI args
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

        // 2. Load or generate source image (always RGB, 3 channels — no alpha)
        int width = 0, height = 0, channels = 0;
        std::vector<uint8_t> src_data;

        if (input_path.empty()) {
            std::cout << "No --input provided. Generating 256×256 synthetic gradient.\n";
            src_data = make_gradient(width, height, channels);
            // Save the unmodified gradient so the user can diff before/after visually.
            save_bmp("gradient_input.bmp", src_data, width, height, channels);
            std::cout << "Written: gradient_input.bmp (pre-filter reference)\n";
        } else {
            src_data = load_rgb_image(input_path, width, height, channels);
            std::cout << "Loaded: " << input_path
                      << " (" << width << "×" << height << ")\n";
        }

        const size_t total_bytes = static_cast<size_t>(width * height * channels);

        std::cout << "Contrast=" << contrast
                  << "  Brightness=" << brightness
                  << "  Kernel=" << kvariant << "\n";

        // 3. OpenCL context (GPU-first, CPU fallback)
        OclContext ocl = create_context();

        // 4. Allocate buffers
        cl::Buffer buf_src(ocl.context,
                           CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                           total_bytes,
                           src_data.data());

        cl::Buffer buf_dst(ocl.context,
                           CL_MEM_WRITE_ONLY,
                           total_bytes);

        // 5. Build program
        const std::string source = load_kernel_source("kernels/mad.cl");
        cl::Program::Sources sources;
        sources.push_back({source.c_str(), source.size()});

        cl::Program program(ocl.context, sources);
        try {
            program.build({ocl.device});
        } catch (const cl::Error&) {
            std::cerr << "Build log:\n"
                      << program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(ocl.device)
                      << "\n";
            throw;
        }

        // 6. Set kernel arguments and dispatch
        //    scalar: one work-item per byte  → NDRange = total_bytes
        //    vec3:   one work-item per pixel → NDRange = width * height
        const bool   use_vec3  = (kvariant == "vec3");
        const char*  kname     = use_vec3 ? "mad_vec_kernel" : "mad_kernel";
        const size_t work_size = use_vec3
            ? static_cast<size_t>(width * height)
            : total_bytes;

        cl::Kernel kernel(program, kname);
        kernel.setArg(0, buf_src);
        kernel.setArg(1, buf_dst);
        kernel.setArg(2, contrast);
        kernel.setArg(3, brightness);

        ocl.queue.enqueueNDRangeKernel(kernel,
                                        cl::NullRange,
                                        cl::NDRange(work_size),
                                        cl::NullRange);
        ocl.queue.finish();

        // 7. Read back and save
        std::vector<uint8_t> dst_data(total_bytes);
        ocl.queue.enqueueReadBuffer(buf_dst, CL_TRUE, 0, total_bytes, dst_data.data());

        save_bmp("output.bmp", dst_data, width, height, channels);
        std::cout << "Written: output.bmp (" << width << "×" << height << ")\n";

    } catch (const cl::Error& e) {
        std::cerr << "OpenCL error: " << e.what() << " (" << e.err() << ")\n";
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
