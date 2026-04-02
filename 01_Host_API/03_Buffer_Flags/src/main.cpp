/*
 * Applied OpenCL Lab — Module 1, Phase 3: Buffer Flags
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
 *   1. Parse CLI args (--contrast, --brightness)
 *   2. Generate 256×256 RGB synthetic gradient
 *   3. Create OpenCL context + profiling queue (always enabled for this demo)
 *   4. Build MAD program once; reuse across all strategies
 *   5. run_strategy() × 3 → collect TimingResult per strategy
 *   6. Print comparison table to stdout
 *   7. Save output.bmp (from last strategy run)
 */

// stb — single-header image IO (implementations compiled here)
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image.h>
#include <stb_image_write.h>

#include "image_utils.hpp"    // make_gradient(), save_bmp()
#include "ocl_wrapper.hpp"    // create_context(), OclContext
#include "opencl_utils.hpp"   // load_kernel_source(), build_program(), CL_CHECK, duration_ms()

#include <CLI/CLI.hpp>

#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

// ── Types ─────────────────────────────────────────────────────────────────────

struct TimingResult {
    std::string name;
    double upload_ms   = 0.0;   // 0.0 if transfer is hidden in constructor
    double kernel_ms   = 0.0;
    double download_ms = 0.0;
    bool   upload_tracked = false;  // true only for Explicit Write strategy
};

enum class Strategy { EXPLICIT_WRITE, COPY_ON_CREATE, USE_HOST_PTR };

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
    size_t                      total_bytes,
    float                       contrast,
    int                         brightness
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
        CL_CHECK(queue.enqueueWriteBuffer(buf_src, CL_FALSE, 0, total_bytes,
                                          src.data(), nullptr, &ev_write));

        cl::Kernel kernel(program, "mad_kernel");
        CL_CHECK(kernel.setArg(0, buf_src));
        CL_CHECK(kernel.setArg(1, buf_dst));
        CL_CHECK(kernel.setArg(2, contrast));
        CL_CHECK(kernel.setArg(3, brightness));
        // WHY cast to cl_int: kernel parameter is declared as int.
        CL_CHECK(kernel.setArg(4, static_cast<cl_int>(total_bytes)));

        CL_CHECK(queue.enqueueNDRangeKernel(kernel, cl::NullRange,
                                             cl::NDRange(total_bytes), cl::NullRange,
                                             nullptr, &ev_kernel));
        CL_CHECK(queue.enqueueReadBuffer(buf_dst, CL_FALSE, 0, total_bytes,
                                         dst.data(), nullptr, &ev_read));
        CL_CHECK(queue.finish());

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
        CL_CHECK(kernel.setArg(0, buf_src));
        CL_CHECK(kernel.setArg(1, buf_dst));
        CL_CHECK(kernel.setArg(2, contrast));
        CL_CHECK(kernel.setArg(3, brightness));
        // WHY cast to cl_int: kernel parameter is declared as int.
        CL_CHECK(kernel.setArg(4, static_cast<cl_int>(total_bytes)));

        CL_CHECK(queue.enqueueNDRangeKernel(kernel, cl::NullRange,
                                             cl::NDRange(total_bytes), cl::NullRange,
                                             nullptr, &ev_kernel));
        CL_CHECK(queue.enqueueReadBuffer(buf_dst, CL_FALSE, 0, total_bytes,
                                         dst.data(), nullptr, &ev_read));
        CL_CHECK(queue.finish());

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
        CL_CHECK(kernel.setArg(0, buf_src));
        CL_CHECK(kernel.setArg(1, buf_dst));
        CL_CHECK(kernel.setArg(2, contrast));
        CL_CHECK(kernel.setArg(3, brightness));

        // WHY cast to cl_int: kernel parameter is declared as int.
        CL_CHECK(kernel.setArg(4, static_cast<cl_int>(total_bytes)));

        CL_CHECK(queue.enqueueNDRangeKernel(kernel, cl::NullRange,
                                             cl::NDRange(total_bytes), cl::NullRange,
                                             nullptr, &ev_kernel));
        CL_CHECK(queue.enqueueReadBuffer(buf_dst, CL_FALSE, 0, total_bytes,
                                         dst.data(), nullptr, &ev_read));
        CL_CHECK(queue.finish());

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
                         int width, int height,
                         float contrast, int brightness) {
    const int W = 18;   // strategy name column width
    const int N = 14;   // numeric column width

    const std::string sep(W + N * 4 + 3, '-');

    std::cout << "\nBuffer Strategy Comparison — "
              << width << "x" << height << " RGB"
              << "  (contrast=" << contrast
              << ", brightness=" << brightness << ")\n"
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

int main(int argc, char* argv[]) {
    try {
        float contrast   = 1.2f;
        int   brightness = 10;

        CLI::App app{"Buffer strategy benchmark"};
        app.add_option("-c,--contrast",   contrast,   "Contrast multiplier (default: 1.2)");
        app.add_option("-b,--brightness", brightness, "Brightness addend   (default: 10)");
        CLI11_PARSE(app, argc, argv);

        // 1. Synthetic source image
        int width = 0, height = 0, channels = 0;
        const std::vector<uint8_t> src = make_gradient(width, height, channels);

        // WHY promote width first: width * height * channels as plain int
        // multiplication overflows before the cast on large images (e.g. 4K).
        const size_t total_bytes = static_cast<size_t>(width) * height * channels;

        std::vector<uint8_t> dst(total_bytes);

        std::cout << "Generated " << width << "x" << height
                  << " RGB gradient (" << total_bytes << " bytes).\n";

        // 2. OpenCL setup
        OclContext ocl = create_context();

        // WHY always-on profiling: this demo exists to measure timing — there
        // is no reason to make profiling optional here. The overhead of
        // CL_QUEUE_PROFILING_ENABLE is part of what we are studying.
        cl::CommandQueue queue(ocl.context, ocl.device, CL_QUEUE_PROFILING_ENABLE);

        // 3. Build program once; all strategies share the same compiled kernel.
        // WHY build_program(): centralises the try/catch + getBuildInfo log so every
        // module surfaces the same diagnostic on kernel compile errors.
        cl::Program program = build_program(ocl.context, ocl.device, "kernels/mad.cl");

        // 4. Run all three strategies
        std::vector<TimingResult> results;

        results.push_back(run_strategy(
            "Explicit Write", Strategy::EXPLICIT_WRITE,
            ocl.context, queue, program, src, dst, total_bytes,
            contrast, brightness));

        results.push_back(run_strategy(
            "Copy on Create", Strategy::COPY_ON_CREATE,
            ocl.context, queue, program, src, dst, total_bytes,
            contrast, brightness));

        results.push_back(run_strategy(
            "Use Host Ptr", Strategy::USE_HOST_PTR,
            ocl.context, queue, program, src, dst, total_bytes,
            contrast, brightness));

        // 5. Print comparison table
        print_table(results, width, height, contrast, brightness);

        // 6. Save output from last strategy run (Use Host Ptr result)
        save_bmp("output.bmp", dst, width, height, channels);
        std::cout << "\nWritten: output.bmp\n";

    } catch (const cl::Error& e) {
        std::cerr << "OpenCL error: " << e.what() << " (" << e.err() << ")\n";
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
