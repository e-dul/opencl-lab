/*
 * Applied OpenCL Lab — Module 1, Phase 3: Buffers Layout
 *
 * Compares three OpenCL buffer allocation strategies side-by-side.
 * All strategies run the same scalar MAD kernel on a 256×256 RGB image;
 * only how the source buffer is created differs.
 *
 * Strategies:
 *   1. Explicit Write   — CL_MEM_READ_ONLY + enqueueWriteBuffer
 *                         Upload is a discrete command → event-trackable.
 *
 *   2. Copy on Create   — CL_MEM_COPY_HOST_PTR
 *                         Driver copies host data into device memory inside
 *                         the cl::Buffer() constructor (synchronous, blocking).
 *                         Transfer cost is real but not event-trackable.
 *
 *   3. Use Host Ptr     — CL_MEM_USE_HOST_PTR
 *                         Hints the driver to use host memory directly.
 *                         Integrated GPU (Intel HD, Apple M1): truly zero-copy,
 *                         the kernel reads host RAM over the shared memory bus.
 *                         Discrete GPU (NVIDIA, AMD): driver may DMA-copy
 *                         implicitly — result varies by platform.
 *
 * Flow:
 *   1. Generate 256×256 RGB synthetic gradient
 *   2. Create OpenCL context + profiling queue (always enabled for this demo)
 *   3. Build MAD program once; reuse across all strategies
 *   4. run_strategy() × 3 → collect TimingResult per strategy
 *   5. Print comparison table to stdout
 *   6. Save output.bmp (from last strategy run)
 */

// stb — single-header image IO (implementations compiled here)
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image.h>
#include <stb_image_write.h>

#include "ocl_wrapper.hpp"    // create_context(), OclContext
#include "opencl_utils.hpp"   // load_kernel_source(), CL_CHECK

#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

// ── Constants ─────────────────────────────────────────────────────────────────

static constexpr float CONTRAST   = 1.2f;
static constexpr int   BRIGHTNESS = 10;

// ── Types ─────────────────────────────────────────────────────────────────────

struct TimingResult {
    std::string name;
    double upload_ms   = 0.0;   // 0.0 if transfer is hidden in constructor
    double kernel_ms   = 0.0;
    double download_ms = 0.0;
    bool   upload_tracked = false;  // true only for Explicit Write strategy
};

enum class Strategy { EXPLICIT_WRITE, COPY_ON_CREATE, USE_HOST_PTR };

// ── Profiling helper ──────────────────────────────────────────────────────────

// WHY / 1e6: getProfilingInfo returns nanoseconds; divide to convert to ms.
static double duration_ms(const cl::Event& e) {
    return (e.getProfilingInfo<CL_PROFILING_COMMAND_END>() -
            e.getProfilingInfo<CL_PROFILING_COMMAND_START>()) / 1e6;
}

// ── Synthetic image ───────────────────────────────────────────────────────────

static std::vector<uint8_t> make_gradient(int& width, int& height, int& channels) {
    width    = 256;
    height   = 256;
    channels = 3;
    std::vector<uint8_t> img(static_cast<size_t>(width * height * channels));
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const size_t idx = static_cast<size_t>((y * width + x) * channels);
            img[idx + 0] = static_cast<uint8_t>(x);    // R: left → right
            img[idx + 1] = static_cast<uint8_t>(y);    // G: top  → bottom
            img[idx + 2] = 128;                         // B: constant mid-grey
        }
    }
    return img;
}

// ── run_strategy ──────────────────────────────────────────────────────────────
//
// Runs the full upload → kernel → download pipeline for one buffer strategy.
// A fresh cl::Kernel and cl::Buffer are created each call so strategies are
// independent and do not share state.

