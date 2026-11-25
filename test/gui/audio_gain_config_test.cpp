#include "gui/app_config.h"

#include <QCoreApplication>
#include <QSettings>
#include <QTemporaryDir>

#include <cmath>
#include <cstdio>
#include "../row_id.h"

namespace {

int passed = 0;
int failed = 0;

void check(const char* id, bool condition)
{
    report_row_id(id);
    if (condition) {
        ++passed;
    } else {
        ++failed;
        std::printf("FAIL %s\n", id);
    }
}

bool close(float a, float b)
{
    return std::abs(a - b) < 0.001f;
}

} // namespace

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    QTemporaryDir dir;
    check("SGC-01", dir.isValid());

    const QString path = dir.filePath(QStringLiteral("jnext.conf"));
    AppConfig defaults(path);
    defaults.load();
    const auto& d = defaults.data();
    check("SGC-02", d.audio_gain_beeper_db == 0.0f &&
                    d.audio_gain_ay_db[0] == 0.0f &&
                    d.audio_gain_ay_db[1] == 0.0f &&
                    d.audio_gain_ay_db[2] == 0.0f &&
                    d.audio_gain_dac_db == 0.0f);

    AppConfig writer(path);
    writer.data().audio_gain_db = -1.0f;
    writer.data().audio_gain_beeper_db = -2.0f;
    writer.data().audio_gain_ay_db[0] = 3.0f;
    writer.data().audio_gain_ay_db[1] = -4.0f;
    writer.data().audio_gain_ay_db[2] = 5.0f;
    writer.data().audio_gain_dac_db = -6.0f;
    writer.save();

    {
        QSettings raw(path, QSettings::IniFormat);
        raw.beginGroup(QStringLiteral("audio"));
        check("SGC-11", close(raw.value(QStringLiteral("gain_db")).toFloat(), -1.0f));
        check("SGC-12", close(raw.value(QStringLiteral("gain_beeper_db")).toFloat(), -2.0f));
        check("SGC-13", close(raw.value(QStringLiteral("gain_ay0_db")).toFloat(), 3.0f));
        check("SGC-14", close(raw.value(QStringLiteral("gain_ay1_db")).toFloat(), -4.0f));
        check("SGC-15", close(raw.value(QStringLiteral("gain_ay2_db")).toFloat(), 5.0f));
        check("SGC-16", close(raw.value(QStringLiteral("gain_dac_db")).toFloat(), -6.0f));
        raw.endGroup();
    }

    AppConfig reader(path);
    reader.load();
    check("SGC-03", close(reader.data().audio_gain_beeper_db, -2.0f));
    check("SGC-04", close(reader.data().audio_gain_ay_db[0], 3.0f));
    check("SGC-05", close(reader.data().audio_gain_ay_db[1], -4.0f));
    check("SGC-06", close(reader.data().audio_gain_ay_db[2], 5.0f));
    check("SGC-07", close(reader.data().audio_gain_dac_db, -6.0f));

    const QString invalid_path = dir.filePath(QStringLiteral("invalid.conf"));
    {
        QSettings raw(invalid_path, QSettings::IniFormat);
        raw.beginGroup(QStringLiteral("audio"));
        raw.setValue(QStringLiteral("gain_beeper_db"), 24.1);
        raw.setValue(QStringLiteral("gain_ay0_db"), -24.1);
        raw.setValue(QStringLiteral("gain_ay1_db"), QStringLiteral("loud"));
        raw.setValue(QStringLiteral("gain_ay2_db"), 25.0);
        raw.setValue(QStringLiteral("gain_dac_db"), -25.0);
        raw.endGroup();
    }
    AppConfig invalid(invalid_path);
    invalid.load();
    check("SGC-08", invalid.data().audio_gain_beeper_db == 0.0f &&
                    invalid.data().audio_gain_ay_db[0] == 0.0f &&
                    invalid.data().audio_gain_ay_db[1] == 0.0f &&
                    invalid.data().audio_gain_ay_db[2] == 0.0f &&
                    invalid.data().audio_gain_dac_db == 0.0f);

    check("SGC-09", merge_cli_precedence(true, 6.0f, -12.0f) == 6.0f);
    check("SGC-10", merge_cli_precedence(false, 6.0f, -12.0f) == -12.0f);

    const QString valid_path = dir.filePath(QStringLiteral("valid-raw.conf"));
    {
        QSettings raw(valid_path, QSettings::IniFormat);
        raw.beginGroup(QStringLiteral("audio"));
        raw.setValue(QStringLiteral("gain_db"), 1.0);
        raw.setValue(QStringLiteral("gain_beeper_db"), 2.0);
        raw.setValue(QStringLiteral("gain_ay0_db"), 3.0);
        raw.setValue(QStringLiteral("gain_ay1_db"), 4.0);
        raw.setValue(QStringLiteral("gain_ay2_db"), 5.0);
        raw.setValue(QStringLiteral("gain_dac_db"), 6.0);
        raw.endGroup();
    }
    AppConfig valid(valid_path);
    valid.load();
    check("SGC-17", close(valid.data().audio_gain_db, 1.0f));
    check("SGC-18", close(valid.data().audio_gain_beeper_db, 2.0f));
    check("SGC-19", close(valid.data().audio_gain_ay_db[0], 3.0f));
    check("SGC-20", close(valid.data().audio_gain_ay_db[1], 4.0f));
    check("SGC-21", close(valid.data().audio_gain_ay_db[2], 5.0f));
    check("SGC-22", close(valid.data().audio_gain_dac_db, 6.0f));

    // hungarian-ui: audio.oversample, a Bool, default off.
    check("SGC-HU-01", !AppConfigData().audio_oversample && !d.audio_oversample &&
                       !valid.data().audio_oversample);
    {
        const QString os_path = dir.filePath(QStringLiteral("oversample.conf"));
        AppConfig w(os_path);
        w.data().audio_oversample = true;
        w.save();
        QSettings raw(os_path, QSettings::IniFormat);
        AppConfig r(os_path);
        r.load();
        check("SGC-HU-02", raw.value(QStringLiteral("audio/oversample")).toString() == QStringLiteral("true") &&
                           r.data().audio_oversample);
    }

    // hungarian-ui: the DAC channel mixer and the AY stereo override.
    check("SGC-HU-03", d.audio_channels == AudioChannels() && valid.data().audio_channels == AudioChannels());
    {
        const QString ch_path = dir.filePath(QStringLiteral("channels.conf"));
        AudioChannels c;
        const DacPan pans[4] = {DacPan::Right, DacPan::Centre, DacPan::Left, DacPan::Centre};
        for (int ch = 0; ch < 4; ++ch) {
            c.dac_gain_db[ch] = static_cast<float>(ch + 1) * -2.0f;
            c.dac_pan[ch] = pans[ch];
        }
        c.ay_stereo = AyStereoOrder::Acb;
        c.ay_mono[1] = true;
        AppConfig w(ch_path);
        w.data().audio_channels = c;
        w.save();
        QSettings raw(ch_path, QSettings::IniFormat);
        raw.beginGroup(QStringLiteral("audio"));
        const auto is = [&raw](const char* key, const char* text) {
            return raw.value(QString::fromLatin1(key)).toString() == QString::fromLatin1(text);
        };
        const bool spelled =
            is("dac_a_gain_db", "-2") && is("dac_b_gain_db", "-4") && is("dac_c_gain_db", "-6") && is("dac_d_gain_db", "-8") &&
            is("dac_a_pan", "right") && is("dac_b_pan", "centre") && is("dac_c_pan", "left") && is("dac_d_pan", "centre") &&
            is("ay_stereo", "acb") && is("ay0_mono", "false") && is("ay1_mono", "true") && is("ay2_mono", "false");
        AppConfig r(ch_path);
        r.load();
        check("SGC-HU-04", spelled && r.data().audio_channels == c);
    }
    {
        const QString bad_path = dir.filePath(QStringLiteral("channels-bad.conf"));
        {
            QSettings raw(bad_path, QSettings::IniFormat);
            raw.beginGroup(QStringLiteral("audio"));
            raw.setValue(QStringLiteral("dac_a_gain_db"), 25.0);
            raw.setValue(QStringLiteral("dac_d_gain_db"), QStringLiteral("loud"));
            raw.setValue(QStringLiteral("dac_b_pan"), QStringLiteral("middle"));
            raw.setValue(QStringLiteral("dac_c_pan"), QStringLiteral(" Centre "));
            raw.setValue(QStringLiteral("ay_stereo"), QStringLiteral("bca"));
            raw.endGroup();
        }
        AppConfig r(bad_path);
        r.load();
        AudioChannels want;
        want.dac_pan[2] = DacPan::Centre;
        check("SGC-HU-05", r.data().audio_channels == want);
    }

    std::printf("Total: %d Passed: %d Failed: %d Skipped: 0\n",
                passed + failed, passed, failed);
    return failed == 0 ? 0 : 1;
}
