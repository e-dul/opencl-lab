/*
 * Applied OpenCL Lab — Add-on 4.1: vkFFT Audio Spectrogram
 *
 * Flow:
 *   1. Parse CLI args (--input, --fft-size, --hop-size, --output)
 *   2. Load .wav → float PCM samples (mono, normalized)
 *   3. Segment into overlapping frames → cl::Buffer (interleaved complex)
 *   4. vkFFT batch forward FFT (OpenCL backend, VKFFT_BACKEND=3)
 *   5. magnitude.cl kernel → positive-spectrum magnitudes
 *   6. [Optional] FFTW3 CPU reference + speedup report
 *   7. Log-compress + jet colormap → output_spectrogram.bmp
 */

// ── stb implementations (compiled exactly once in this TU) ───────────────────
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

// ── vkFFT OpenCL backend ─────────────────────────────────────────────────────
// VKFFT_BACKEND=3 selects the OpenCL path inside vkFFT.h.
// It must be defined before including vkFFT.h; CMakeLists.txt does this via
// target_compile_definitions so all TUs see it consistently.
#include "vkFFT.h"

// ── Project headers ──────────────────────────────────────────────────────────
#include "ocl_wrapper.hpp"   // create_context(), OclContext
#include "opencl_utils.hpp"  // CL_CHECK, load_kernel_source, build_program, duration_ms, get_binary_dir

// ── Optional CPU reference ───────────────────────────────────────────────────
#ifdef HAVE_FFTW3
#include <fftw3.h>
#endif

#include <CLI/CLI.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

// ── Jet colormap ─────────────────────────────────────────────────────────────
// Maps t ∈ [0, 1] to RGBA using the standard jet palette.
// Produces visible colour gradation so the spectrogram is not a solid image.
static std::array<uint8_t, 4> jet_rgba(float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    float r = std::clamp(1.5f - std::abs(4.0f * t - 3.0f), 0.0f, 1.0f);
    float g = std::clamp(1.5f - std::abs(4.0f * t - 2.0f), 0.0f, 1.0f);
    float b = std::clamp(1.5f - std::abs(4.0f * t - 1.0f), 0.0f, 1.0f);
    return {static_cast<uint8_t>(r * 255),
            static_cast<uint8_t>(g * 255),
            static_cast<uint8_t>(b * 255),
            255};
}

