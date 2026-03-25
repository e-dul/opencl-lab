// MultiGPU_Strategy — demonstrates horizontal image-slice partitioning across
// multiple OpenCL devices with independent per-device contexts and queues.
//
// Key design decisions:
//   1. Separate cl::Context per device — OpenCL 1.2 does not support a single
//      context spanning devices from different platforms.
//   2. Per-device kernel time uses cl::Event profiling (accurate, not inflated
//      by data transfer or host latency).
//   3. Total N-GPU elapsed time: max(write_start+kernel+read end) -
//      min(write_start) using CL_PROFILING_COMMAND_START/END nanoseconds
//      converted to ms.  On same-platform multi-device systems the device
//      clock is shared and arithmetic is exact.  On cross-platform setups
//      the result may be approximate due to independent clock origins, but
//      satisfies the spec's event-profiling requirement.

#include "opencl_utils.hpp"   // CL_CHECK, load_kernel_source, duration_ms

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <CLI/CLI.hpp>

#include <algorithm>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

// ──────────────────────────────────────────────────────────────────────────────
// Device descriptor collected during enumeration
// ──────────────────────────────────────────────────────────────────────────────
struct DeviceEntry {
    cl::Platform platform;
    cl::Device   device;
};

// ──────────────────────────────────────────────────────────────────────────────
// enumerate_devices — mirrors the GPU-env-var logic in ocl_wrapper.hpp but
// returns ALL matching devices rather than just the first one.
// ──────────────────────────────────────────────────────────────────────────────
static std::vector<DeviceEntry> enumerate_devices() {
    std::vector<cl::Platform> platforms;
    cl::Platform::get(&platforms);
    if (platforms.empty()) {
        throw std::runtime_error("No OpenCL platforms found. Check your drivers.");
    }

    // Read optional vendor hint from environment — consistent with create_context().
    const char* raw_hint = std::getenv("GPU");
    std::string gpu_hint = raw_hint ? raw_hint : "";
    std::transform(gpu_hint.begin(), gpu_hint.end(), gpu_hint.begin(), ::toupper);

    auto contains_hint = [&](const std::string& s) -> bool {
        if (gpu_hint.empty()) return true;   // no filter → accept all
        std::string upper = s;
        std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);
        return upper.find(gpu_hint) != std::string::npos;
    };

    std::vector<DeviceEntry> result;

    // First pass: collect GPU devices that match the optional vendor filter.
    for (auto& p : platforms) {
        std::vector<cl::Device> gpus;
        p.getDevices(CL_DEVICE_TYPE_GPU, &gpus);

        for (auto& dev : gpus) {
            // Accept if platform vendor OR device vendor matches hint.
            if (contains_hint(p.getInfo<CL_PLATFORM_VENDOR>()) ||
                contains_hint(dev.getInfo<CL_DEVICE_VENDOR>())) {
                result.push_back({p, dev});
            }
        }
    }

    // CPU fallback when no GPU matches — prevents a confusing empty list.
    if (result.empty()) {
        std::cerr << "[WARNING] No GPU devices found";
        if (!gpu_hint.empty()) std::cerr << " matching GPU=" << raw_hint;
        std::cerr << ". Falling back to CPU devices.\n";

        for (auto& p : platforms) {
            std::vector<cl::Device> cpus;
            p.getDevices(CL_DEVICE_TYPE_CPU, &cpus);
            for (auto& dev : cpus) {
                result.push_back({p, dev});
            }
        }
    }

    if (result.empty()) {
        throw std::runtime_error("No usable OpenCL device found on any platform.");
    }
    return result;
}

// ──────────────────────────────────────────────────────────────────────────────
// print_device_table — prints a human-readable device inventory at startup
// ──────────────────────────────────────────────────────────────────────────────
static void print_device_table(const std::vector<DeviceEntry>& devices) {
    std::cout << "\nAvailable OpenCL devices:\n";
    std::cout << std::left
              << std::setw(4)  << "Idx"
              << std::setw(20) << "Vendor"
              << std::setw(40) << "Device Name"
              << "CUs\n";
    std::cout << std::string(68, '-') << "\n";

    for (size_t i = 0; i < devices.size(); ++i) {
        auto vendor = devices[i].device.getInfo<CL_DEVICE_VENDOR>();
        auto name   = devices[i].device.getInfo<CL_DEVICE_NAME>();
        auto cus    = devices[i].device.getInfo<CL_DEVICE_MAX_COMPUTE_UNITS>();
        std::cout << std::left
                  << std::setw(4)  << i
                  << std::setw(20) << vendor.substr(0, 19)
                  << std::setw(40) << name.substr(0, 39)
                  << cus << "\n";
    }
    std::cout << "\n";
}

