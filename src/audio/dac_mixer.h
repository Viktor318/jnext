#pragma once

#include <cmath>
#include <cstdint>
#include "audio/audio_channels.h"
#include "audio/dac.h"

/// hungarian-ui - the DAC channel mixer: a volume, a place (-100 left .. +100
/// right) and a solo switch for each Soundrive channel A-D. A host listening aid with no hardware
/// analogue: the machine adds A + B to the left and C + D to the right
/// (soundrive.vhd:112-113), which is the default here. It sits beside the host
/// gains in Mixer and changes only the audible mix; the channel registers, the
/// snapshots and the debugger's source tap and stems never see it.
class DacMixer {
public:
    using Pan = DacPan;
    static constexpr int CHANNELS = DAC_MIX_CHANNELS;
    static constexpr int machine_pan(int ch) { return dac_machine_pan(ch); }

    DacMixer() { for (int ch = 0; ch < CHANNELS; ++ch) pan_[ch] = machine_pan(ch); refresh_(); }

    void set_gain_db(int ch, float db)
    {
        if (ch < 0 || ch >= CHANNELS) return;
        gain_db_[ch] = db;
        refresh_();
    }
    float gain_db(int ch) const { return ch >= 0 && ch < CHANNELS ? gain_db_[ch] : 0.0f; }

    void set_pan(int ch, int p)
    {
        if (ch < 0 || ch >= CHANNELS) return;
        pan_[ch] = DacPan::clamp(p);
        refresh_();
    }
    int pan(int ch) const { return ch >= 0 && ch < CHANNELS ? pan_[ch] : Pan::Centre; }

    /// Solo: while any channel is soloed, only the soloed ones are heard; none soloed, all are.
    void set_solo(int ch, bool on)
    {
        if (ch < 0 || ch >= CHANNELS) return;
        solo_[ch] = on;
        refresh_();
    }
    bool solo(int ch) const { return ch >= 0 && ch < CHANNELS && solo_[ch]; }

    /// False when every channel is as the machine has it: the Mixer then takes
    /// its own integer path and this class does no work.
    bool active() const { return active_; }

    /// The two sides' excursion from the DAC's rest level, in the 13-bit mix
    /// domain. The pan law is linear: a channel at p gives (100 - p) / 200 of itself
    /// to the left and (100 + p) / 200 to the right. A centred channel gives half of itself to each side, so the same
    /// byte on a left and a right channel (the 0xDF port writes A and D) is as
    /// loud centred as it is on the machine. Each channel swings around its own
    /// idle 0x80 (soundrive.vhd:74-77), so no volume adds a DC offset.
    struct Excursion { int32_t left, right; };   // returned in a register, so mix() keeps no address of its locals
    [[gnu::noinline, gnu::cold]] Excursion excursion(const Dac& dac, float group_gain) const
    {
        float l = 0.0f, r = 0.0f;
        for (int ch = 0; ch < CHANNELS; ++ch) {
            const float e = static_cast<float>((static_cast<int32_t>(dac.audible_channel(ch)) - 0x80) << 2);
            l += e * weight_l_[ch];
            r += e * weight_r_[ch];
        }
        return {static_cast<int32_t>(std::lround(l * group_gain)), static_cast<int32_t>(std::lround(r * group_gain))};
    }

private:
    void refresh_()
    {
        active_ = false;
        const bool any_solo = solo_[0] || solo_[1] || solo_[2] || solo_[3];
        for (int ch = 0; ch < CHANNELS; ++ch) {
            const float gain = any_solo && !solo_[ch] ? 0.0f : gain_db_[ch] == 0.0f ? 1.0f : std::pow(10.0f, gain_db_[ch] / 20.0f);
            // Exact at the three old places: 1 / 0, 0.5 / 0.5, 0 / 1.
            weight_l_[ch] = gain * static_cast<float>(100 - pan_[ch]) / 200.0f;
            weight_r_[ch] = gain * static_cast<float>(100 + pan_[ch]) / 200.0f;
            if (gain != 1.0f || pan_[ch] != machine_pan(ch)) active_ = true;
        }
    }

    float gain_db_[CHANNELS]{};
    int   pan_[CHANNELS]{};
    bool  solo_[CHANNELS]{};
    float weight_l_[CHANNELS]{};
    float weight_r_[CHANNELS]{};
    bool  active_ = false;
};
