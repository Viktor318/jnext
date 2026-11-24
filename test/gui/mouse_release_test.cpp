// Captured-mouse release gesture, through the real MainWindow (GitHub #365).
//
// No VHDL oracle: host UI. The oracle is the design table of the issue plan:
//   * three exclusive methods; the default (ctrl-alt) is the historical
//     behaviour: Ctrl+Alt releases and is NOT consumed (Alt must still reach
//     Keyboard::alt_held_), the middle button stays a Kempston button, and Esc
//     is the ZX BREAK key;
//   * esc and middle-click are consumed whole: the press, and for Esc its
//     release, never reach the ZX keyboard / Kempston mouse;
//   * uncaptured, Esc is BREAK and any click on the viewport captures;
//   * every hint names the active gesture; Preferences > Input has the combo and
//     Apply changes the method live.
//
// Events go through QApplication::sendEvent on the real window; capture goes
// through the real "Capture &Mouse" action. Offscreen QPA, JNEXT_CONFIG_DIR in a
// temp dir (apply_preferences persists).
//
// Declared equivalent (not killable): forwarding the middle press to the
// dispatcher BEFORE the release in the same handler. set_mouse_captured(false)
// resets the dispatcher before any frame can read port 0xFADF.
//
// hungarian-ui rows MRQ-HU-01..04 (doc/hu/jnext-atadas.md, "Windowsos magyar
// billentyűzet"): with the host layout on, only the LEFT Alt releases (the right
// one is AltGr: AltGr + V types `@`); the window hands the layout module the
// event's physical key, classic scancode, character and dead-key flag; the
// Preferences fields; the switches reach the module.
//
// Run: ./build/test/mouse_release_test

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFocusEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPushButton>
#include <QStatusBar>
#include <QTemporaryDir>

#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "core/emulator.h"
#include "core/emulator_config.h"
#include "debug/debugger.h"
#include "gui/app_config.h"
#include "gui/main_window.h"
#include "gui/preferences_dialog.h"
#include "gui/ui_language.h"
#include "../row_id.h"

namespace {

int g_total = 0, g_pass = 0, g_fail = 0;

void check(const char* id, const char* desc, bool cond, const std::string& detail = {})
{
    report_row_id(id);
    ++g_total;
    if (cond) {
        ++g_pass;
    } else {
        ++g_fail;
        std::printf("  FAIL %s: %s", id, desc);
        if (!detail.empty()) std::printf(" [%s]", detail.c_str());
        std::printf("\n");
    }
}

using RM = pointer_capture::ReleaseMethod;

struct KeyEv { SDL_Scancode sc; bool pressed; };

struct Fixture {
    Emulator emu;
    std::unique_ptr<jnext::dbg::Debugger> backend;
    MainWindow win;
    std::vector<KeyEv> keys;
    QAction* capture = nullptr;

    explicit Fixture(RM method = RM::CtrlAlt)
    {
        EmulatorConfig cfg;
        cfg.type = MachineType::ZX48K;
        emu.init(cfg);
        backend = std::make_unique<jnext::dbg::Debugger>(emu);
        win.set_debugger(backend.get());
        win.set_emulator(&emu);
        win.set_key_callback([this](SDL_Scancode sc, bool pressed) { keys.push_back({sc, pressed}); });
        if (method != RM::CtrlAlt) {
            AppConfigData c;
            c.mouse_release = method;
            win.apply_startup_config(c);
        }
        for (QAction* a : win.findChildren<QAction*>())
            if (a->text() == QStringLiteral("Capture &Mouse")) capture = a;
    }

    bool captured() const { return capture && capture->isChecked(); }
    void capture_now() { if (capture && !capture->isChecked()) capture->trigger(); }