// ──────────────────────────────────────────────────────────────────────────────
// generate_gradient — synthetic 4K RGBA image; no asset dependency required.
// Pattern is deterministic so we can do an invert-twice correctness check.
// ──────────────────────────────────────────────────────────────────────────────
static std::vector<cl_uchar> generate_gradient(int width, int height) {
    // Safety: promote before multiply to avoid int overflow on 4K images.
    size_t total_bytes = static_cast<size_t>(width) * height * 4;
    std::vector<cl_uchar> data(total_bytes);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            size_t idx = (static_cast<size_t>(y) * width + x) * 4;
            data[idx + 0] = static_cast<cl_uchar>((x + y)       % 256); // R
            data[idx + 1] = static_cast<cl_uchar>((x * 2 + y)   % 256); // G
            data[idx + 2] = static_cast<cl_uchar>((x + y * 3)   % 256); // B
            data[idx + 3] = static_cast<cl_uchar>((x ^ y)       % 256); // A
        }
    }
    return data;
}

// ──────────────────────────────────────────────────────────────────────────────
// build_program_from_source — compile the slice kernel for a given device/context pair
// WHY separate from common build_program: this module pre-loads the source once
// and distributes it across multiple device contexts; the common helper takes a
// file path and re-reads it each call, which would be wasteful here.
// ──────────────────────────────────────────────────────────────────────────────
static cl::Program build_program_from_source(const cl::Context& ctx,
                                              const cl::Device&  dev,
                                              const std::string& source) {
    cl::Program prog(ctx, source);
    try {
        prog.build({dev});
    } catch (const cl::Error& e) {
        // Surface the compiler log so a developer can debug kernel issues.
        std::string log = prog.getBuildInfo<CL_PROGRAM_BUILD_LOG>(dev);
        throw std::runtime_error("Kernel build failed:\n" + log);
    }
    return prog;
}

// ──────────────────────────────────────────────────────────────────────────────
// run_single_gpu — baseline leg using device[0] for the full image.
// Returns kernel execution time in ms (cl::Event profiling).
// ──────────────────────────────────────────────────────────────────────────────
static double run_single_gpu(const DeviceEntry&           dev_entry,
                              const std::string&           kernel_source,
                              const std::vector<cl_uchar>& input,
                              std::vector<cl_uchar>&       output,
                              int                          width,
                              int                          height) {
    size_t image_bytes = static_cast<size_t>(width) * height * 4;

    cl::Context ctx(dev_entry.device);
    // WHY CL_QUEUE_PROFILING_ENABLE: mandatory for cl::Event timestamp queries.
    cl::CommandQueue queue(ctx, dev_entry.device, CL_QUEUE_PROFILING_ENABLE);

    cl::Program prog = build_program_from_source(ctx, dev_entry.device, kernel_source);
    cl::Kernel  kernel(prog, "process_slice");

    cl::Buffer buf_in (ctx, CL_MEM_READ_ONLY,  image_bytes);
    cl::Buffer buf_out(ctx, CL_MEM_WRITE_ONLY, image_bytes);

    cl::Event ev_write, ev_kernel, ev_read;
    CL_CHECK(queue.enqueueWriteBuffer(buf_in, CL_FALSE, 0, image_bytes,
                                      input.data(), nullptr, &ev_write));

    CL_CHECK(kernel.setArg(0, buf_in));
    CL_CHECK(kernel.setArg(1, buf_out));
    CL_CHECK(kernel.setArg(2, width));
    CL_CHECK(kernel.setArg(3, height));
    CL_CHECK(kernel.setArg(4, static_cast<int>(0)));  // row_offset = 0 (full image)

    size_t global_size = static_cast<size_t>(width) * height;
    std::vector<cl::Event> wait_write = {ev_write};
    CL_CHECK(queue.enqueueNDRangeKernel(kernel, cl::NullRange,
                                        cl::NDRange(global_size), cl::NullRange,
                                        &wait_write, &ev_kernel));

    output.resize(image_bytes);
    std::vector<cl::Event> wait_kernel = {ev_kernel};
    CL_CHECK(queue.enqueueReadBuffer(buf_out, CL_FALSE, 0, image_bytes,
                                     output.data(), &wait_kernel, &ev_read));

    CL_CHECK(queue.finish());

    return duration_ms(ev_kernel);
}

