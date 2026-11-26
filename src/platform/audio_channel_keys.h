#pragma once

#include <cstdlib>
#include <string>
#include <vector>

#include "audio/audio_channels.h"
#include "platform/config_keys.h"

/// hungarian-ui - the audio.dac_* / audio.ay_* settings as given with --set, applied
/// in order over `c`. The values are the validated canonical text config_keys gives.
inline void apply_audio_channel_keys(AudioChannels& c, const std::vector<config_keys::Assignment>& list)
{
    using config_keys::KeyId;
    const auto at = [](KeyId id, KeyId first) { return static_cast<int>(id) - static_cast<int>(first); };
    for (const config_keys::Assignment& a : list) {
        const int word = config_keys::word_index(config_keys::info(a.id).values, a.text);
        if (a.id >= KeyId::AudioDacAGainDb && a.id <= KeyId::AudioDacDGainDb)
            c.dac_gain_db[at(a.id, KeyId::AudioDacAGainDb)] = static_cast<float>(std::strtod(a.text.c_str(), nullptr));
        else if (a.id >= KeyId::AudioDacAPan && a.id <= KeyId::AudioDacDPan)
            c.dac_pan[at(a.id, KeyId::AudioDacAPan)] = DacPan::clamp(static_cast<int>(std::strtol(a.text.c_str(), nullptr, 10)));
        else if (a.id >= KeyId::AudioDacAMute && a.id <= KeyId::AudioDacDMute)
            c.dac_mute[at(a.id, KeyId::AudioDacAMute)] = a.text == "true";
        else if (a.id == KeyId::AudioAyStereo && word >= 0)
            c.ay_stereo = static_cast<AyStereoOrder>(word);
        else if (a.id >= KeyId::AudioAy0Mono && a.id <= KeyId::AudioAy2Mono)
            c.ay_mono[at(a.id, KeyId::AudioAy0Mono)] = a.text == "true";
    }
}
