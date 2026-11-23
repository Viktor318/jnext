#pragma once

#include <cstdint>

/// hungarian-ui - 4x oversampling of the mixer's output (audio.oversample).
///
/// The mixer averages the 13-bit mix over each 44.1 kHz output sample. That box
/// filter is a weak anti-alias filter for a stepped DAC signal: the images above
/// 22 kHz are only partly removed and fold back as tones that are not in the
/// music, and the top octave is attenuated (about 2 dB at 16 kHz). With
/// oversampling on, the same averaging is done on four equal parts of the output
/// sample (176.4 kHz) and a linear-phase FIR low-pass brings it back to 44.1 kHz.
///
/// A host listening aid: no hardware analogue, not machine state, never
/// serialised, and nothing the Z80 can see. Off by default; the mixer's own path
/// is untouched then.
class Oversampler {
public:
    static constexpr int FACTOR = 4;
    static constexpr int TAPS   = 256;   ///< at 176.4 kHz; delay TAPS / 2 (0.73 ms)

    Oversampler();

    /// Forget the past: the next output is filtered from silence.
    void reset();

    /// The mix levels `l` / `r` held for `cycles` master cycles. Within one output
    /// sample the parts are filled in order; the fourth takes what is left, so the
    /// 634 / 635-cycle samples of the 28 MHz clock need no bookkeeping here.
    void accumulate(int32_t l, int32_t r, uint32_t cycles)
    {
        while (cycles != 0) {
            const uint32_t room = kPart[part_] - held_[part_];
            const uint32_t n = cycles < room ? cycles : room;
            sum_l_[part_] += static_cast<int64_t>(l) * n;
            sum_r_[part_] += static_cast<int64_t>(r) * n;
            held_[part_] += n;
            cycles -= n;
            if (held_[part_] == kPart[part_]) ++part_;
        }
    }

    /// Close the output sample (not `emit`: a Qt macro): the filtered mix, relative to `rest` (the mix's
    /// silence), times `scale`.
    void finish(int32_t rest, float scale, float& l, float& r);

private:
    /// 28 MHz / 176.4 kHz = 158.73 cycles per part; the last part is open-ended.
    static constexpr uint32_t kPart[FACTOR] = {159, 159, 158, UINT32_MAX};

    int64_t  sum_l_[FACTOR];
    int64_t  sum_r_[FACTOR];
    uint32_t held_[FACTOR];
    int      part_;

    // Each history is written twice, TAPS apart, so the newest TAPS values are
    // always contiguous.
    float hist_l_[TAPS * 2];
    float hist_r_[TAPS * 2];
    int   pos_;
    float last_l_, last_r_;   ///< the level of an empty part: what the previous one had

    float fir_[TAPS];
};