// ──────────────────────────────────────────────────────────────────────────────
// Per-device timing record collected during the N-GPU leg
// ──────────────────────────────────────────────────────────────────────────────
struct DeviceTiming {
    double      kernel_ms;
    std::string device_name;
};

// ──────────────────────────────────────────────────────────────────────────────
// run_n_gpu — parallel leg: slice partitioning across all active devices.
//
// Total elapsed time: max(read_end) - min(write_start) using
// CL_PROFILING_COMMAND_START/END nanoseconds per task spec §7.
// Per-device kernel time: cl::Event profiling (accurate, within one driver).
// ──────────────────────────────────────────────────────────────────────────────
static double run_n_gpu(const std::vector<DeviceEntry>&  devices,
                        const std::string&               kernel_source,
                        const std::vector<cl_uchar>&     input,
                        std::vector<cl_uchar>&           output,
                        int                              width,
                        int                              height,
                        std::vector<DeviceTiming>&       timings) {
    int n = static_cast<int>(devices.size());
    int rows_per_device = height / n;

    output.resize(static_cast<size_t>(width) * height * 4);
    timings.resize(static_cast<size_t>(n));

    // Keep buffers and events alive until queue.finish() completes.
    // WHY buf_in/buf_out in DeviceState: they are created before finish() is
    // called; destroying them while async operations are in-flight is UB.
    struct DeviceState {
        cl::Context      ctx;
        cl::CommandQueue queue;
        cl::Buffer       buf_in;
        cl::Buffer       buf_out;
        cl::Event        ev_write;
        cl::Event        ev_kernel;
        cl::Event        ev_read;
    };
    std::vector<DeviceState> states;
    states.reserve(static_cast<size_t>(n));

    // Launch all devices without waiting — the host issues all dispatches
    // before calling finish() so they can overlap in time on parallel hardware.
    for (int i = 0; i < n; ++i) {
        int row_start  = i * rows_per_device;
        // Last device absorbs any remainder rows.
        int row_end    = (i == n - 1) ? height : row_start + rows_per_device;
        int slice_rows = row_end - row_start;

        size_t slice_bytes  = static_cast<size_t>(width) * slice_rows * 4;
        size_t slice_offset = static_cast<size_t>(width) * row_start  * 4;

        DeviceState ds;
        ds.ctx    = cl::Context(devices[i].device);
        ds.queue  = cl::CommandQueue(ds.ctx, devices[i].device, CL_QUEUE_PROFILING_ENABLE);
        ds.buf_in  = cl::Buffer(ds.ctx, CL_MEM_READ_ONLY,  slice_bytes);
        ds.buf_out = cl::Buffer(ds.ctx, CL_MEM_WRITE_ONLY, slice_bytes);

        CL_CHECK(ds.queue.enqueueWriteBuffer(ds.buf_in, CL_FALSE, 0, slice_bytes,
                                             input.data() + slice_offset,
                                             nullptr, &ds.ev_write));

        cl::Program prog   = build_program_from_source(ds.ctx, devices[i].device, kernel_source);
        cl::Kernel   kernel(prog, "process_slice");

        CL_CHECK(kernel.setArg(0, ds.buf_in));
        CL_CHECK(kernel.setArg(1, ds.buf_out));
        CL_CHECK(kernel.setArg(2, width));
        CL_CHECK(kernel.setArg(3, slice_rows));
        CL_CHECK(kernel.setArg(4, row_start));

        size_t global_size = static_cast<size_t>(width) * slice_rows;
        std::vector<cl::Event> wait_write = {ds.ev_write};
        CL_CHECK(ds.queue.enqueueNDRangeKernel(kernel, cl::NullRange,
                                               cl::NDRange(global_size), cl::NullRange,
                                               &wait_write, &ds.ev_kernel));

        // Read result back into the correct slice of the merged output buffer.
        std::vector<cl::Event> wait_kernel = {ds.ev_kernel};
        CL_CHECK(ds.queue.enqueueReadBuffer(ds.buf_out, CL_FALSE, 0, slice_bytes,
                                            output.data() + slice_offset,
                                            &wait_kernel, &ds.ev_read));

        timings[static_cast<size_t>(i)].device_name = devices[i].device.getInfo<CL_DEVICE_NAME>();
        states.push_back(std::move(ds));
    }

    // Synchronize all devices before reading event timestamps.
    for (auto& ds : states) {
        CL_CHECK(ds.queue.finish());
    }

    // Total N-GPU time: max(read_end) - min(write_start) via cl::Event profiling.
    // Per spec §7: CL_PROFILING_COMMAND_START/END in nanoseconds → convert to ms.
    cl_ulong global_start = std::numeric_limits<cl_ulong>::max();
    cl_ulong global_end   = 0;
    for (int i = 0; i < n; ++i) {
        auto& ds  = states[static_cast<size_t>(i)];
        cl_ulong w_start = ds.ev_write.getProfilingInfo<CL_PROFILING_COMMAND_START>();
        cl_ulong r_end   = ds.ev_read.getProfilingInfo<CL_PROFILING_COMMAND_END>();
        if (w_start < global_start) global_start = w_start;
        if (r_end   > global_end)   global_end   = r_end;

        timings[static_cast<size_t>(i)].kernel_ms = duration_ms(ds.ev_kernel);
    }

    return static_cast<double>(global_end - global_start) / 1e6;
}

