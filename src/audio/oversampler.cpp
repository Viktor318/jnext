#include "audio/oversampler.h"

#include <cmath>

Oversampler::Oversampler()
{
    // Blackman-windowed sinc at 176.4 kHz, cut-off 20.5 kHz, unity gain at DC.
    // Even length: the response is centred half-way between two sub-samples,
    // which is where the centre of the four parts of an output sample is.
    constexpr double pi = 3.14159265358979323846;
    constexpr double fc = 20500.0 / (44100.0 * FACTOR);
    double h[TAPS];
    double sum = 0.0;
    for (int k = 0; k < TAPS; ++k) {
        const double u = k - TAPS / 2 + 0.5;
        const double w = 0.42 + 0.5 * std::cos(2.0 * pi * u / TAPS) + 0.08 * std::cos(4.0 * pi * u / TAPS);
        h[k] = std::sin(2.0 * pi * fc * u) / (pi * u) * w;
        sum += h[k];
    }
    for (int k = 0; k < TAPS; ++k) fir_[k] = static_cast<float>(h[k] / sum);
    reset();
}

void Oversampler::reset()
{
    for (int i = 0; i < FACTOR; ++i) {
        sum_l_[i] = sum_r_[i] = 0;
        held_[i] = 0;
    }
    part_ = 0;
    for (int i = 0; i < TAPS * 2; ++i) hist_l_[i] = hist_r_[i] = 0.0f;
    pos_ = 0;
    last_l_ = last_r_ = 0.0f;
}

void Oversampler::finish(int32_t rest, float scale, float& l, float& r)
{
    for (int i = 0; i < FACTOR; ++i) {
        if (held_[i] != 0) {
            const int64_t zero = static_cast<int64_t>(rest) * held_[i];
            const float   k    = scale / static_cast<float>(held_[i]);
            last_l_ = static_cast<float>(sum_l_[i] - zero) * k;
            last_r_ = static_cast<float>(sum_r_[i] - zero) * k;
        }
        hist_l_[pos_] = hist_l_[pos_ + TAPS] = last_l_;
        hist_r_[pos_] = hist_r_[pos_ + TAPS] = last_r_;
        pos_ = (pos_ + 1) % TAPS;
        sum_l_[i] = sum_r_[i] = 0;
        held_[i] = 0;
    }
    part_ = 0;

    // Eight partial sums, so the compiler may use vector registers without
    // being allowed to reorder floating-point additions.
    constexpr int LANES = 8;
    static_assert(TAPS % LANES == 0, "the FIR is summed in whole lanes");
    float al[LANES] = {}, ar[LANES] = {};
    const float* a = hist_l_ + pos_;
    const float* b = hist_r_ + pos_;
    for (int k = 0; k < TAPS; k += LANES) {
        for (int j = 0; j < LANES; ++j) {
            al[j] += a[k + j] * fir_[k + j];
            ar[j] += b[k + j] * fir_[k + j];
        }
    }
    l = r = 0.0f;
    for (int j = 0; j < LANES; ++j) {
        l += al[j];
        r += ar[j];
    }
}
