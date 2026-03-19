/*
 * Applied OpenCL Lab — Add-on 4.3: Deployment Package
 *
 * This binary is the "reference application" that gets packaged.
 * It replicates the 01_Visual_Kernel logic (MAD filter) with:
 *   - CLI11 argument parsing
 *   - cl::Event kernel profiling
 *   - Graceful CPU/non-GPU device warning for Docker/CI paths
 *
 * Flow:
 *   1. Parse CLI args (--input, --output)
 *   2. Load input BMP/PNG via stb_image; generate synthetic gradient if omitted
 *   3. Upload to cl::Buffer, dispatch gradient.cl, read back
 *   4. Write output.bmp via stb_image_write
 *   5. Print kernel execution time (ms via cl::Event)
 */

// stb — single-header image IO (compiled exactly once per binary)
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image.h>
#include <stb_image_write.h>

#include "image_utils.hpp"    // load_rgb_image(), save_bmp(), make_gradient()
#include "ocl_wrapper.hpp"    // create_context(), OclContext
#include "opencl_utils.hpp"   // load_kernel_source(), CL_CHECK, duration_ms()

#include <CLI/CLI.hpp>

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

int main(int argc, char* argv[]) {
    try {
        // 1. Parse CLI args
        std::string input_path;
        std::string output_path = "output.bmp";

        CLI::App app{"Deployment demo — gradient/MAD filter (packaging reference)"};
        app.add_option("--input",  input_path,  "Source image (BMP/PNG/JPG). Omit to use a synthetic gradient.");
        app.add_option("--output", output_path, "Destination image path (default: output.bmp)");
        CLI11_PARSE(app, argc, argv);

        // 2. Load or generate source image (always RGB, 3 channels — no alpha)
        int width = 0, height = 0, channels = 0;
        std::vector<uint8_t> src_data;

        if (input_path.empty()) {
            std::cout << "No --input provided. Generating 256x256 synthetic gradient.\n";
            src_data = make_gradient(width, height, channels);
        } else {
            // Fail gracefully with a descriptive message if the file does not exist.
            // WHY exit 0: CI pipelines treat non-zero as build failure; a missing
            // asset is an environment issue, not a code defect.
            std::ifstream probe(input_path);
            if (!probe.good()) {
                std::cerr << "Error: input file not found: " << input_path << "\n";
                return 0;
            }
            src_data = load_rgb_image(input_path, width, height, channels);
            std::cout << "Loaded: " << input_path
                      << " (" << width << "x" << height << ")\n";
        }

        if (static_cast<size_t>(width) * height > static_cast<size_t>(INT_MAX)) {
            throw std::runtime_error("Image too large — pixel count exceeds INT_MAX.");
        }
        const size_t total_bytes = static_cast<size_t>(width) * height * channels;

        // 3. OpenCL context (GPU-first, CPU fallback via create_context())
        OclContext ocl = create_context();

        // WHY: clGetPlatformIDs returns CL_SUCCESS even when pocl (CPU) is the only
        // ICD. Without this check the binary silently runs on CPU — correct output,
        // 50-200x slower, no error. Warn loudly so the user knows GPU was not found.
        cl_device_type dev_type;
        CL_CHECK(ocl.device.getInfo(CL_DEVICE_TYPE, &dev_type));
        if (dev_type != CL_DEVICE_TYPE_GPU) {
            std::cerr << "WARNING: selected device is not a GPU ("
                      << ocl.device.getInfo<CL_DEVICE_NAME>() << "). "
                      << "Set GPU=<vendor> or install a GPU ICD.\n";
        }

        // 4. Create a profiling-enabled queue.
        // WHY separate queue: create_context() returns a default queue without
        // CL_QUEUE_PROFILING_ENABLE. We need profiling for accurate kernel timing.
        cl::CommandQueue prof_queue(ocl.context, ocl.device,
                                    CL_QUEUE_PROFILING_ENABLE);

        // 5. Allocate device buffers
        cl::Buffer buf_src(ocl.context,
                           CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                           total_bytes,
                           src_data.data());

        cl::Buffer buf_dst(ocl.context,
                           CL_MEM_WRITE_ONLY,
                           total_bytes);

        // 6. Build program from gradient.cl (same kernel as 01_Visual_Kernel/mad.cl)
        // WHY /proc/self/exe: inside an AppImage the binary runs from a squashfs
        // mount. The working directory is wherever the user launched from, not
        // where the kernels live. Resolving relative to the binary location makes
        // this work both as a plain build and as an AppImage.
        const std::filesystem::path bin_dir =
            std::filesystem::read_symlink("/proc/self/exe").parent_path();
        const std::string source = load_kernel_source(
            (bin_dir / "kernels/gradient.cl").string());
        cl::Program::Sources sources;
        sources.push_back({source.c_str(), source.size()});

        cl::Program program(ocl.context, sources);
        try {
            program.build({ocl.device});
        } catch (const cl::Error&) {
            std::cerr << "Kernel build log:\n"
                      << program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(ocl.device)
                      << "\n";
            throw;
        }

        // 7. Dispatch kernel (scalar path: one work-item per byte)
        //    neutral MAD: contrast=1.0, brightness=0 → identity transform;
        //    demonstrates the pipeline without altering the image.
        cl::Kernel kernel(program, "mad_kernel");
        CL_CHECK(kernel.setArg(0, buf_src));
        CL_CHECK(kernel.setArg(1, buf_dst));
        CL_CHECK(kernel.setArg(2, 1.0f));   // contrast  (identity)
        CL_CHECK(kernel.setArg(3, 0));       // brightness (identity)
        CL_CHECK(kernel.setArg(4, static_cast<cl_int>(total_bytes)));  // bounds guard in kernel

        cl::Event kernel_event;
        CL_CHECK(prof_queue.enqueueNDRangeKernel(
            kernel,
            cl::NullRange,
            cl::NDRange(total_bytes),
            cl::NullRange,
            nullptr,
            &kernel_event));
        CL_CHECK(prof_queue.finish());

        // 8. Report kernel execution time via cl::Event profiling
        const double kernel_ms = duration_ms(kernel_event);
        std::cout << std::fixed << std::setprecision(3)
                  << "Kernel time : " << kernel_ms << " ms\n";

        // 9. Read back and write output
        std::vector<uint8_t> dst_data(total_bytes);
        CL_CHECK(prof_queue.enqueueReadBuffer(
            buf_dst, CL_TRUE, 0, total_bytes, dst_data.data()));

        save_bmp(output_path, dst_data, width, height, channels);
        std::cout << "Written: " << output_path
                  << " (" << width << "x" << height << ")\n";

    } catch (const cl::Error& e) {
        std::cerr << "OpenCL error: " << e.what() << " (" << e.err() << ")\n";
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