static TimingResult run_strategy(
    const std::string&          label,
    Strategy                    strategy,
    const cl::Context&          ctx,
    cl::CommandQueue&           queue,
    const cl::Program&          program,
    const std::vector<uint8_t>& src,
    std::vector<uint8_t>&       dst,
    size_t                      total_bytes
) {
    TimingResult result;
    result.name = label;

    // dst buffer is always a plain device allocation; we read it back explicitly.
    cl::Buffer buf_dst(ctx, CL_MEM_WRITE_ONLY, total_bytes);

    cl::Event ev_kernel, ev_read;

    if (strategy == Strategy::EXPLICIT_WRITE) {
        // ── Strategy 1: Explicit Write ─────────────────────────────────────────
        // Allocate device memory, then explicitly enqueue the host→device copy.
        // This is the only strategy where the upload is a discrete CL command
        // and therefore fully event-trackable.
        cl::Buffer buf_src(ctx, CL_MEM_READ_ONLY, total_bytes);

        cl::Event ev_write;
        queue.enqueueWriteBuffer(buf_src, CL_FALSE, 0, total_bytes,
                                 src.data(), nullptr, &ev_write);

        cl::Kernel kernel(program, "mad_kernel");
        kernel.setArg(0, buf_src);
        kernel.setArg(1, buf_dst);
        kernel.setArg(2, CONTRAST);
        kernel.setArg(3, BRIGHTNESS);

        queue.enqueueNDRangeKernel(kernel, cl::NullRange,
                                   cl::NDRange(total_bytes), cl::NullRange,
                                   nullptr, &ev_kernel);
        queue.enqueueReadBuffer(buf_dst, CL_FALSE, 0, total_bytes,
                                dst.data(), nullptr, &ev_read);
        queue.finish();

        result.upload_ms      = duration_ms(ev_write);
        result.upload_tracked = true;

    } else if (strategy == Strategy::COPY_ON_CREATE) {
        // ── Strategy 2: Copy on Create ─────────────────────────────────────────
        // WHY CL_MEM_COPY_HOST_PTR: the driver copies host data into device
        // memory synchronously as part of clCreateBuffer (inside the cl::Buffer
        // constructor). The transfer is real but happens before any CL command
        // is enqueued, so there is no cl_event to attach to it.
        //
        // WHY const_cast: OpenCL C API declares host_ptr as void* (not const),
        // matching pre-C99 conventions. The COPY flag guarantees the driver only
        // reads this pointer, so the cast is safe.
        cl::Buffer buf_src(ctx,
                           CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                           total_bytes,
                           const_cast<uint8_t*>(src.data()));

        cl::Kernel kernel(program, "mad_kernel");
        kernel.setArg(0, buf_src);
        kernel.setArg(1, buf_dst);
        kernel.setArg(2, CONTRAST);
        kernel.setArg(3, BRIGHTNESS);

        queue.enqueueNDRangeKernel(kernel, cl::NullRange,
                                   cl::NDRange(total_bytes), cl::NullRange,
                                   nullptr, &ev_kernel);
        queue.enqueueReadBuffer(buf_dst, CL_FALSE, 0, total_bytes,
                                dst.data(), nullptr, &ev_read);
        queue.finish();

        result.upload_ms      = 0.0;    // hidden in constructor — not trackable
        result.upload_tracked = false;

    } else {
        // ── Strategy 3: Use Host Ptr ───────────────────────────────────────────
        // WHY CL_MEM_USE_HOST_PTR: hints the driver to map device memory onto
        // the existing host allocation rather than allocating separate device RAM.
        //
        // Integrated GPU (Intel HD, Apple M1 — shared physical memory):
        //   The kernel reads host RAM directly via the shared bus. True zero-copy;
        //   upload cost is effectively 0.
        //
        // Discrete GPU (NVIDIA, AMD — separate VRAM):
        //   The driver may still DMA-copy the data into a device-side mirror, but
        //   this transfer is implicit and not event-trackable. upload_ms = 0 here
        //   even though physical bytes did move.
        //
        // Alignment note: OpenCL requires host_ptr to be aligned to the device's
        // minimum data type size. std::vector<uint8_t> satisfies uchar alignment.
        cl::Buffer buf_src(ctx,
                           CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR,
                           total_bytes,
                           const_cast<uint8_t*>(src.data()));

        cl::Kernel kernel(program, "mad_kernel");
        kernel.setArg(0, buf_src);
        kernel.setArg(1, buf_dst);
        kernel.setArg(2, CONTRAST);
        kernel.setArg(3, BRIGHTNESS);

        queue.enqueueNDRangeKernel(kernel, cl::NullRange,
                                   cl::NDRange(total_bytes), cl::NullRange,
                                   nullptr, &ev_kernel);
        queue.enqueueReadBuffer(buf_dst, CL_FALSE, 0, total_bytes,
                                dst.data(), nullptr, &ev_read);
        queue.finish();

        result.upload_ms      = 0.0;    // zero-copy or implicit copy by driver
        result.upload_tracked = false;
    }

    // WHY finish() before duration_ms(): profiling timestamps are only valid
    // once the event has reached CL_COMPLETE. queue.finish() ensures all
    // enqueued commands — including reads — have completed before we query.
    result.kernel_ms   = duration_ms(ev_kernel);
    result.download_ms = duration_ms(ev_read);

    return result;
}