    void key(Qt::Key k, bool down, Qt::KeyboardModifiers m = Qt::NoModifier)
    {
        QKeyEvent ev(down ? QEvent::KeyPress : QEvent::KeyRelease, k, m);
        QApplication::sendEvent(&win, &ev);
    }
    // A key event with its native scan code and text, as a real keyboard sends it.
    void native_key(int k, bool down, Qt::KeyboardModifiers m, quint32 scan, const QString& text = {})
    {
        QKeyEvent ev(down ? QEvent::KeyPress : QEvent::KeyRelease, k, m, scan, 0, 0, text);
        QApplication::sendEvent(&win, &ev);
    }
    void button(Qt::MouseButton b, bool down)
    {
        QMouseEvent ev(down ? QEvent::MouseButtonPress : QEvent::MouseButtonRelease,
                       QPointF(10, 10), QPointF(10, 10), b, down ? b : Qt::NoButton, Qt::NoModifier);
        QApplication::sendEvent(&win, &ev);
    }
    int count(SDL_Scancode sc, bool pressed) const
    {
        int n = 0;
        for (const auto& e : keys) if (e.sc == sc && e.pressed == pressed) ++n;
        return n;
    }
};

const Qt::KeyboardModifiers kCtrlAlt = Qt::ControlModifier | Qt::AltModifier;

}  // namespace