// ── WAV loader ────────────────────────────────────────────────────────────────
// Minimal hand-rolled RIFF/WAVE parser.
// Supports:
//   - Format tag 1 (PCM int16, normalized to float by /32768.0f)
//   - Format tag 3 (IEEE float32)
//   - Mono and stereo (stereo averaged to mono)
// Returns normalized float samples in [-1.0, 1.0].
static std::vector<float> load_wav(const std::string& path, int& sample_rate) {
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open())
        throw std::runtime_error("Cannot open WAV file: " + path);

    // Helper: read a little-endian value from the stream.
    auto read_u32 = [&]() -> uint32_t {
        uint32_t v = 0;
        f.read(reinterpret_cast<char*>(&v), 4);
        return v;
    };
    auto read_u16 = [&]() -> uint16_t {
        uint16_t v = 0;
        f.read(reinterpret_cast<char*>(&v), 2);
        return v;
    };

    // RIFF header: "RIFF" + file-size + "WAVE"
    char riff_id[4];
    f.read(riff_id, 4);
    if (std::string(riff_id, 4) != "RIFF")
        throw std::runtime_error("Not a RIFF file: " + path);
    read_u32(); // file size (unused)
    char wave_id[4];
    f.read(wave_id, 4);
    if (std::string(wave_id, 4) != "WAVE")
        throw std::runtime_error("RIFF is not WAVE: " + path);

    // Scan chunks until "fmt " and "data" are found.
    uint16_t format_tag   = 0;
    uint16_t num_channels = 0;
    uint16_t bits_per_sample = 0;
    uint32_t data_size    = 0;
    bool     found_fmt    = false;
    bool     found_data   = false;
    std::vector<float> samples;

    while (f.peek() != EOF) {
        char chunk_id[4];
        f.read(chunk_id, 4);
        if (f.gcount() < 4) break;
        uint32_t chunk_size = read_u32();

        if (std::string(chunk_id, 4) == "fmt ") {
            format_tag      = read_u16();
            num_channels    = read_u16();
            uint32_t sr     = read_u32();
            sample_rate     = static_cast<int>(sr);
            read_u32(); // byte rate
            read_u16(); // block align
            bits_per_sample = read_u16();
            found_fmt       = true;
            // Skip any extra fmt bytes (e.g. cbSize in extensible format)
            if (chunk_size > 16)
                f.seekg(chunk_size - 16, std::ios::cur);
        } else if (std::string(chunk_id, 4) == "data") {
            if (!found_fmt)
                throw std::runtime_error("WAV 'data' chunk found before 'fmt ': " + path);
            data_size  = chunk_size;
            found_data = true;

            if (format_tag == 1) {
                // PCM int16 — normalize to [-1, 1] by dividing by 32768
                if (bits_per_sample != 16)
                    throw std::runtime_error("PCM WAV with bits_per_sample != 16 not supported");
                size_t num_interleaved = data_size / 2;  // int16 samples (all channels)
                std::vector<int16_t> raw(num_interleaved);
                f.read(reinterpret_cast<char*>(raw.data()),
                       static_cast<std::streamsize>(data_size));
                // Downmix: average channels to mono
                size_t num_frames_wav = num_interleaved / num_channels;
                samples.resize(num_frames_wav);
                for (size_t i = 0; i < num_frames_wav; ++i) {
                    float sum = 0.0f;
                    for (int ch = 0; ch < num_channels; ++ch)
                        sum += static_cast<float>(raw[i * num_channels + ch]);
                    samples[i] = sum / (32768.0f * num_channels);
                }
            } else if (format_tag == 3) {
                // IEEE float32
                if (bits_per_sample != 32)
                    throw std::runtime_error("Float WAV with bits_per_sample != 32 not supported");
                size_t num_interleaved = data_size / 4;
                std::vector<float> raw(num_interleaved);
                f.read(reinterpret_cast<char*>(raw.data()),
                       static_cast<std::streamsize>(data_size));
                size_t num_frames_wav = num_interleaved / num_channels;
                samples.resize(num_frames_wav);
                for (size_t i = 0; i < num_frames_wav; ++i) {
                    float sum = 0.0f;
                    for (int ch = 0; ch < num_channels; ++ch)
                        sum += raw[i * num_channels + ch];
                    samples[i] = sum / static_cast<float>(num_channels);
                }
            } else {
                throw std::runtime_error(
                    "Unsupported WAV format tag: " + std::to_string(format_tag) +
                    " (only PCM=1 and float=3 are supported)");
            }
        } else {
            // Unknown chunk — skip it.
            f.seekg(chunk_size, std::ios::cur);
        }
    }

    if (!found_fmt)  throw std::runtime_error("WAV 'fmt ' chunk not found: " + path);
    if (!found_data) throw std::runtime_error("WAV 'data' chunk not found: " + path);
    if (samples.empty())
        throw std::runtime_error("WAV file contains no samples: " + path);

    return samples;
}

