#pragma once

// image_utils.hpp — header-only image IO helpers (stb-backed)
//
// IMPORTANT: The caller's .cpp must include stb headers WITH implementation
// defines BEFORE including this header, exactly once per binary:
//
//   #define STB_IMAGE_IMPLEMENTATION
//   #define STB_IMAGE_WRITE_IMPLEMENTATION
//   #include <stb_image.h>
//   #include <stb_image_write.h>
//   #include "image_utils.hpp"
//
// WHY no stb #include here: stb_image_write.h places its implementation
// section OUTSIDE its include guard, so re-including it would trigger
// "redefinition" errors even with guards in place.

#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

// Load image from disk, force-converted to RGB (3 channels, no alpha).
// Returns pixel data in row-major order. Throws on failure.
inline std::vector<uint8_t> load_rgb_image(const std::string& path,
                                            int& width, int& height,
                                            int& channels) {
    int loaded = 0;
    uint8_t* raw = stbi_load(path.c_str(), &width, &height, &loaded, 3);
    if (!raw) {
        throw std::runtime_error("stbi_load failed: " +
                                 std::string(stbi_failure_reason()));
    }
    channels = 3;
    const size_t n = static_cast<size_t>(width * height * 3);
    std::vector<uint8_t> data(raw, raw + n);
    stbi_image_free(raw);
    return data;
}

// Save pixel data as BMP. Throws on failure.
inline void save_bmp(const std::string& path,
                     const std::vector<uint8_t>& data,
                     int width, int height, int channels) {
    if (!stbi_write_bmp(path.c_str(), width, height, channels, data.data())) {
        throw std::runtime_error("stbi_write_bmp failed: " + path);
    }
}

// Generate a 256×256 RGB gradient (R=x, G=y, B=128).
inline std::vector<uint8_t> make_gradient(int& width, int& height,
                                           int& channels) {
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
