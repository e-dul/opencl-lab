#pragma once
// graphics_hpc_utils.hpp — Shared host-side utilities for Graphics & HPC modules.
//
// WHY this header exists: B2, B3, B3_Dynamic, and B4 all re-implemented
// save_framebuffer verbatim. Centralising here means a single edit propagates
// to all modules.
//
// round_up, get_binary_dir, and build_program live in opencl_utils.hpp —
// they are general OpenCL helpers, not graphics-specific.

#include "opencl_utils.hpp"
#include "image_utils.hpp"

#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// save_framebuffer — read a float RGBA buffer from the GPU, tonemap to uint8,
// and write a BMP file.
//
// WHY float → uint8 conversion here: all ray-tracing modules produce a flat
// cl::Buffer of RGBA floats in [0,1]. The conversion and clamping logic is
// identical in every module; centralising avoids subtle divergence (e.g. one
// module forgetting to clamp).
//
// Parameters:
//   queue       — command queue to use for enqueueReadBuffer
//   fb_buf      — GPU buffer containing width*height*4 floats (RGBA, [0,1])
//   width/height — image dimensions in pixels
//   output_path — destination BMP file path
// ---------------------------------------------------------------------------
inline void save_framebuffer(const cl::CommandQueue& queue,
                              const cl::Buffer&       fb_buf,
                              int                     width,
                              int                     height,
                              const std::string&      output_path)
{
    // §7.1: promote to size_t before multiply to avoid signed overflow
    std::vector<float> pixel_f(static_cast<size_t>(width) * height * 4);
    CL_CHECK(queue.enqueueReadBuffer(fb_buf, CL_TRUE, 0,
                                     pixel_f.size() * sizeof(float),
                                     pixel_f.data()));

    std::vector<uint8_t> pixel_u8(pixel_f.size());
    for (size_t i = 0; i < pixel_f.size(); ++i) {
        float c = pixel_f[i] < 0.0f ? 0.0f : (pixel_f[i] > 1.0f ? 1.0f : pixel_f[i]);
        pixel_u8[i] = static_cast<uint8_t>(c * 255.0f + 0.5f);
    }
    save_bmp(output_path, pixel_u8, width, height, 4);
    std::cout << "Saved: " << output_path << "\n";
}

