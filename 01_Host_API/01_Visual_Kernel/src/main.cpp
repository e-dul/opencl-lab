/*
 * Applied OpenCL Lab — Module 1, Phase 1: Visual Kernel (MAD filter)
 *
 * Flow:
 *   1. Parse CLI args (--contrast, --brightness, --input)
 *   2. Load image OR generate 256×256 RGB gradient
 *   3. Allocate src/dst cl::Buffer, build mad.cl, enqueue NDRange
 *   4. Read back result → output.bmp
 */

// stb — single-header image IO (implementations compiled here)
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image.h>
#include <stb_image_write.h>

#include "ocl_wrapper.hpp"    // create_context(), OclContext
#include "opencl_utils.hpp"   // load_kernel_source(), CL_CHECK

#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

// ── CLI parsing ───────────────────────────────────────────────────────────────

struct Args {
    float       contrast   = 1.0f;
    int         brightness = 0;
    std::string input_path;            // empty → generate synthetic image
    std::string kernel     = "scalar"; // "scalar" | "vec3"
};

static void print_help(const char* prog) {
    std::cout <<
        "Usage: " << prog << " [OPTIONS]\n"
        "\n"
        "Options:\n"
        "  -c, --contrast   <float>  Contrast multiplier (default: 1.0)\n"
        "  -b, --brightness <int>    Brightness addend  (default: 0)\n"
        "  -i, --input      <path>   Source image (BMP/PNG/JPG).\n"
        "                            Omit to use a synthetic 256x256 gradient.\n"
        "  -k, --kernel     <name>   Kernel: scalar (default) or vec3.\n"
        "  -h, --help                Show this help and exit.\n"
        "\n"
        "Output:\n"
        "  output.bmp            — filtered result\n"
        "  gradient_input.bmp    — original gradient (synthetic mode only)\n";
}

Args parse_args(int argc, char* argv[]) {
    Args a;
    for (int i = 1; i < argc; ++i) {
        std::string key(argv[i]);
        if (key == "--help" || key == "-h") {
            print_help(argv[0]);
            std::exit(0);
        } else if ((key == "--contrast" || key == "-c") && i + 1 < argc) {
            a.contrast = std::stof(argv[++i]);
        } else if ((key == "--brightness" || key == "-b") && i + 1 < argc) {
            a.brightness = std::stoi(argv[++i]);
        } else if ((key == "--input" || key == "-i") && i + 1 < argc) {
            a.input_path = argv[++i];
        } else if ((key == "--kernel" || key == "-k") && i + 1 < argc) {
            a.kernel = argv[++i];
        }
    }
    return a;
}

// ── Synthetic image (256×256 RGB horizontal gradient) ─────────────────────────

std::vector<uint8_t> make_gradient(int& width, int& height, int& channels) {
    width    = 256;
    height   = 256;
    channels = 3;
    std::vector<uint8_t> img(static_cast<size_t>(width * height * channels));
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            size_t idx      = static_cast<size_t>((y * width + x) * channels);
            img[idx + 0]    = static_cast<uint8_t>(x);          // R increases left→right
            img[idx + 1]    = static_cast<uint8_t>(y);          // G increases top→bottom
            img[idx + 2]    = 128;                              // B constant mid-grey
        }
    }
    return img;
}

// ── Main ──────────────────────────────────────────────────────────────────────

int main(int argc, char* argv[]) {
    try {
        const Args args = parse_args(argc, argv);

        // 1. Load or generate source image (always RGB, 3 channels — no alpha)
        int width = 0, height = 0, channels = 0;
        std::vector<uint8_t> src_data;

        if (args.input_path.empty()) {
            std::cout << "No --input provided. Generating 256×256 synthetic gradient.\n";
            src_data = make_gradient(width, height, channels);
            // Save the unmodified gradient so the user can diff before/after visually.
            if (!stbi_write_bmp("gradient_input.bmp", width, height, channels, src_data.data())) {
                throw std::runtime_error("stbi_write_bmp failed for gradient_input.bmp");
            }
            std::cout << "Written: gradient_input.bmp (pre-filter reference)\n";
        } else {
            int loaded_channels = 0;
            uint8_t* raw = stbi_load(args.input_path.c_str(),
                                     &width, &height, &loaded_channels,
                                     3 /*force RGB — strips alpha*/);
            if (!raw) {
                throw std::runtime_error("stbi_load failed: " +
                                         std::string(stbi_failure_reason()));
            }
            const size_t n = static_cast<size_t>(width * height * 3);
            src_data.assign(raw, raw + n);
            stbi_image_free(raw);
            channels = 3;
            std::cout << "Loaded: " << args.input_path
                      << " (" << width << "×" << height << ")\n";
        }

        const size_t total_bytes = static_cast<size_t>(width * height * channels);

        std::cout << "Contrast=" << args.contrast
                  << "  Brightness=" << args.brightness
                  << "  Kernel=" << args.kernel << "\n";

        // 2. OpenCL context (GPU-first, CPU fallback)
        OclContext ocl = create_context();

        // 3. Allocate buffers
        cl::Buffer buf_src(ocl.context,
                           CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                           total_bytes,
                           src_data.data());

        cl::Buffer buf_dst(ocl.context,
                           CL_MEM_WRITE_ONLY,
                           total_bytes);

        // 4. Build program
        const std::string source = load_kernel_source("kernels/mad.cl");
        cl::Program::Sources sources;
        sources.push_back({source.c_str(), source.size()});

        cl::Program program(ocl.context, sources);
        try {
            program.build({ocl.device});
        } catch (const cl::Error&) {
            // Surface the build log before re-throwing
            std::cerr << "Build log:\n"
                      << program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(ocl.device)
                      << "\n";
            throw;
        }

        // 5. Set kernel arguments and dispatch
        //    scalar: one work-item per byte  → NDRange = total_bytes
        //    vec3:   one work-item per pixel → NDRange = width * height
        const bool        use_vec3     = (args.kernel == "vec3");
        const std::string kernel_name  = use_vec3 ? "mad_vec_kernel" : "mad_kernel";
        const size_t      work_size    = use_vec3
            ? static_cast<size_t>(width * height)
            : total_bytes;

        cl::Kernel kernel(program, kernel_name.c_str());
        kernel.setArg(0, buf_src);
        kernel.setArg(1, buf_dst);
        kernel.setArg(2, args.contrast);
        kernel.setArg(3, args.brightness);

        ocl.queue.enqueueNDRangeKernel(kernel,
                                        cl::NullRange,
                                        cl::NDRange(work_size),
                                        cl::NullRange);
        ocl.queue.finish();

        // 6. Read back
        std::vector<uint8_t> dst_data(total_bytes);
        ocl.queue.enqueueReadBuffer(buf_dst, CL_TRUE, 0, total_bytes, dst_data.data());

        // 7. Write output
        const char* out_path = "output.bmp";
        if (!stbi_write_bmp(out_path, width, height, channels, dst_data.data())) {
            throw std::runtime_error("stbi_write_bmp failed");
        }

        std::cout << "Written: " << out_path
                  << " (" << width << "×" << height << ")\n";

    } catch (const cl::Error& e) {
        std::cerr << "OpenCL error: " << e.what() << " (" << e.err() << ")\n";
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
