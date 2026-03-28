// sub_buffers_demo — partitions a single parent cl::Buffer into N sub-buffers
// and dispatches a separate fill kernel to each sub-buffer region.
//
// Key teaching points:
//   1. createSubBuffer() aliases a slice of the parent — no extra allocation,
//      no host memcpy.  The parent and all sub-buffers share the same device
//      memory; writes through a sub-buffer are immediately visible via the parent.
//   2. CL_DEVICE_MEM_BASE_ADDR_ALIGN must be queried at runtime.  Sub-buffer
//      origins that are not a multiple of this value trigger CL_MISALIGNED_SUB_BUFFER_OFFSET
//      at createSubBuffer() time.  Hardcoding 64 or 128 bytes silently fails on
//      some hardware (e.g. RDNA GPUs require 256-byte alignment).
//   3. Zero data movement: the parent buffer is written once; each strip is
//      overwritten exclusively through its own sub-buffer kernel —
//      no enqueueWriteBuffer to sub-regions ever.

// WHY both STB_IMAGE_IMPLEMENTATION and STB_IMAGE_WRITE_IMPLEMENTATION:
// image_utils.hpp is a thin wrapper that assumes both stb headers have been
// included with their implementation defines exactly once in this translation
// unit.  Including only the write half causes link errors from missing stbi_load.
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>
#include "image_utils.hpp"
#include "opencl_utils.hpp"  // CL_CHECK, build_program, get_binary_dir
#include "ocl_wrapper.hpp"   // create_context(), OclContext

#include <CLI/CLI.hpp>

#include <array>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <limits>
#include <vector>

