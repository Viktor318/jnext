#pragma once

#include <cstdint>

/// hungarian-ui - the host's channel settings: volume and place of each DAC
/// channel (audio/dac_mixer.h) and the AY stereo override
/// (TurboSound::set_stereo_override). One value that travels whole through the
/// configuration, EmulatorConfig and the cold boot; the default is the machine.

enum class DacPan : uint8_t { Left, Centre, Right };            // the words of audio.dac_X_pan, in order
enum class AyStereoOrder : uint8_t { Machine, Abc, Acb };       // the words of audio.ay_stereo, in order

constexpr int DAC_MIX_CHANNELS = 4;   // A, B, C, D

/// The machine's own DAC routing (soundrive.vhd:112-113).
constexpr DacPan dac_machine_pan(int ch) { return ch < 2 ? DacPan::Left : DacPan::Right; }

struct AudioChannels {
    float         dac_gain_db[DAC_MIX_CHANNELS] = {0.0f, 0.0f, 0.0f, 0.0f};
    DacPan        dac_pan[DAC_MIX_CHANNELS] = {dac_machine_pan(0), dac_machine_pan(1), dac_machine_pan(2), dac_machine_pan(3)};
    AyStereoOrder ay_stereo = AyStereoOrder::Machine;
    bool          ay_mono[3] = {false, false, false};

    bool operator==(const AudioChannels& o) const
    {
        for (int ch = 0; ch < DAC_MIX_CHANNELS; ++ch)
            if (dac_gain_db[ch] != o.dac_gain_db[ch] || dac_pan[ch] != o.dac_pan[ch]) return false;
        for (int chip = 0; chip < 3; ++chip)
            if (ay_mono[chip] != o.ay_mono[chip]) return false;
        return ay_stereo == o.ay_stereo;
    }
    bool operator!=(const AudioChannels& o) const { return !(*this == o); }
};

/// One AY chip's layout as the Preferences dialog shows it: as the machine has
/// it, and as it is heard with the override.
struct AyChipLayout {
    bool sounds = false;      ///< in the mix: selected or TurboSound on, not held in reset, not panned off
    bool left = true, right = true;
    bool acb = false, mono = false;
};
struct AyLayouts {
    AyChipLayout machine[3];
    AyChipLayout heard[3];
};