// ── Table printer ─────────────────────────────────────────────────────────────

static void print_table(const std::vector<TimingResult>& results,
                         int width, int height) {
    const int W = 18;   // strategy name column width
    const int N = 14;   // numeric column width

    const std::string sep(W + N * 4 + 3, '-');

    std::cout << "\nBuffer Strategy Comparison — "
              << width << "×" << height << " RGB"
              << "  (contrast=" << CONTRAST
              << ", brightness=" << BRIGHTNESS << ")\n"
              << sep << "\n"
              << std::left  << std::setw(W) << "Strategy"
              << std::right
              << std::setw(N) << "Upload(ms)"
              << std::setw(N) << "Kernel(ms)"
              << std::setw(N) << "Download(ms)"
              << std::setw(N) << "Total(ms)"
              << "\n" << sep << "\n";

    std::cout << std::fixed << std::setprecision(3);
    for (const auto& r : results) {
        const double total = r.upload_ms + r.kernel_ms + r.download_ms;
        const std::string upload_str = r.upload_tracked
            ? (std::to_string(static_cast<int>(r.upload_ms)) + "")  // placeholder
            : "0.000 *";

        std::cout << std::left  << std::setw(W) << r.name
                  << std::right;

        // Upload column — append '*' marker if not event-trackable
        std::cout << std::setw(N - 2);
        if (r.upload_tracked) {
            std::cout << r.upload_ms << "  ";
        } else {
            std::cout << "0.000" << " *";
        }

        std::cout << std::setw(N) << r.kernel_ms
                  << std::setw(N) << r.download_ms;

        if (!r.upload_tracked) {
            // Total excludes hidden upload cost — mark it
            std::cout << std::setw(N - 2) << total << " *";
        } else {
            std::cout << std::setw(N) << total;
        }
        std::cout << "\n";
    }

    std::cout << sep << "\n"
              << "* Upload cost is hidden inside cl::Buffer() constructor "
                 "(not event-trackable).\n"
              << "  Total for strategies 2 & 3 excludes this hidden transfer overhead.\n";
}

// ── Main ──────────────────────────────────────────────────────────────────────

int main() {
    try {
        // 1. Synthetic source image
        int width = 0, height = 0, channels = 0;
        const std::vector<uint8_t> src = make_gradient(width, height, channels);
        const size_t total_bytes = static_cast<size_t>(width * height * channels);

        std::vector<uint8_t> dst(total_bytes);

        std::cout << "Generated " << width << "×" << height
                  << " RGB gradient (" << total_bytes << " bytes).\n";

        // 2. OpenCL setup
        OclContext ocl = create_context();

        // WHY always-on profiling: this demo exists to measure timing — there
        // is no reason to make profiling optional here. The overhead of
        // CL_QUEUE_PROFILING_ENABLE is part of what we are studying.
        cl::CommandQueue queue(ocl.context, ocl.device, CL_QUEUE_PROFILING_ENABLE);

        // 3. Build program once; all strategies share the same compiled kernel.
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

        // 4. Run all three strategies
        std::vector<TimingResult> results;

        results.push_back(run_strategy(
            "Explicit Write", Strategy::EXPLICIT_WRITE,
            ocl.context, queue, program, src, dst, total_bytes));

        results.push_back(run_strategy(
            "Copy on Create", Strategy::COPY_ON_CREATE,
            ocl.context, queue, program, src, dst, total_bytes));

        results.push_back(run_strategy(
            "Use Host Ptr", Strategy::USE_HOST_PTR,
            ocl.context, queue, program, src, dst, total_bytes));

        // 5. Print comparison table
        print_table(results, width, height);

        // 6. Save output from last strategy run (Use Host Ptr result)
        const char* out_path = "output.bmp";
        if (!stbi_write_bmp(out_path, width, height, channels, dst.data())) {
            throw std::runtime_error("stbi_write_bmp failed");
        }
        std::cout << "\nWritten: " << out_path << "\n";

    } catch (const cl::Error& e) {
        std::cerr << "OpenCL error: " << e.what() << " (" << e.err() << ")\n";
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
