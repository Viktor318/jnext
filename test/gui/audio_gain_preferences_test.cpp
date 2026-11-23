#include "core/emulator.h"
#include "core/emulator_config.h"
#include "gui/app_config.h"
#include "gui/main_window.h"
#include "debug/debugger.h"
#include "gui/preferences_dialog.h"

#include <QApplication>
#include <QTemporaryDir>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QSlider>
#include <QCheckBox>

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

QSlider* slider(PreferencesDialog& dialog, const char* name)
{
    return dialog.findChild<QSlider*>(QString::fromLatin1(name));
}

} // namespace

int main(int argc, char** argv)
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    // MainWindow reads (and may write) jnext.conf: keep it off the user's real
    // ~/.jnext, whether or not the harness has already isolated $HOME.
    QTemporaryDir cfg_dir;
    if (!cfg_dir.isValid()) {
        std::printf("  FAIL: could not create a temporary config directory\n");
        std::printf("Total:    0  Passed:    0  Failed:    1  Skipped:    0\n");
        return 1;
    }
    qputenv("JNEXT_CONFIG_DIR", cfg_dir.path().toUtf8());
    QApplication app(argc, argv);

    AppConfigData initial;
    initial.audio_gain_db = 1.0f;
    initial.audio_gain_beeper_db = 2.0f;
    initial.audio_gain_ay_db[0] = 3.0f;
    initial.audio_gain_ay_db[1] = 4.0f;
    initial.audio_gain_ay_db[2] = 5.0f;
    initial.audio_gain_dac_db = 6.0f;
    PreferencesDialog dialog(initial);

    QSlider* controls[6] = {
        slider(dialog, "audioGainDbSlider"),
        slider(dialog, "audioGainBeeperDbSlider"),
        slider(dialog, "audioGainAy0DbSlider"),
        slider(dialog, "audioGainAy1DbSlider"),
        slider(dialog, "audioGainAy2DbSlider"),
        slider(dialog, "audioGainDacDbSlider")
    };
    bool complete = true;
    for (QSlider* control : controls)
        complete = complete && control &&
                   control->minimum() == -24 && control->maximum() == 24;
    check("SGP-01", complete);

    for (int i = 0; i < 6; ++i)
        check(i == 0 ? "SGP-02" : i == 1 ? "SGP-03" : i == 2 ? "SGP-04" :
              i == 3 ? "SGP-05" : i == 4 ? "SGP-06" : "SGP-07",
              controls[i] && controls[i]->value() == i + 1);

    AppConfigData collected;
    bool emitted = false;
    QObject::connect(&dialog, &PreferencesDialog::apply_requested,
                     [&collected, &emitted](const AppConfigData& cfg) {
                         collected = cfg;
                         emitted = true;
                     });
    for (int i = 0; i < 6; ++i) controls[i]->setValue(-(i + 1));
    auto* buttons = dialog.findChild<QDialogButtonBox*>();
    buttons->button(QDialogButtonBox::Apply)->click();
    check("SGP-08", emitted &&
                    collected.audio_gain_db == -1.0f &&
                    collected.audio_gain_beeper_db == -2.0f &&
                    collected.audio_gain_ay_db[0] == -3.0f &&
                    collected.audio_gain_ay_db[1] == -4.0f &&
                    collected.audio_gain_ay_db[2] == -5.0f &&
                    collected.audio_gain_dac_db == -6.0f);

    // hungarian-ui: the oversampling checkbox - off as given, collected, applied live.
    {
        auto* box = dialog.findChild<QCheckBox*>(QStringLiteral("audioOversampleCheck"));
        const bool off = box && !box->isChecked() && !collected.audio_oversample;
        if (box) box->setChecked(true);
        buttons->button(QDialogButtonBox::Apply)->click();
        check("SGP-HU-01", off && collected.audio_oversample);
        PreferencesDialog again(collected);
        auto* box2 = again.findChild<QCheckBox*>(QStringLiteral("audioOversampleCheck"));
        check("SGP-HU-02", box2 && box2->isChecked());
    }

    Emulator emulator;
    EmulatorConfig config;
    config.type = MachineType::ZX48K;
    check("SGP-09", emulator.init(config));
    // GH #278 WP2 — the loop owner's debugger backend, which set_emulator()
    // requires in a debugger build; declared before the window, so it outlives it.
    jnext::dbg::Debugger backend(emulator);
    MainWindow window;
    window.set_debugger(&backend);
    window.set_emulator(&emulator);
    window.apply_startup_config(collected);
    check("SGP-10", emulator.mixer().output_gain_db() == -1.0f &&
                    emulator.mixer().beeper_gain_db() == -2.0f &&
                    emulator.mixer().ay_gain_db(0) == -3.0f &&
                    emulator.mixer().ay_gain_db(1) == -4.0f &&
                    emulator.mixer().ay_gain_db(2) == -5.0f &&
                    emulator.mixer().dac_gain_db() == -6.0f);

    const bool live_on = emulator.mixer().oversample();
    collected.audio_oversample = false;
    window.apply_startup_config(collected);
    check("SGP-HU-03", live_on && !emulator.mixer().oversample());

    std::printf("Total: %d Passed: %d Failed: %d Skipped: 0\n",
                passed + failed, passed, failed);
    return failed == 0 ? 0 : 1;
}