// ──────────────────────────────────────────────────────────────────────────────
// print_timing_table — structured output per task spec
// ──────────────────────────────────────────────────────────────────────────────
static void print_timing_table(int n,
                               double t_single_ms,
                               double t_n_gpu_ms,
                               const std::vector<DeviceTiming>& timings) {
    double speedup = (t_n_gpu_ms > 0.0) ? t_single_ms / t_n_gpu_ms : 0.0;
    std::string n_label = "N-GPU parallel (N=" + std::to_string(n) + ")";

    std::cout << "\n";
    std::cout << "┌────────────────────────────┬──────────────┐\n";
    std::cout << "│ Configuration              │ Time (ms)    │\n";
    std::cout << "├────────────────────────────┼──────────────┤\n";

    std::cout << "│ Single-GPU (device 0)      │ "
              << std::fixed << std::setprecision(3) << std::right << std::setw(9)
              << t_single_ms << "    │\n";

    std::cout << "│ " << std::left << std::setw(27) << n_label
              << "│ "
              << std::fixed << std::setprecision(3) << std::right << std::setw(9)
              << t_n_gpu_ms << "    │\n";

    std::cout << "│ Speedup                    │ "
              << std::fixed << std::setprecision(3) << std::right << std::setw(9)
              << speedup << "    │\n";

    std::cout << "└────────────────────────────┴──────────────┘\n";

    std::cout << "\nPer-device kernel times:\n";
    for (size_t i = 0; i < timings.size(); ++i) {
        std::cout << "  [" << i << "] "
                  << std::left << std::setw(40) << timings[i].device_name.substr(0, 39)
                  << std::right << std::fixed << std::setprecision(3)
                  << timings[i].kernel_ms << " ms\n";
    }

    if (n == 1) {
        std::cout << "\n[NOTE] Only 1 device found; N-GPU leg is equivalent to single-GPU baseline.\n";
    }
}