// ── main ─────────────────────────────────────────────────────────────────────
int main(int argc, char* argv[]) {
    try {
        // 1. CLI
        CLI::App app{"vkFFT Audio Spectrogram — GPU batch FFT via vkFFT (OpenCL backend)"};
        std::string input_path;
        std::string output_path = "output_spectrogram.bmp";
        int fft_size  = 2048;
        int hop_size  = 512;
        app.add_option("--input",    input_path,  "Path to .wav file")->required();
        app.add_option("--fft-size", fft_size,    "FFT size (power of 2)")->default_val(2048);
        app.add_option("--hop-size", hop_size,    "Hop size in samples")->default_val(512);
        app.add_option("--output",   output_path, "Output BMP path")->default_val("output_spectrogram.bmp");
        CLI11_PARSE(app, argc, argv);

        // 2. Load WAV
        int sample_rate = 0;
        std::vector<float> pcm = load_wav(input_path, sample_rate);
        std::cout << "Loaded: " << input_path
                  << "  samples=" << pcm.size()
                  << "  rate=" << sample_rate << " Hz\n";

        // 3. Compute frame count
        // WHY std::max(..., 1): ensure at least one frame even for very short files.
        const int num_frames = (pcm.size() >= static_cast<size_t>(fft_size))
            ? static_cast<int>((pcm.size() - fft_size) / hop_size + 1)
            : 1;
        const int num_bins    = fft_size / 2;          // positive-spectrum bins per frame
        const size_t total_bins = static_cast<size_t>(num_frames) * num_bins; // total output magnitudes

        std::cout << "Frames: " << num_frames
                  << "  FFT size: " << fft_size
                  << "  Hop: " << hop_size << "\n";

        // Overflow guard: buffer sizes use size_t arithmetic from here on.
        if (static_cast<size_t>(num_frames) > SIZE_MAX / (static_cast<size_t>(fft_size) * 2 * sizeof(float)))
            throw std::runtime_error("Frame count overflows buffer size");

        // 4. Build interleaved complex frames (im=0 for real input)
        // Buffer layout: [frame0_re0, frame0_im0, frame0_re1, ..., frameN-1_im(M-1)]
        const size_t complex_count = static_cast<size_t>(num_frames) * fft_size * 2;
        std::vector<float> complex_data(complex_count, 0.0f);

        for (int f_idx = 0; f_idx < num_frames; ++f_idx) {
            for (int b = 0; b < fft_size; ++b) {
                size_t pcm_idx    = static_cast<size_t>(f_idx) * hop_size + b;
                float  sample_val = (pcm_idx < pcm.size()) ? pcm[pcm_idx] : 0.0f;
                size_t buf_idx    = static_cast<size_t>(f_idx) * fft_size * 2 + b * 2;
                complex_data[buf_idx]     = sample_val; // real
                complex_data[buf_idx + 1] = 0.0f;       // imaginary
            }
        }

        // 5. OpenCL context — GPU-first, respects GPU env var
        OclContext ocl = create_context();

        // WHY separate profiling queue: create_context() constructs a default queue
        // without CL_QUEUE_PROFILING_ENABLE. vkFFT and our magnitude kernel both need
        // cl::Event timestamps, which require the profiling flag on the queue.
        cl::CommandQueue prof_queue(ocl.context, ocl.device, CL_QUEUE_PROFILING_ENABLE);
        cl_command_queue queue_raw = prof_queue();

        // 6. Upload complex frames to GPU
        const size_t buffer_bytes = complex_count * sizeof(float);
        // WHY CL_MEM_COPY_HOST_PTR: we want the data uploaded immediately at buffer
        // creation without keeping the host pointer alive after this call.
        cl::Buffer cl_buf(ocl.context,
                          CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
                          buffer_bytes,
                          complex_data.data());

        // Retrieve raw cl_mem handle — vkFFT takes pointers to cl_mem, not cl::Buffer.
        cl_mem   cl_buf_raw  = cl_buf();
        uint64_t buf_size_u64 = static_cast<uint64_t>(buffer_bytes);

        // 7. vkFFT configuration
        cl_context    ctx_raw = ocl.context();
        cl_device_id  dev_raw = ocl.device();

        VkFFTConfiguration vkfft_cfg = {};
        vkfft_cfg.FFTdim       = 1;
        vkfft_cfg.size[0]      = static_cast<uint64_t>(fft_size);
        vkfft_cfg.numberBatches = static_cast<uint64_t>(num_frames);
        vkfft_cfg.buffer        = &cl_buf_raw;
        vkfft_cfg.bufferSize    = &buf_size_u64;
        vkfft_cfg.context       = &ctx_raw;
        vkfft_cfg.device        = &dev_raw;
        vkfft_cfg.commandQueue  = &queue_raw;

        VkFFTApplication vkfft_app = {};
        VkFFTResult res = initializeVkFFT(&vkfft_app, vkfft_cfg);
        if (res != VKFFT_SUCCESS)
            throw std::runtime_error(
                "initializeVkFFT failed: code=" + std::to_string(res) +
                " (size=" + std::to_string(fft_size) +
                ", batches=" + std::to_string(num_frames) + ")");

        // 8. Launch forward FFT, capture timing via CL barrier events.
        // WHY barrier events: vkFFT OpenCL backend (VKFFT_BACKEND=3) does not expose a
        // per-kernel cl_event through VkFFTLaunchParams (fence is Vulkan-only). Two
        // barrier-with-wait-list markers bracket all vkFFT-enqueued work on the profiling
        // queue, giving a CL-timeline interval that satisfies §6 of the master spec.
        VkFFTLaunchParams launch = {};
        launch.commandQueue = &queue_raw;

        cl_event fft_start_raw, fft_end_raw;
        CL_CHECK(clEnqueueBarrierWithWaitList(queue_raw, 0, nullptr, &fft_start_raw));

        res = VkFFTAppend(&vkfft_app, -1, &launch);  // -1 = forward FFT
        if (res != VKFFT_SUCCESS)
            throw std::runtime_error(
                "VkFFTAppend failed: code=" + std::to_string(res) +
                " (size=" + std::to_string(fft_size) +
                ", batches=" + std::to_string(num_frames) + ")");

        CL_CHECK(clEnqueueBarrierWithWaitList(queue_raw, 0, nullptr, &fft_end_raw));
        CL_CHECK(clFinish(queue_raw));

        cl_ulong fft_t0 = 0, fft_t1 = 0;
        CL_CHECK(clGetEventProfilingInfo(fft_start_raw, CL_PROFILING_COMMAND_END,
                                         sizeof(fft_t0), &fft_t0, nullptr));
        CL_CHECK(clGetEventProfilingInfo(fft_end_raw,   CL_PROFILING_COMMAND_START,
                                         sizeof(fft_t1), &fft_t1, nullptr));
        CL_CHECK(clReleaseEvent(fft_start_raw));
        CL_CHECK(clReleaseEvent(fft_end_raw));
        double fft_ms = (fft_t1 >= fft_t0)
            ? static_cast<double>(fft_t1 - fft_t0) / 1e6
            : 0.0;

        // 9. Magnitude kernel
        const std::string kernel_path =
            (get_binary_dir() / "kernels" / "magnitude.cl").string();
        cl::Program mag_prog = build_program(ocl.context, ocl.device, kernel_path);
        cl::Kernel  mag_kernel(mag_prog, "magnitude");

        const size_t mag_buffer_bytes = static_cast<size_t>(total_bins) * sizeof(float);
        cl::Buffer   mag_buf(ocl.context, CL_MEM_WRITE_ONLY, mag_buffer_bytes);

        cl_int mag_fft_size   = fft_size;
        cl_int mag_num_bins   = num_bins;
        if (total_bins > static_cast<size_t>(std::numeric_limits<cl_int>::max()))
            throw std::runtime_error("total_bins exceeds cl_int range");
        cl_int mag_total_bins = static_cast<cl_int>(total_bins);

        CL_CHECK(mag_kernel.setArg(0, cl_buf));
        CL_CHECK(mag_kernel.setArg(1, mag_buf));
        CL_CHECK(mag_kernel.setArg(2, mag_fft_size));
        CL_CHECK(mag_kernel.setArg(3, mag_num_bins));
        CL_CHECK(mag_kernel.setArg(4, mag_total_bins));

        cl::Event mag_event;
        CL_CHECK(prof_queue.enqueueNDRangeKernel(
            mag_kernel,
            cl::NullRange,
            cl::NDRange(static_cast<size_t>(total_bins)),
            cl::NullRange,
            nullptr,
            &mag_event));
        CL_CHECK(prof_queue.finish());

        double mag_ms = duration_ms(mag_event);

        // 10. Read magnitudes back to host
        std::vector<float> magnitudes(static_cast<size_t>(total_bins));
        CL_CHECK(prof_queue.enqueueReadBuffer(
            mag_buf, CL_TRUE, 0,
            mag_buffer_bytes,
            magnitudes.data()));

        deleteVkFFT(&vkfft_app);

        // 11. CPU FFTW reference (compiled in only when FFTW3 is available)
#ifdef HAVE_FFTW3
        double cpu_ms = 0.0;
        {
            // Allocate FFTW in/out buffers for one frame at a time.
            fftwf_complex* fftwf_in  = fftwf_alloc_complex(fft_size);
            fftwf_complex* fftwf_out = fftwf_alloc_complex(fft_size);
            fftwf_plan plan = fftwf_plan_dft_1d(
                fft_size, fftwf_in, fftwf_out, FFTW_FORWARD, FFTW_ESTIMATE);

            auto t_cpu_start = std::chrono::steady_clock::now();

            for (int f_idx = 0; f_idx < num_frames; ++f_idx) {
                for (int b = 0; b < fft_size; ++b) {
                    size_t pcm_idx = static_cast<size_t>(f_idx) * hop_size + b;
                    float  s       = (pcm_idx < pcm.size()) ? pcm[pcm_idx] : 0.0f;
                    fftwf_in[b][0] = s;
                    fftwf_in[b][1] = 0.0f;
                }
                fftwf_execute(plan);
            }

            auto t_cpu_end = std::chrono::steady_clock::now();
            cpu_ms = std::chrono::duration<double, std::milli>(
                t_cpu_end - t_cpu_start).count();

            fftwf_destroy_plan(plan);
            fftwf_free(fftwf_in);
            fftwf_free(fftwf_out);
        }
        double speedup = (fft_ms > 0.0) ? (cpu_ms / fft_ms) : 0.0;
#endif

        // 12. Console output
        std::cout << std::fixed << std::setprecision(3);
        std::cout << "GPU FFT batch:       " << fft_ms  << " ms  (CL event)\n";
        std::cout << "GPU magnitude kernel: " << mag_ms << " ms\n";

#ifdef HAVE_FFTW3
        std::cout << "CPU FFTW batch:      " << cpu_ms  << " ms\n";
        std::cout << "Speedup (CPU/GPU):   " << std::setprecision(1) << speedup << "x\n";
        if (speedup < 10.0)
            std::cout << "  [NOTE: speedup below 10x gate — report actual hardware]\n";
#else
        std::cout << "[CPU reference skipped: FFTW3 not found]\n";
#endif

        // 13. BMP output — frequency-vs-time heatmap
        // Log-compress to emphasise quiet spectral content alongside loud peaks.
        std::vector<float> log_mags(static_cast<size_t>(total_bins));
        for (size_t i = 0; i < total_bins; ++i)
            log_mags[i] = std::log10(1.0f + magnitudes[i]);

        float min_val = *std::min_element(log_mags.begin(), log_mags.end());
        float max_val = *std::max_element(log_mags.begin(), log_mags.end());
        float range   = (max_val > min_val) ? (max_val - min_val) : 1.0f;

        // BMP dimensions: width = num_frames (time axis), height = num_bins (freq axis)
        // WHY vertical flip: BMP origin is bottom-left; low-frequency bins should appear
        // at the bottom of the image, so we write row (num_bins-1-row) from magnitudes.
        const int bmp_w = num_frames;
        const int bmp_h = num_bins;
        std::vector<uint8_t> pixels(static_cast<size_t>(bmp_w) * bmp_h * 4);

        for (int row = 0; row < bmp_h; ++row) {
            int mag_row = num_bins - 1 - row;  // flip: low freq at bottom of BMP
            for (int col = 0; col < bmp_w; ++col) {
                float t = (log_mags[static_cast<size_t>(col) * num_bins + mag_row] - min_val)
                          / range;
                auto rgba = jet_rgba(t);
                size_t px_idx = (static_cast<size_t>(row) * bmp_w + col) * 4;
                pixels[px_idx + 0] = rgba[0];
                pixels[px_idx + 1] = rgba[1];
                pixels[px_idx + 2] = rgba[2];
                pixels[px_idx + 3] = rgba[3];
            }
        }

        if (!stbi_write_bmp(output_path.c_str(), bmp_w, bmp_h, 4, pixels.data()))
            throw std::runtime_error("stbi_write_bmp failed: " + output_path);

        std::cout << "Output: " << output_path
                  << " (" << bmp_w << "x" << bmp_h << " px)\n";

    } catch (const cl::Error& e) {
        std::cerr << "OpenCL error: " << e.what() << " (" << e.err() << ")\n";
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
