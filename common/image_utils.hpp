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
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

// Load a raw binary file into a byte vector.
// Validates that the file size exactly matches expected_bytes. Throws on any
// error (file not found, size mismatch, read failure).
inline std::vector<uint8_t> load_raw_binary(const std::string& path,
                                             size_t expected_bytes) {
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open()) {
        throw std::runtime_error("Cannot open file: " + path);
    }

    f.seekg(0, std::ios::end);
    const auto file_size = static_cast<size_t>(f.tellg());
    f.seekg(0, std::ios::beg);

    if (file_size != expected_bytes) {
        throw std::runtime_error(
            "File size mismatch for '" + path + "': expected " +
            std::to_string(expected_bytes) + " bytes, got " +
            std::to_string(file_size));
    }

    std::vector<uint8_t> data(expected_bytes);
    f.read(reinterpret_cast<char*>(data.data()),
           static_cast<std::streamsize>(expected_bytes));
    if (!f) {
        throw std::runtime_error("Read error: " + path);
    }
    return data;
}

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
    const size_t n = static_cast<size_t>(width) * height * 3;  // §7.1: promote before multiply
    std::vector<uint8_t> data(raw, raw + n);
    stbi_image_free(raw);
    return data;
}

// Load image from disk, force-converted to RGBA (4 channels).
// Returns pixel data in row-major order. Throws on failure.
inline std::vector<uint8_t> load_rgba_image(const std::string& path,
                                             int& width, int& height,
                                             int& channels) {
    int loaded = 0;
    uint8_t* raw = stbi_load(path.c_str(), &width, &height, &loaded, 4);
    if (!raw) {
        throw std::runtime_error("stbi_load failed: " +
                                 std::string(stbi_failure_reason()));
    }
    channels = 4;
    const size_t n = static_cast<size_t>(width) * height * 4;  // §7.1: promote before multiply
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
