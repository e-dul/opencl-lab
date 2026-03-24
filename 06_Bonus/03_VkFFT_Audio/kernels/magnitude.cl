/*
 * magnitude.cl — compute magnitude of positive spectrum bins.
 *
 * After vkFFT forward transform, the buffer holds interleaved complex pairs:
 *   [re0, im0, re1, im1, ..., re(N-1), im(N-1)]  per frame.
 *
 * WHY only fft_size/2 bins: a real-valued input signal has a Hermitian-symmetric
 * spectrum, so bins [fft_size/2 .. fft_size-1] are conjugates of [0 .. fft_size/2-1].
 * Discarding the redundant half halves the output buffer and the BMP height.
 */

__kernel void magnitude(
    __global const float* input,   /* complex interleaved, num_frames * fft_size * 2 floats */
    __global float*       output,  /* positive-spectrum magnitudes, num_frames * (fft_size/2) floats */
    int fft_size,
    int num_bins,    /* fft_size / 2 — bins per frame in the output */
    int total_bins   /* num_frames * num_bins — total output elements */
) {
    size_t gid = get_global_id(0);
    /* Guard excess work-items from NDRange rounding. */
    if (gid >= (size_t)total_bins) return;

    /* Recover frame and bin from flat output index. */
    size_t frame = gid / (size_t)num_bins;
    size_t bin   = gid % (size_t)num_bins;

    /* WHY frame * fft_size * 2: each frame occupies fft_size complex pairs (2 floats each). */
    size_t complex_idx = frame * (size_t)fft_size * 2 + bin * 2;

    float re = input[complex_idx];
    float im = input[complex_idx + 1];

    output[gid] = sqrt(re * re + im * im);
}
