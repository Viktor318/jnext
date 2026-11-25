#pragma once

#include <cmath>
#include <cstdint>
#include "audio/dac.h"

/// hungarian-ui - the DAC channel mixer: a volume and a place (left, centre or
/// right) for each Soundrive channel A-D. A host listening aid with no hardware
/// analogue: the machine adds A + B to the left and C + D to the right
/// (soundrive.vhd:112-113), which is the default here. It sits beside the host
/// gains in Mixer and changes only the audible mix; the channel registers, the
/// snapshots and the debugger's source tap and stems never see it.
class DacMixer {
public:
    enum class Pan : uint8_t { Left, Centre, Right };
    static constexpr int CHANNELS = 4;   // A, B, C, D

    /// The machine's own routing (soundrive.vhd:112-113).
    static constexpr Pan machine_pan(int ch) { return ch < 2 ? Pan::Left : Pan::Right; }

    DacMixer() { for (int ch = 0; ch < CHANNELS; ++ch) pan_[ch] = machine_pan(ch); refresh_(); }

    void set_gain_db(int ch, float db)
    {
        if (ch < 0 || ch >= CHANNELS) return;
        gain_db_[ch] = db;
        refresh_();
    }
    float gain_db(int ch) const { return ch >= 0 && ch < CHANNELS ? gain_db_[ch] : 0.0f; }

    void set_pan(int ch, Pan p)
    {
        if (ch < 0 || ch >= CHANNELS) return;
        pan_[ch] = p;
        refresh_();
    }
    Pan pan(int ch) const { return ch >= 0 && ch < CHANNELS ? pan_[ch] : Pan::Centre; }

    /// False when every channel is as the machine has it: the Mixer then takes
    /// its own integer path and this class does no work.
    bool active() const { return active_; }

    /// The two sides' excursion from the DAC's rest level, in the 13-bit mix
    /// domain. A centred channel gives half of itself to each side, so the same
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
        for (int ch = 0; ch < CHANNELS; ++ch) {
            const float gain = gain_db_[ch] == 0.0f ? 1.0f : std::pow(10.0f, gain_db_[ch] / 20.0f);
            weight_l_[ch] = pan_[ch] == Pan::Left ? gain : pan_[ch] == Pan::Centre ? gain * 0.5f : 0.0f;
            weight_r_[ch] = pan_[ch] == Pan::Right ? gain : pan_[ch] == Pan::Centre ? gain * 0.5f : 0.0f;
            if (gain != 1.0f || pan_[ch] != machine_pan(ch)) active_ = true;
        }
    }

    float gain_db_[CHANNELS]{};
    Pan   pan_[CHANNELS]{};
    float weight_l_[CHANNELS]{};
    float weight_r_[CHANNELS]{};
    bool  active_ = false;
};
