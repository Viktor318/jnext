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
#include <QComboBox>
#include <QElapsedTimer>
#include <QLabel>
#include <QRadioButton>
#include <QTabWidget>
#include <QTextDocument>

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

    // hungarian-ui: Audio > DAC channels and AY channels.
    {
        AppConfigData cfg;
        PreferencesDialog dlg(cfg);
        auto* layout = dlg.findChild<QComboBox*>(QStringLiteral("dacLayoutCombo"));
        auto* order = dlg.findChild<QComboBox*>(QStringLiteral("ayStereoCombo"));
        auto* apply = dlg.findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Apply);
        const auto radio = [&dlg](const char* name) { return dlg.findChild<QRadioButton*>(QString::fromLatin1(name)); };
        const auto places = [&radio]() {
            QString s;
            for (const char* ch : {"A", "B", "C", "D"})
                for (const char* p : {"Left", "Centre", "Right"}) {
                    auto* b = radio((std::string("dac") + ch + p + "Radio").c_str());
                    if (b && b->isChecked()) s += QLatin1Char(p[0]);
                }
            return s;
        };
        const auto pick = [layout](int i) { layout->setCurrentIndex(i); };
        bool sliders = true;
        for (const char* n : {"audioDacAGainDbSlider", "audioDacBGainDbSlider", "audioDacCGainDbSlider", "audioDacDGainDbSlider"}) {
            QSlider* s = slider(dlg, n);
            sliders = sliders && s && s->minimum() == -24 && s->maximum() == 24 && s->value() == 0;
        }
        check("SGP-HU-04", sliders && layout && order && layout->currentIndex() == 0 && order->currentIndex() == 0 &&
                               places() == QStringLiteral("LLRR") && !apply->isEnabled());

        // The list sets the four places and follows them; "Custom" is not a layout to pick.
        pick(1);
        const bool lcr = places() == QStringLiteral("LCCR") && layout->currentIndex() == 1 && apply->isEnabled();
        pick(2);
        const bool mono = places() == QStringLiteral("CCCC") && layout->currentIndex() == 2;
        pick(0);
        const bool machine = places() == QStringLiteral("LLRR") && !apply->isEnabled();
        pick(3);
        const bool snap = places() == QStringLiteral("LLRR") && layout->currentIndex() == 0 && !apply->isEnabled();
        radio("dacBRightRadio")->setChecked(true);
        const bool custom = places() == QStringLiteral("LRRR") && layout->currentIndex() == 3 && apply->isEnabled();
        radio("dacBLeftRadio")->setChecked(true);
        check("SGP-HU-05", lcr && mono && machine && snap && custom && layout->currentIndex() == 0 && !apply->isEnabled());

        // Everything is collected, shown again and applied live; Apply greys out again.
        AppConfigData got;
        QObject::connect(&dlg, &PreferencesDialog::apply_requested, [&got](const AppConfigData& c) { got = c; });
        pick(1);
        slider(dlg, "audioDacAGainDbSlider")->setValue(-3);
        slider(dlg, "audioDacDGainDbSlider")->setValue(4);
        order->setCurrentIndex(2);
        dlg.findChild<QCheckBox*>(QStringLiteral("ay1MonoCheck"))->setChecked(true);
        apply->click();
        AudioChannels want;
        want.dac_gain_db[0] = -3.0f;
        want.dac_gain_db[3] = 4.0f;
        want.dac_pan[1] = want.dac_pan[2] = DacPan::Centre;
        want.ay_stereo = AyStereoOrder::Acb;
        want.ay_mono[1] = true;
        const bool collected_ok = got.audio_channels == want && !apply->isEnabled();
        PreferencesDialog again(got);
        AppConfigData got2;
        QObject::connect(&again, &PreferencesDialog::apply_requested, [&got2](const AppConfigData& c) { got2 = c; });
        again.findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->click();
        check("SGP-HU-06", collected_ok && got2.audio_channels == want &&
                               again.findChild<QComboBox*>(QStringLiteral("dacLayoutCombo"))->currentIndex() == 1);
        window.apply_startup_config(got);
        check("SGP-HU-07", emulator.audio_channels() == want);

        // The two rows: what the machine has, and what is heard.
        const auto text = [&dlg](const char* name) {
            QTextDocument doc;
            auto* l = dlg.findChild<QLabel*>(QString::fromLatin1(name));
            doc.setHtml(l ? l->text() : QString());
            return doc.toPlainText();
        };
        const bool dash = text("ayMachine0Label") == QStringLiteral("AY #0: -") && text("ayHeard2Label") == QStringLiteral("AY #2: -");
        TurboSound& ts = emulator.turbosound();
        ts.set_enabled(true);
        ts.set_stereo_mode(false);
        ts.set_mono_mode(0x04);
        ts.reg_addr(0xDE);   // chip 1: left only
        dlg.set_ay_layout_source([&emulator]() { return emulator.ay_layouts(); });
        const bool rows = text("ayMachine0Label") == QStringLiteral("AY #0: ABC") &&
                          text("ayMachine1Label") == QStringLiteral("AY #1: ABC, left only") &&
                          text("ayMachine2Label") == QStringLiteral("AY #2: mono") &&
                          text("ayHeard0Label") == QStringLiteral("AY #0: ACB") &&
                          text("ayHeard1Label") == QStringLiteral("AY #1: mono, left only") &&
                          text("ayHeard2Label") == QStringLiteral("AY #2: mono");
        ts.reg_addr(0xBE);   // chip 1: right only; the timer brings it
        ts.set_enabled(false);
        QElapsedTimer clock;
        clock.start();
        while (clock.elapsed() < 5000 && text("ayMachine0Label") != QStringLiteral("AY #0: silent"))
            QApplication::processEvents(QEventLoop::AllEvents, 50);
        check("SGP-HU-08", dash && rows && text("ayMachine0Label") == QStringLiteral("AY #0: silent") &&
                               text("ayHeard1Label") == QStringLiteral("AY #1: mono, right only") && !apply->isEnabled());

        // The lists are no wider than their text needs.
        dlg.resize(1400, dlg.height());
        dlg.show();
        QApplication::processEvents();
        check("SGP-HU-09", layout->width() <= layout->sizeHint().width() && order->width() <= order->sizeHint().width());

        // The Audio tab asks for no more height than it needs: a larger hint than minimum let Windows
        // grow the dialog by a few empty lines when it was moved. Opened, then laid out again as a
        // move does it: the same height. The note and the checkbox start at the tab's left edge.
        auto* tabs = dlg.findChild<QTabWidget*>();
        QWidget* note = dlg.findChild<QWidget*>(QStringLiteral("audioNoteLabel"));
        QWidget* audio_tab = note->parentWidget();
        tabs->setCurrentWidget(audio_tab);
        dlg.resize(dlg.sizeHint());
        QApplication::processEvents();
        const int opened = dlg.height();
        dlg.adjustSize();
        QApplication::processEvents();
        const int moved = dlg.height();
        dlg.resize(dlg.sizeHint());
        QApplication::processEvents();
        auto* box = dlg.findChild<QWidget*>(QStringLiteral("audioOversampleCheck"));
        const int left = audio_tab->findChild<QWidget*>(QStringLiteral("audioGainDbSlider"))->parentWidget()->x();
        check("SGP-HU-10", opened == moved && dlg.height() == opened && note->x() < left && box->x() < left &&
                               note->height() < note->fontMetrics().height() * 4);
    }

    std::printf("Total: %d Passed: %d Failed: %d Skipped: 0\n",
                passed + failed, passed, failed);
    return failed == 0 ? 0 : 1;
}