int main(int argc, char** argv)
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QTemporaryDir cfgdir;
    qputenv("JNEXT_CONFIG_DIR", cfgdir.path().toUtf8());
    QApplication app(argc, argv);

    {   // MRQ-01
        Fixture f;
        f.capture_now();
        const bool was = f.captured();
        f.key(Qt::Key_Alt, true, kCtrlAlt);
        check("MRQ-01", "default: captured Ctrl+Alt releases, and the Alt press still reaches the ZX keyboard",
              was && !f.captured() && f.count(SDL_SCANCODE_LALT, true) == 1);
    }
    {   // MRQ-02
        Fixture f(RM::Esc);
        f.capture_now();
        const bool was = f.captured();
        f.key(Qt::Key_Escape, true);
        f.key(Qt::Key_Escape, false);
        check("MRQ-02", "esc: a captured Esc press+release releases and neither reaches the ZX keyboard",
              was && !f.captured() && f.count(SDL_SCANCODE_ESCAPE, true) == 0 &&
                  f.count(SDL_SCANCODE_ESCAPE, false) == 0,
              std::to_string(f.keys.size()));
    }
    {   // MRQ-03
        Fixture f(RM::Esc);
        f.capture_now();
        f.key(Qt::Key_Alt, true, kCtrlAlt);
        check("MRQ-03", "esc: captured Ctrl+Alt does not release", f.captured());
    }
    {   // MRQ-04
        Fixture f(RM::Esc);
        f.key(Qt::Key_Escape, true);
        f.key(Qt::Key_Escape, false);
        check("MRQ-04", "esc: an uncaptured Esc press and release both reach the ZX keyboard (BREAK intact)",
              f.count(SDL_SCANCODE_ESCAPE, true) == 1 && f.count(SDL_SCANCODE_ESCAPE, false) == 1);
    }
    {   // MRQ-05
        Fixture f(RM::Esc);
        f.capture_now();
        f.key(Qt::Key_Escape, true);                 // releases; its key-up goes elsewhere
        QFocusEvent out(QEvent::FocusOut);
        QApplication::sendEvent(&f.win, &out);
        f.key(Qt::Key_Escape, true);
        check("MRQ-05", "esc: after a focus loss the swallow is forgotten and an uncaptured Esc is BREAK",
              !f.captured() && f.count(SDL_SCANCODE_ESCAPE, true) == 1);
    }
    {   // MRQ-06
        Fixture f(RM::MiddleClick);
        f.capture_now();
        f.button(Qt::MiddleButton, true);
        const int after_press = f.emu.mouse().buttons();
        f.button(Qt::MiddleButton, false);
        check("MRQ-06", "middle-click: a captured middle press releases; Kempston buttons stay 0 (no stuck bit)",
              !f.captured() && after_press == 0 && f.emu.mouse().buttons() == 0,
              std::to_string(after_press));
    }
    {   // MRQ-07
        Fixture f(RM::MiddleClick);
        f.capture_now();
        f.button(Qt::LeftButton, true);
        check("MRQ-07", "middle-click: a captured left press is Kempston bit 1 (0x02) and keeps the capture",
              f.captured() && f.emu.mouse().buttons() == 0x02, std::to_string(f.emu.mouse().buttons()));
    }
    {   // MRQ-08
        Fixture f;
        f.capture_now();
        f.button(Qt::MiddleButton, true);
        check("MRQ-08", "default: a captured middle press is Kempston bit 2 (0x04) and keeps the capture",
              f.captured() && (f.emu.mouse().buttons() & 0x04), std::to_string(f.emu.mouse().buttons()));
    }
    {   // MRQ-09
        Fixture f(RM::MiddleClick);
        f.capture_now();
        f.key(Qt::Key_Alt, true, kCtrlAlt);
        check("MRQ-09", "middle-click: captured Ctrl+Alt does not release", f.captured());
    }
    {   // MRQ-10
        Fixture f(RM::Esc);
        const QString base = f.win.windowTitle();
        f.capture_now();
        const QString title = f.win.windowTitle();
        check("MRQ-10a", "esc: the captured title starts with the base title and says Esc to release mouse",
              title.startsWith(base) && title.contains(QStringLiteral("Esc to release mouse")),
              title.toStdString());
        check("MRQ-10b", "esc: the status bar reads exactly 'Mouse captured - Esc to release'",
              f.win.statusBar()->currentMessage() == QStringLiteral("Mouse captured - Esc to release"),
              f.win.statusBar()->currentMessage().toStdString());
        Fixture g(RM::MiddleClick);
        const QString tip = g.capture ? g.capture->statusTip() : QString();
        check("MRQ-10c", "middle-click: the Capture Mouse status tip names 'middle click to release'",
              tip.contains(QStringLiteral("middle click to release")) && !tip.contains(QStringLiteral("%1")),
              tip.toStdString());
    }
    {   // MRQ-11
        Fixture f;
        AppConfigData c;
        c.machine_type = MachineType::ZX48K;
        c.mouse_release = RM::Esc;
        f.win.apply_preferences(c);
        f.capture_now();
        f.key(Qt::Key_Escape, true);
        check("MRQ-11", "apply_preferences switches the method live: a captured Esc then releases",
              !f.captured() && f.count(SDL_SCANCODE_ESCAPE, true) == 0);
    }
    {   // MRQ-12
        AppConfigData initial;
        initial.mouse_release = RM::Esc;
        PreferencesDialog dlg(initial);
        auto* combo = dlg.findChild<QComboBox*>(QStringLiteral("mouseReleaseCombo"));
        const bool have = combo != nullptr;
#ifdef Q_OS_MACOS
        const char* first = "Alt+Cmd";
#else
        const char* first = "Ctrl+Alt";
#endif
        check("MRQ-12a", "Preferences > Input has mouseReleaseCombo: ctrl-alt, middle-click, esc in order",
              have && combo->count() == 3 &&
                  combo->itemData(0).toInt() == static_cast<int>(RM::CtrlAlt) &&
                  combo->itemData(1).toInt() == static_cast<int>(RM::MiddleClick) &&
                  combo->itemData(2).toInt() == static_cast<int>(RM::Esc) &&
                  combo->itemText(0) == QString::fromLatin1(first) &&
                  combo->itemText(1) == QStringLiteral("middle click") &&
                  combo->itemText(2) == QStringLiteral("Esc"));
        check("MRQ-12b", "the combo is seeded from the saved method (Esc)",
              have && combo->currentData().toInt() == static_cast<int>(RM::Esc));

        AppConfigData collected;
        bool emitted = false;
        QObject::connect(&dlg, &PreferencesDialog::apply_requested,
                         [&](const AppConfigData& c) { collected = c; emitted = true; });
        auto* buttons = dlg.findChild<QDialogButtonBox*>();
        buttons->button(QDialogButtonBox::Ok)->click();   // hungarian-ui: Apply is greyed here
        check("MRQ-12d", "with no change, collect() keeps Esc",
              emitted && collected.mouse_release == RM::Esc);
        emitted = false;
        if (have) combo->setCurrentIndex(1);
        buttons->button(QDialogButtonBox::Apply)->click();
        check("MRQ-12c", "selecting middle click makes collect() return MiddleClick",
              emitted && collected.mouse_release == RM::MiddleClick);
    }

    // --- hungarian-ui: the host keyboard layout ----------------------------
