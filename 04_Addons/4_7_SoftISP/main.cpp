// softisp — SoftISP debayer benchmark
//
// Reads a raw RGGB Bayer file, runs two debayer kernels:
//   V1: naive bilinear (debayer_naive.cl)
//   V2: LDS-tiled bilinear (debayer_lds.cl)
// Asserts byte-exact identity of both outputs, writes BMP files, reports timing.

// STB must be implemented exactly once per binary, before image_utils.hpp.
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image.h>
#include <stb_image_write.h>

#include "opencl_utils.hpp"
#include "ocl_wrapper.hpp"
#include "image_utils.hpp"
#include <CLI/CLI.hpp>

#include <cstdint>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

int run(int argc, char* argv[]);

int main(int argc, char* argv[]) {
    try {
        return run(argc, argv);
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}

int run(int argc, char* argv[]) {
    // ── CLI ──────────────────────────────────────────────────────────────────
    CLI::App app{"SoftISP: bilinear vs LDS-tiled debayer benchmark"};
    std::string input_path = "assets/raw_bayer_4k.raw";
    int width  = 3840;
    int height = 2160;
    app.add_option("--input",  input_path, "Raw RGGB Bayer input file")->capture_default_str();
    app.add_option("--width",  width,      "Image width in pixels")->capture_default_str();
    app.add_option("--height", height,     "Image height in pixels")->capture_default_str();
    CLI11_PARSE(app, argc, argv);

    // ── Load raw Bayer data ──────────────────────────────────────────────────
    // load_raw_binary validates file size == expected_bytes; throws on mismatch.
    const size_t bayer_bytes = static_cast<size_t>(width) * height;
    std::vector<uint8_t> bayer_host = load_raw_binary(input_path, bayer_bytes);

    // ── OpenCL setup ─────────────────────────────────────────────────────────
    OclContext ocl = create_context();

    // WHY CL_QUEUE_PROFILING_ENABLE: cl::Event timestamps require profiling to
    // be enabled at queue-creation time; create_context() returns a plain queue.
    cl::CommandQueue queue(ocl.context, ocl.device, CL_QUEUE_PROFILING_ENABLE);

    // ── Build kernels ────────────────────────────────────────────────────────
    auto bin_dir = get_binary_dir();
    cl::Program prog_v1 = build_program(
        ocl.context, ocl.device,
        (bin_dir / "kernels" / "debayer_naive.cl").string(),
        "-cl-std=CL1.2");

    // WHY -DTILE_W -DTILE_H: LDS tile dimensions are compile-time constants so
    // the compiler can size __local arrays and optimize register allocation.
    cl::Program prog_v2 = build_program(
        ocl.context, ocl.device,
        (bin_dir / "kernels" / "debayer_lds.cl").string(),
        "-cl-std=CL1.2 -DTILE_W=16 -DTILE_H=16");

    cl::Kernel k_v1(prog_v1, "debayer_naive");
    cl::Kernel k_v2(prog_v2, "debayer_lds");

    // ── Buffers ───────────────────────────────────────────────────────────────
    const size_t rgba_bytes = static_cast<size_t>(width) * height * 4;  // §7.1

    cl::Buffer bayer_buf(ocl.context,
        CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
        bayer_bytes, bayer_host.data());

    cl::Buffer rgba_v1_buf(ocl.context, CL_MEM_WRITE_ONLY, rgba_bytes);
    cl::Buffer rgba_v2_buf(ocl.context, CL_MEM_WRITE_ONLY, rgba_bytes);

    // ── Kernel args ───────────────────────────────────────────────────────────
    CL_CHECK(k_v1.setArg(0, bayer_buf));
    CL_CHECK(k_v1.setArg(1, rgba_v1_buf));
    CL_CHECK(k_v1.setArg(2, width));
    CL_CHECK(k_v1.setArg(3, height));

    CL_CHECK(k_v2.setArg(0, bayer_buf));
    CL_CHECK(k_v2.setArg(1, rgba_v2_buf));
    CL_CHECK(k_v2.setArg(2, width));
    CL_CHECK(k_v2.setArg(3, height));

    // ── V1 dispatch ───────────────────────────────────────────────────────────
    // Round up global size to multiples of local size so NDRange is valid for
    // arbitrary image dimensions (not just multiples of 16).
    constexpr size_t LOCAL = 16;
    cl::NDRange local_range(LOCAL, LOCAL);

    size_t gw_v1 = round_up(static_cast<size_t>(width),  LOCAL);
    size_t gh_v1 = round_up(static_cast<size_t>(height), LOCAL);

    cl::Event ev1;
    CL_CHECK(queue.enqueueNDRangeKernel(k_v1,
        cl::NullRange, cl::NDRange(gw_v1, gh_v1), local_range,
        nullptr, &ev1));
    CL_CHECK(queue.finish());

    // ── V2 dispatch ───────────────────────────────────────────────────────────
    size_t gw_v2 = round_up(static_cast<size_t>(width),  LOCAL);
    size_t gh_v2 = round_up(static_cast<size_t>(height), LOCAL);

    cl::Event ev2;
    CL_CHECK(queue.enqueueNDRangeKernel(k_v2,
        cl::NullRange, cl::NDRange(gw_v2, gh_v2), local_range,
        nullptr, &ev2));
    CL_CHECK(queue.finish());

    // ── Read back results ─────────────────────────────────────────────────────
    std::vector<uint8_t> rgba_v1(rgba_bytes);
    std::vector<uint8_t> rgba_v2(rgba_bytes);

    CL_CHECK(queue.enqueueReadBuffer(rgba_v1_buf, CL_TRUE, 0,
        rgba_bytes, rgba_v1.data()));
    CL_CHECK(queue.enqueueReadBuffer(rgba_v2_buf, CL_TRUE, 0,
        rgba_bytes, rgba_v2.data()));
    CL_CHECK(queue.finish());

    // ── Byte-exact assertion ──────────────────────────────────────────────────
    // WHY in-binary comparison (not shell diff): guarantees the test runs
    // atomically and is portable across CI environments.
    for (size_t i = 0; i < rgba_bytes; ++i) {
        if (rgba_v1[i] != rgba_v2[i]) {
            std::ostringstream msg;
            msg << "V1/V2 pixel mismatch at index " << i
                << ": v1=" << static_cast<int>(rgba_v1[i])
                << " v2=" << static_cast<int>(rgba_v2[i]);
            throw std::runtime_error(msg.str());
        }
    }

    // ── Write BMP outputs ─────────────────────────────────────────────────────
    save_bmp("output_rgb_v1.bmp", rgba_v1, width, height, 4);
    save_bmp("output_rgb_v2.bmp", rgba_v2, width, height, 4);

    // ── Timing report ─────────────────────────────────────────────────────────
    double v1_ms  = duration_ms(ev1);
    double v2_ms  = duration_ms(ev2);
    double speedup = v1_ms / v2_ms;
    double v1_fps  = 1000.0 / v1_ms;
    double v2_fps  = 1000.0 / v2_ms;

    std::cout << std::fixed << std::setprecision(3);
    std::cout << "\nV1 (naive bilinear):  " << std::setw(8) << v1_ms << " ms"
              << "  (" << std::setprecision(1) << v1_fps << " FPS)\n";
    std::cout << std::setprecision(3);
    std::cout << "V2 (LDS tiled):       " << std::setw(8) << v2_ms << " ms"
              << "  (" << std::setprecision(1) << v2_fps << " FPS)\n";
    std::cout << std::setprecision(2);
    std::cout << "Speedup:              " << std::setw(8) << speedup << "x\n";

    std::cout << "\nOutputs written: output_rgb_v1.bmp, output_rgb_v2.bmp\n";
    return 0;
}