// ---------------------------------------------------------------------------
// strip_color — return a visually distinct RGBA color for strip index i out
// of total strips.  Uses evenly spaced hues on a simple 6-sector HSV wheel
// converted to RGB.  Alpha is always 255.
// ---------------------------------------------------------------------------
static std::array<uint8_t, 4> strip_color(int i, int total) {
    const float hue = (360.0f * static_cast<float>(i)) / static_cast<float>(total);

    // HSV → RGB with S=V=1 (fully saturated colors for maximum visual contrast).
    const float h = hue / 60.0f;
    const int   sector = static_cast<int>(h) % 6;
    const float f = h - static_cast<float>(static_cast<int>(h));
    const float v = 1.0f;
    const float p = 0.0f;  // v*(1-s) with s=1
    const float q = v * (1.0f - f);
    const float t = v * f;

    float r, g, b;
    switch (sector) {
        case 0: r = v; g = t; b = p; break;
        case 1: r = q; g = v; b = p; break;
        case 2: r = p; g = v; b = t; break;
        case 3: r = p; g = q; b = v; break;
        case 4: r = t; g = p; b = v; break;
        default: r = v; g = p; b = q; break;
    }

    return {
        static_cast<uint8_t>(r * 255.0f),
        static_cast<uint8_t>(g * 255.0f),
        static_cast<uint8_t>(b * 255.0f),
        255u
    };
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main(int argc, char** argv) {
    // ── CLI ─────────────────────────────────────────────────────────────────
    CLI::App app{"sub_buffers — partition a parent buffer into N sub-regions, fill each with a distinct color"};

    int         width  = 512;
    int         height = 512;
    int         strips = 4;
    std::string output = "output.bmp";

    app.add_option("--width",  width,  "Image width in pixels  (default 512)");
    app.add_option("--height", height, "Image height in pixels (default 512)");
    app.add_option("--strips", strips, "Number of horizontal strips / sub-buffers (default 4)");
    app.add_option("--output", output, "Output BMP file path (default output.bmp)");

    CLI11_PARSE(app, argc, argv);

    if (width <= 0 || height <= 0 || strips <= 0) {
        throw std::runtime_error("--width, --height, and --strips must all be > 0");
    }
    if (strips > height) {
        throw std::runtime_error("--strips cannot exceed --height");
    }

    // ── OpenCL context ──────────────────────────────────────────────────────
    OclContext ocl = create_context();

    // ── Alignment query ─────────────────────────────────────────────────────
    // WHY runtime query: CL_DEVICE_MEM_BASE_ADDR_ALIGN is hardware-dependent.
    // RDNA (AMD) may require 256-byte alignment; older Intel iGPUs may require 64 bytes.
    // Hardcoding any value triggers CL_MISALIGNED_SUB_BUFFER_OFFSET on mismatch.
    // The spec returns the value in *bits*, so we convert to bytes.
    cl_uint align_bits = 0;
    CL_CHECK(ocl.device.getInfo(CL_DEVICE_MEM_BASE_ADDR_ALIGN, &align_bits));
    const size_t align_bytes = static_cast<size_t>(align_bits) / 8u;

    std::cout << "Alignment : " << align_bytes << " bytes  "
              << "(CL_DEVICE_MEM_BASE_ADDR_ALIGN = " << align_bits << " bits)\n";

    // ── Compute strip geometry ───────────────────────────────────────────────
    // Each strip is a horizontal band: width * strip_height RGBA (4-byte) pixels.
    // Sub-buffer origin = strip_index * strip_bytes must be a multiple of align_bytes.
    //
    // WHY round up strip_bytes: if (ceil(height/strips) * row_bytes) is not
    // aligned, we pad strip_bytes to the next alignment boundary so every strip
    // start address is aligned.  The last strip may have padding rows — the kernel
    // fills them, but we crop before writing the BMP.
    const size_t row_bytes          = static_cast<size_t>(width) * 4u;  // RGBA
    const int    nominal_strip_h    = (height + strips - 1) / strips;   // ceiling div
    const size_t nominal_strip_bytes = static_cast<size_t>(nominal_strip_h) * row_bytes;

    // Round up to alignment boundary.
    size_t strip_bytes = nominal_strip_bytes;
    if (align_bytes > 0 && strip_bytes % align_bytes != 0) {
        strip_bytes = ((strip_bytes + align_bytes - 1) / align_bytes) * align_bytes;
    }

    const size_t total_bytes  = strip_bytes * static_cast<size_t>(strips);
    const int    strip_height = static_cast<int>(strip_bytes / row_bytes);

    // Sanity check: every strip origin must satisfy the alignment constraint.
    for (int i = 0; i < strips; ++i) {
        const size_t origin = strip_bytes * static_cast<size_t>(i);
        if (align_bytes > 0 && origin % align_bytes != 0) {
            throw std::runtime_error("Internal alignment error at strip " + std::to_string(i));
        }
    }

    std::cout << "Image     : " << width << " x " << height << " (" << strips << " strips)\n";
    std::cout << "Strip     : " << strip_height << " rows, " << strip_bytes << " bytes\n";
    std::cout << "Buffer    : " << total_bytes << " bytes total\n";

    // ── Parent buffer ────────────────────────────────────────────────────────
    // WHY CL_MEM_READ_WRITE: sub-buffers inherit the parent's flags and must be
    // a strict subset of them.  READ_WRITE is required so the fill kernels can
    // write through the sub-buffer alias.
    cl::Buffer parent_buf(ocl.context, CL_MEM_READ_WRITE, total_bytes);

    // ── Kernel ──────────────────────────────────────────────────────────────
    const auto        bin_dir     = get_binary_dir();
    const std::string kernel_path = (bin_dir / "kernels" / "sub_buffer_demo.cl").string();
    cl::Program       program     = build_program(ocl.context, ocl.device, kernel_path);
    cl::Kernel        kernel(program, "fill_strip");

    // ── Sub-buffer dispatch loop ─────────────────────────────────────────────
    for (int i = 0; i < strips; ++i) {
        // Build the region descriptor: origin and size within the parent buffer.
        cl_buffer_region region{};
        region.origin = strip_bytes * static_cast<size_t>(i);
        region.size   = strip_bytes;

        // WHY createSubBuffer instead of enqueueWriteBuffer sub-range:
        // createSubBuffer creates a zero-copy alias — the driver adjusts the base
        // pointer to the sub-region; no device memory is copied or reallocated.
        // After all dispatches, enqueueReadBuffer on the PARENT retrieves all
        // strips in one call, proving the writes landed in the same allocation.
        cl_int     sub_err = CL_SUCCESS;
        cl::Buffer sub_buf = parent_buf.createSubBuffer(
            CL_MEM_READ_WRITE,
            CL_BUFFER_CREATE_TYPE_REGION,
            &region,
            &sub_err);
        CL_CHECK(sub_err);

        // Distinct hue per strip for visual differentiation in the BMP.
        const auto col = strip_color(i, strips);
        cl_uchar4  fill_color{col[0], col[1], col[2], col[3]};

        // Fill all pixels in this sub-buffer slot (including any alignment padding).
        // §7.1: guard against strip_bytes so large that slot_pixels would overflow int.
        if (strip_bytes / 4u > static_cast<size_t>(std::numeric_limits<int>::max()))
            throw std::runtime_error("strip pixel count exceeds INT_MAX");
        const int slot_pixels = static_cast<int>(strip_bytes / 4u);

        CL_CHECK(kernel.setArg(0, sub_buf));
        CL_CHECK(kernel.setArg(1, fill_color));
        CL_CHECK(kernel.setArg(2, slot_pixels));

        // WHY cl::NullRange for local size: lets the runtime choose the optimal
        // work-group size, removing the need to round up global work size.
        CL_CHECK(ocl.queue.enqueueNDRangeKernel(
            kernel,
            cl::NullRange,
            cl::NDRange(static_cast<size_t>(slot_pixels)),
            cl::NullRange));
    }

    CL_CHECK(ocl.queue.finish());

    // ── Read back parent buffer ──────────────────────────────────────────────
    // WHY read the parent (not individual sub-buffers): this validates the
    // zero-copy guarantee — all sub-buffer writes landed in the same underlying
    // device allocation; the parent's view assembles them for free.
    std::vector<uint8_t> host_pixels(total_bytes);
    CL_CHECK(ocl.queue.enqueueReadBuffer(
        parent_buf, CL_TRUE, 0, total_bytes, host_pixels.data()));

    // ── Crop to requested height ─────────────────────────────────────────────
    // Padding rows may push total_bytes beyond width * height * 4.  Resize to
    // the exact requested canvas before writing the BMP.
    const size_t out_bytes = static_cast<size_t>(width) * static_cast<size_t>(height) * 4u;
    host_pixels.resize(out_bytes);

    // ── Write BMP ───────────────────────────────────────────────────────────
    save_bmp(output, host_pixels, width, height, 4);
    std::cout << "Output    : " << output << "\n";

    return 0;
}