// ──────────────────────────────────────────────────────────────────────────────
// main
// ──────────────────────────────────────────────────────────────────────────────
int main(int argc, char* argv[]) {
    CLI::App app{"MultiGPU_Strategy — slice-partition benchmark across OpenCL devices"};

    int         width  = 3840;
    int         height = 2160;
    int         n_gpus = 0;   // 0 = use all available
    std::string image_path;

    app.add_option("--width",  width,  "Image width  (default 3840)");
    app.add_option("--height", height, "Image height (default 2160)");
    app.add_option("--gpus",   n_gpus, "Max devices to use, 0 = all (default 0)");
    app.add_option("--image",  image_path, "Optional path to BMP/PNG source image (unused: benchmark uses synthetic data)");

    CLI11_PARSE(app, argc, argv);

    try {
        // ── Device enumeration ────────────────────────────────────────────
        auto all_devices = enumerate_devices();
        print_device_table(all_devices);

        // Apply --gpus limit
        if (n_gpus > 0 && static_cast<size_t>(n_gpus) < all_devices.size()) {
            all_devices.resize(static_cast<size_t>(n_gpus));
            std::cout << "Limiting to " << n_gpus << " device(s) per --gpus flag.\n\n";
        }
        int n = static_cast<int>(all_devices.size());

        // ── Input data ────────────────────────────────────────────────────
        std::vector<cl_uchar> input;
        if (!image_path.empty()) {
            int img_w = 0, img_h = 0, channels = 0;
            // Force RGBA (4 channels) regardless of source format.
            unsigned char* pixels = stbi_load(image_path.c_str(), &img_w, &img_h, &channels, 4);
            if (!pixels) {
                throw std::runtime_error("Failed to load image: " + image_path
                                         + " (" + stbi_failure_reason() + ")");
            }
            width  = img_w;
            height = img_h;
            size_t total_bytes = static_cast<size_t>(width) * height * 4;
            input.assign(pixels, pixels + total_bytes);
            stbi_image_free(pixels);
            std::cout << "Loaded image: " << image_path
                      << " (" << width << "x" << height << " RGBA)\n";
        } else {
            std::cout << "Generating synthetic " << width << "x" << height << " RGBA gradient...\n";
            input = generate_gradient(width, height);
        }

        // ── Load kernel source ────────────────────────────────────────────
        // WHY runtime load: separation of host and device code; allows hot-editing.
        std::string kernel_source = load_kernel_source("kernels/slice_kernel.cl");

        // ── Single-GPU baseline leg ───────────────────────────────────────
        std::cout << "Running single-GPU baseline on: "
                  << all_devices[0].device.getInfo<CL_DEVICE_NAME>() << "\n";
        std::vector<cl_uchar> output_single;
        double t_single_ms = run_single_gpu(all_devices[0], kernel_source,
                                            input, output_single, width, height);
        std::cout << "Single-GPU kernel time: " << std::fixed << std::setprecision(3)
                  << t_single_ms << " ms\n";

        // ── N-GPU parallel leg ────────────────────────────────────────────
        std::cout << "\nRunning N-GPU parallel leg across " << n << " device(s)...\n";
        std::vector<cl_uchar>     output_n;
        std::vector<DeviceTiming> timings;
        double t_n_gpu_ms = run_n_gpu(all_devices, kernel_source,
                                      input, output_n, width, height, timings);

        // ── Timing table ──────────────────────────────────────────────────
        print_timing_table(n, t_single_ms, t_n_gpu_ms, timings);

        // ── Correctness verification ──────────────────────────────────────
        // Apply process_slice a second time on the N-GPU output.
        // Two inversions == identity → result must match original input byte-for-byte.
        std::cout << "\nRunning correctness check (double-invert round-trip)...\n";
        std::vector<cl_uchar> output_verify;
        run_single_gpu(all_devices[0], kernel_source,
                       output_n, output_verify, width, height);

        bool match = (output_verify == input);
        if (match) {
            std::cout << "[PASS] Output verified\n";
        } else {
            for (size_t idx = 0; idx < input.size(); ++idx) {
                if (output_verify[idx] != input[idx]) {
                    std::cout << "[FAIL] Mismatch at pixel " << (idx / 4)
                              << " channel " << (idx % 4)
                              << ": expected " << static_cast<int>(input[idx])
                              << " got "      << static_cast<int>(output_verify[idx]) << "\n";
                    break;
                }
            }
        }

    } catch (const cl::Error& e) {
        std::cerr << "[CL ERROR] " << e.what() << " (code " << e.err() << ")\n";
        throw;
    } catch (const std::exception& e) {
        std::cerr << "[ERROR] " << e.what() << "\n";
        throw;
    }

    return 0;
}