#ifdef Q_OS_WIN
    const quint32 kRightAlt = host_layout::RIGHT_ALT_SCAN_WINDOWS, kLeftAlt = 0x38, kCtrl = 0x1d;
#else
    const quint32 kRightAlt = host_layout::RIGHT_ALT_SCAN_XKB, kLeftAlt = 64, kCtrl = 37;
#endif
    {   // MRQ-HU-01
        auto run = [&](host_layout::Mode mode, bool left) {
            Fixture f;
            AppConfigData c;
            c.windows_layout = mode;
            f.win.apply_startup_config(c);
            f.capture_now();
            f.native_key(Qt::Key_Control, true, Qt::ControlModifier, kCtrl);
            f.native_key(Qt::Key_Alt, true, kCtrlAlt, left ? kLeftAlt : kRightAlt);
            return f.captured();
        };
        const bool altgr_on = run(host_layout::Mode::On, false);
        const bool left_on = run(host_layout::Mode::On, true);
        const bool altgr_off = run(host_layout::Mode::Off, false);
        check("MRQ-HU-01", "layout on: captured Ctrl + right Alt (AltGr) keeps the mouse, Ctrl + left Alt releases; "
                           "layout off: Ctrl + right Alt releases, as before",
              altgr_on && !left_on && !altgr_off);
    }
    {   // MRQ-HU-02
        Fixture f;
        AppConfigData c;
        c.windows_layout = host_layout::Mode::On;
        f.win.apply_startup_config(c);
        std::vector<std::pair<host_layout::KeyEvent, bool>> got;
        bool take = true;
        f.win.set_layout_key_callback([&](const host_layout::KeyEvent& e, bool pressed) {
            got.emplace_back(e, pressed);
            return take;
        });
        // Hungarian Shift+4 = '!': key Exclam (classic: the 1 key), the 4 key's scan code.
        f.native_key(Qt::Key_Exclam, true, Qt::ShiftModifier, 13, QStringLiteral("!"));
        f.native_key(Qt::Key_Exclam, false, Qt::ShiftModifier, 13, QStringLiteral("!"));
        f.native_key(Qt::Key_Dead_Tilde, true, Qt::NoModifier, 10);
        f.native_key(Qt::Key_Alt, true, Qt::AltModifier, kRightAlt);
        f.native_key(Qt::Key_Alt, false, Qt::NoModifier, kRightAlt);
        const bool bang = got.size() == 5 && got[0].second && !got[1].second &&
                          got[0].first.phys == 13 && got[0].first.sc == SDL_SCANCODE_1 &&
                          got[0].first.ch == U'!' && !got[0].first.dead && got[1].first.phys == 13;
        const bool dead = got.size() == 5 && got[2].first.dead && got[2].first.phys == 10;
        const bool altgr = got.size() == 5 && got[3].first.sc == SDL_SCANCODE_RALT && got[4].first.sc == SDL_SCANCODE_RALT;
        check("MRQ-HU-02", "the window hands every key to the layout callback (not the key callback): physical "
                           "scan code, classic scancode, character, dead key; the right Alt as SC_RALT",
              bang && dead && altgr && f.keys.empty(),
              "events=" + std::to_string(got.size()) + " key_callback=" + std::to_string(f.keys.size()));
    }
    {   // MRQ-HU-03
        AppConfigData initial;
        initial.windows_layout = host_layout::Mode::Off;
        initial.alt_digit_caps_shift = false;
        PreferencesDialog dlg(initial);
        auto* combo = dlg.findChild<QComboBox*>(QStringLiteral("windowsLayoutCombo"));
        auto* box = dlg.findChild<QCheckBox*>(QStringLiteral("altDigitCapsShiftCheck"));
        const bool have = combo && box && combo->count() == 3 &&
            combo->itemData(0).toInt() == static_cast<int>(host_layout::Mode::Auto) &&
            combo->itemData(1).toInt() == static_cast<int>(host_layout::Mode::On) &&
            combo->itemData(2).toInt() == static_cast<int>(host_layout::Mode::Off);
        const bool seeded = have && combo->currentData().toInt() == static_cast<int>(host_layout::Mode::Off) &&
                            !box->isChecked();
        AppConfigData collected;
        bool emitted = false;
        QObject::connect(&dlg, &PreferencesDialog::apply_requested,
                         [&](const AppConfigData& c) { collected = c; emitted = true; });
        if (have) { combo->setCurrentIndex(1); box->setChecked(true); }
        dlg.findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Apply)->click();
        check("MRQ-HU-03", "Preferences > Input: windowsLayoutCombo (Auto, On, Off) and altDigitCapsShiftCheck, "
                           "seeded from the saved values and returned by collect()",
              seeded && emitted && collected.windows_layout == host_layout::Mode::On && collected.alt_digit_caps_shift);
    }
    {   // MRQ-HU-04
        Fixture f;
        std::vector<std::pair<bool, bool>> modes;
        f.win.set_layout_mode_callback([&](bool layout, bool alt_digit) { modes.emplace_back(layout, alt_digit); });
        AppConfigData c;
        c.windows_layout = host_layout::Mode::On;
        c.alt_digit_caps_shift = false;
        f.win.apply_startup_config(c);
        c.windows_layout = host_layout::Mode::Auto;
        c.alt_digit_caps_shift = true;
        ui_language::set_session_choice(ui_language::Choice::English);
        f.win.apply_startup_config(c);
        ui_language::set_session_choice(ui_language::Choice::Hungarian);
        f.win.apply_startup_config(c);
        ui_language::set_session_choice(ui_language::Choice::Auto);
        const std::vector<std::pair<bool, bool>> want = {{false, false}, {true, false}, {false, true}, {true, true}};
        std::string seen;
        for (const auto& m : modes) seen += std::string(m.first ? "L" : "-") + (m.second ? "A " : "- ");
        check("MRQ-HU-04", "the switches reach the layout module: at once when the callback is set, then on every "
                           "apply_startup_config(); Auto is on with the Hungarian interface only",
              modes == want, seen);
    }

    {   // MRQ-HU-05
        // A ShortcutOverride the window accepts is a key Qt's shortcuts (menu
        // mnemonics, Alt+Q ...) do not get. Sent as Qt's shortcut map sends it,
        // not accepted to begin with.
        auto takes = [](Fixture& f, Qt::KeyboardModifiers m, const QString& text) {
            QKeyEvent ev(QEvent::ShortcutOverride, Qt::Key_B, m, 48, 0, 0, text);
            ev.ignore();
            QApplication::sendEvent(&f.win, &ev);
            return ev.isAccepted();
        };
        Fixture on;
        AppConfigData c;
        c.windows_layout = host_layout::Mode::On;
        on.win.apply_startup_config(c);
        on.native_key(Qt::Key_Alt, true, Qt::AltModifier, kRightAlt);              // AltGr down
        const bool altgr = takes(on, Qt::AltModifier, QStringLiteral("{"));
        on.native_key(Qt::Key_Alt, false, Qt::NoModifier, kRightAlt);
        const bool win_altgr = takes(on, kCtrlAlt, QStringLiteral("{"));           // Windows: Ctrl + Alt
        const bool plain_alt = takes(on, Qt::AltModifier, QStringLiteral("b"));     // left Alt + B: the menu's
        const bool chord = takes(on, kCtrlAlt, QString());                         // Ctrl+Alt+B: no character
        Fixture off;
        c.windows_layout = host_layout::Mode::Off;
        off.win.apply_startup_config(c);
        const bool off_altgr = takes(off, kCtrlAlt, QStringLiteral("{"));
        check("MRQ-HU-05", "layout on: AltGr + B typing '{' is taken from Qt's shortcuts (right Alt held, or Ctrl + Alt "
                           "with the character); left Alt + B and a characterless Ctrl+Alt+B stay shortcuts; layout off: unchanged",
              altgr && win_altgr && !plain_alt && !chord && !off_altgr);
    }

    std::printf("\n==============================================\n");
    std::printf("Total: %4d  Passed: %4d  Failed: %4d  Skipped: %4d\n", g_total, g_pass, g_fail, 0);
    return g_fail == 0 ? 0 : 1;
}
