#include "gui/preferences_dialog.h"
#include "gui/host_shortcuts.h"   // GH #403
#include "gui/qt_ctrl_is_command.h"   // GH #365
#include "qt/palette_tint_qt.h"   // GH #363
#include "gui/emulator_widget.h"
#ifdef ENABLE_DEBUGGER
#include "gui/shortcut_capture_button.h"
#endif

#include "core/nextpi_provisioner.h"
#include "peripheral/esp_host_policy.h"
#include "debug/source_search.h"   // GH #420
#include <QMessageBox>

#include <QComboBox>
#include <QSpinBox>
#include <QSlider>
#include <QCheckBox>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QLabel>
#include <QPushButton>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTabWidget>
#include <QAbstractButton>
#include <QButtonGroup>
#include <QGroupBox>
#include <QRadioButton>
#include <QSignalBlocker>
#include <QSpacerItem>
#include <QTimer>
#include <QAbstractSpinBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QGridLayout>
#include <QRegularExpression>
#include <QScrollArea>

#include <cmath>

namespace {

// hungarian-ui - a justified, word-wrapped note whose hint does not depend on its present width:
// it asks for the width of half its text and the height that gives (two lines), as its minimum
// too. QLabel's own hint for wrapped text is taken at a guessed narrow width (five lines for two),
// and Windows grows a dialog to its hint when it is moved, which left empty lines under the note.
// A narrower window than that still gets every line: the layout asks heightForWidth().
class FitNoteLabel : public QLabel {
public:
    FitNoteLabel(const QString& text, QWidget* parent) : QLabel(parent), plain_(text) {
        setTextFormat(Qt::RichText);
        setText(QStringLiteral("<div align=\"justify\">%1</div>").arg(text.toHtmlEscaped()));
        setWordWrap(true);
    }
    QSize sizeHint() const override {
        const int w = fontMetrics().horizontalAdvance(plain_) / 2 + fontMetrics().averageCharWidth() * 6;
        return QSize(w, heightForWidth(w));
    }
    QSize minimumSizeHint() const override { return sizeHint(); }

private:
    QString plain_;
};

} // namespace

PreferencesDialog::PreferencesDialog(const AppConfigData& current, QWidget* parent,
                                     const std::vector<jnext::dbgkeys::LoadIssue>& key_issues,
                                     const std::vector<HostChord>& host_chords,
                                     const std::vector<JoyDeviceInfo>& joy_devices)
    : QDialog(parent), joy_devices_(joy_devices),
      source_dirs_saved_(current.source_dirs),
      source_scheme_(current.source_scheme),
      debug_keys_(current.debug_keys),
      recent_files_(current.recent_files),
      crt_filter_saved_(current.crt_filter),
      crt_shader_saved_(current.crt_shader),
      crt_shader_style_saved_(current.crt_shader_style),
      host_chords_(host_chords)
{
    for (int i = 0; i < 2; ++i) {
        initial_joy_source_[i] = current.joy_source[i];
        initial_joy_device_[i] = JoyDeviceRef{ current.joy_device[i].toStdString(),
                                               current.joy_device_name[i].toStdString() };
    }
    setWindowTitle(tr("Preferences"));

    // Populated by build_startup_tab()/build_paths_tab() below, then filled
    // from `current` once every widget exists (avoids partially-initialised
    // reads if a future field is added to one tab but not the other).
    auto* tabs = new QTabWidget(this);
    tabs->addTab(build_startup_tab(), tr("Startup"));
    tabs->addTab(build_input_tab(), tr("Input"));
    tabs->addTab(build_audio_tab(), tr("Audio"));
    tabs->addTab(build_network_tab(), tr("Network"));
    tabs->addTab(build_nextpi_tab(), tr("NextPi"));
    tabs->addTab(build_paths_tab(), tr("Paths"));
#ifdef ENABLE_DEBUGGER
    tabs->addTab(build_debugger_tab(), tr("Debugger"));
    tabs->addTab(build_debug_keys_tab(key_issues), tr("Debugger Keys"));
#else
    // No tab, but debug_keys_ was still seeded from `current` above and is
    // still written back by collect() — a debugger-less build must not wipe a
    // user's bindings just by opening this dialog (GH #1, GH #25).
    (void)key_issues;
#endif

    // --- Fill from `current` ---
    machine_combo_->setCurrentIndex(machine_combo_->findData(static_cast<int>(current.machine_type)));
    cpu_speed_combo_->setCurrentIndex(cpu_speed_combo_->findData(static_cast<int>(current.cpu_speed)));
    emu_speed_spin_->setValue(current.emulator_speed_percent);
    fast_boot_spin_->setValue(current.fast_boot_seconds);
    fast_boot_hide_check_->setChecked(current.fast_boot_hide_screen);
    language_combo_->setCurrentIndex(language_combo_->findData(static_cast<int>(current.ui_language)));
    when_slow_combo_->setCurrentIndex(
        when_slow_combo_->findData(static_cast<int>(current.when_slow_prefer)));
    mouse_release_combo_->setCurrentIndex(
        mouse_release_combo_->findData(static_cast<int>(current.mouse_release)));
    windows_layout_combo_->setCurrentIndex(
        windows_layout_combo_->findData(static_cast<int>(current.windows_layout)));
    alt_digit_check_->setChecked(current.alt_digit_caps_shift);
    scale_combo_->setCurrentIndex(scale_combo_->findData(current.window_scale_x2));
    // Both set (a hand-edited file): the shader wins, as it does at start-up. The Lite
    // style is Soft Scanlines. Without the shader choices only Scanlines can show.
    int crt_choice = CRT_OFF;
    if (crt_combo_->findData(CRT_TV) >= 0 && current.crt_shader)
        crt_choice = current.crt_shader_style == crt_shader::Style::Pc ? CRT_PC
                   : current.crt_shader_style == crt_shader::Style::Lite ? CRT_SOFT : CRT_TV;
    else if (current.crt_filter ||
             (current.crt_shader && current.crt_shader_style == crt_shader::Style::Lite))
        crt_choice = CRT_SCANLINES;   // no shader here: start-up runs a saved Soft scanlines as Scanlines
    crt_combo_->setCurrentIndex(crt_combo_->findData(crt_choice));
    crt_initial_choice_ = crt_choice;
    silent_check_->setChecked(current.silent);
    tape_fast_check_->setChecked(current.tape_fast_load);
    tape_auto_check_->setChecked(current.tape_auto_play);
    audio_gain_slider_->setValue(static_cast<int>(std::lround(current.audio_gain_db)));
    audio_beeper_gain_slider_->setValue(
        static_cast<int>(std::lround(current.audio_gain_beeper_db)));
    for (int chip = 0; chip < 3; ++chip)
        audio_ay_gain_slider_[chip]->setValue(
            static_cast<int>(std::lround(current.audio_gain_ay_db[chip])));
    audio_dac_gain_slider_->setValue(
        static_cast<int>(std::lround(current.audio_gain_dac_db)));
    audio_oversample_check_->setChecked(current.audio_oversample);
    ay_stereo_combo_->setCurrentIndex(
        ay_stereo_combo_->findData(static_cast<int>(current.audio_channels.ay_stereo)));
    for (int chip = 0; chip < 3; ++chip) ay_mono_check_[chip]->setChecked(current.audio_channels.ay_mono[chip]);
    for (int ch = 0; ch < DAC_MIX_CHANNELS; ++ch)
        dac_channel_slider_[ch]->setValue(static_cast<int>(std::lround(current.audio_channels.dac_gain_db[ch])));
    set_dac_layout(current.audio_channels.dac_pan);
    esp_enabled_check_->setChecked(current.esp_enabled);
    {
        QStringList hosts;
        for (const std::string& host : current.esp_allowed_hosts)
            hosts << QString::fromStdString(host);
        esp_hosts_edit_->setPlainText(hosts.join(QLatin1Char('\n')));
    }
    esp_hosts_edit_->setEnabled(current.esp_enabled);
    nextpi_enabled_check_->setChecked(current.nextpi_enabled);
    nextpi_dir_edit_->setText(current.nextpi_dir);
    nextpi_release_combo_->setEditText(current.nextpi_release);
    nextpi_qemu_edit_->setText(current.nextpi_qemu_binary);
    nextpi_audio_combo_->setEditText(current.nextpi_audio);
    nextpi_qemu_args_edit_->setText(current.nextpi_qemu_args);
    last_load_dir_edit_->setText(current.last_load_dir);
    sd_card_path_edit_->setText(current.sd_card_path);
    sd_card2_path_edit_->setText(current.sd_card2_path);
    screenshot_dir_edit_->setText(current.screenshot_dir);
    if (source_dirs_edit_) source_dirs_edit_->setPlainText(current.source_dirs.join(QLatin1Char('\n')));
    if (source_scheme_combo_)
        source_scheme_combo_->setCurrentIndex(
            source_scheme_combo_->findData(static_cast<int>(current.source_scheme)));
    quick_screenshot_dir_edit_->setText(current.quick_screenshot_dir);
    quick_screenshot_fmt_combo_->setCurrentIndex(
        quick_screenshot_fmt_combo_->findData(
            static_cast<int>(current.quick_screenshot_format)));
    quick_keep_spin_->setValue(current.quick_snapshot_keep);

    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Apply, this);
    connect(buttons, &QDialogButtonBox::accepted, this, [this]() {
        if (!source_dirs_ok()) return;
        emit apply_requested(collect());
        accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    apply_button_ = buttons->button(QDialogButtonBox::Apply);
    connect(apply_button_, &QPushButton::clicked, this, [this]() {
        if (!source_dirs_ok()) return;
        emit apply_requested(collect());
        mark_applied();
    });
    watch_controls();
    mark_applied();

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(tabs);
    layout->addWidget(buttons);
}

QWidget* PreferencesDialog::build_startup_tab() {
    auto* tab = new QWidget(this);
    auto* form = new QFormLayout(tab);

    machine_combo_ = new QComboBox(tab);
    machine_combo_->addItem(tr("48K"),  static_cast<int>(MachineType::ZX48K));
    machine_combo_->addItem(tr("128K"), static_cast<int>(MachineType::ZX128K));
    machine_combo_->addItem(tr("+3"),   static_cast<int>(MachineType::ZX_PLUS3));
    machine_combo_->addItem(tr("Pentagon"), static_cast<int>(MachineType::PENTAGON));
    machine_combo_->addItem(tr("Next"), static_cast<int>(MachineType::ZXN_ISSUE2));
    form->addRow(tr("Machine type:"), machine_combo_);

    cpu_speed_combo_ = new QComboBox(tab);
    cpu_speed_combo_->addItem(tr(cpu_speed_str(CpuSpeed::MHZ_3_5)), static_cast<int>(CpuSpeed::MHZ_3_5));
    cpu_speed_combo_->addItem(tr(cpu_speed_str(CpuSpeed::MHZ_7)),   static_cast<int>(CpuSpeed::MHZ_7));
    cpu_speed_combo_->addItem(tr(cpu_speed_str(CpuSpeed::MHZ_14)),  static_cast<int>(CpuSpeed::MHZ_14));
    cpu_speed_combo_->addItem(tr(cpu_speed_str(CpuSpeed::MHZ_28)),  static_cast<int>(CpuSpeed::MHZ_28));
    form->addRow(tr("CPU speed:"), cpu_speed_combo_);

    emu_speed_spin_ = new QSpinBox(tab);
    emu_speed_spin_->setRange(10, 1000);
    emu_speed_spin_->setSingleStep(10);
    emu_speed_spin_->setSuffix(tr("%"));
    form->addRow(tr("Emulator speed:"), emu_speed_spin_);

    // Issue #35 — the degradation policy. Sits next to Emulator speed because
    // it is the other control over how emulated time meets real time, and it
    // only ever does anything when the two cannot be reconciled.
    when_slow_combo_ = new QComboBox(tab);
    when_slow_combo_->setObjectName(QStringLiteral("whenSlowPreferCombo"));
    when_slow_combo_->addItem(
        tr("Smooth sound (drop frames)"),
        static_cast<int>(audio_pacing::WhenSlowPrefer::Audio));
    when_slow_combo_->addItem(
        tr("Smooth picture (run slower, sound stutters)"),
        static_cast<int>(audio_pacing::WhenSlowPrefer::Video));
    when_slow_combo_->setToolTip(
        tr("Only takes effect on a host that cannot emulate in real time. "
           "Smooth sound keeps the sound card fed by emulating two frames in "
           "one slot, so the first of the pair is never shown. Smooth picture "
           "shows every frame and lets the machine run slower than real time."));
    form->addRow(tr("When the host is too slow:"), when_slow_combo_);

    // GH #499 - a launch setting: it applies at the next start and changes nothing live.
    fast_boot_spin_ = new QSpinBox(tab);
    fast_boot_spin_->setObjectName(QStringLiteral("fastBootSpin"));
    fast_boot_spin_->setRange(static_cast<int>(config_keys::info(config_keys::KeyId::StartupFastBootSeconds).lo),
                              static_cast<int>(config_keys::info(config_keys::KeyId::StartupFastBootSeconds).hi));
    fast_boot_spin_->setSuffix(tr(" s"));
    fast_boot_spin_->setSpecialValueText(tr("Off"));
    fast_boot_spin_->setToolTip(
        tr("Run the first seconds of a plain Next start at full speed with the sound off. "
           "Any key returns to normal speed. Not used when a program is loaded at start. "
           "Takes effect at the next start."));
    form->addRow(tr("Fast boot:"), fast_boot_spin_);

    // GH #544 - a launch setting too.
    fast_boot_hide_check_ = new QCheckBox(tr("Hide the screen during fast boot"), tab);
    fast_boot_hide_check_->setObjectName(QStringLiteral("fastBootHideCheck"));
    fast_boot_hide_check_->setToolTip(
        tr("Keep the window black while fast boot runs, so the first picture is the screen it ends on "
           "(normally the NextZXOS menu). Any key shows the screen at once. "
           "Takes effect at the next start."));
    form->addRow(QString(), fast_boot_hide_check_);
    // startup.language - a launch setting like Fast boot: the window and the
    // debugger are built in one language, so a change shows at the next start.
    // The language names are written in their own language, as is usual in a
    // language list, and are not translated.
    language_combo_ = new QComboBox(tab);
    language_combo_->setObjectName(QStringLiteral("languageCombo"));
    language_combo_->addItem(tr("Automatic (system language)"),
                             static_cast<int>(ui_language::Choice::Auto));
    language_combo_->addItem(QStringLiteral("English"),
                             static_cast<int>(ui_language::Choice::English));
    language_combo_->addItem(QStringLiteral("Magyar"),
                             static_cast<int>(ui_language::Choice::Hungarian));
    language_combo_->setToolTip(
        tr("The language of the menus, dialogs and the debugger. "
           "Takes effect at the next start."));
    form->addRow(tr("Language:"), language_combo_);

    scale_combo_ = new QComboBox(tab);
    // Half steps (EmulatorWidget::set_scale_x2): 1x, 1.5x, 2x, 3x, with the size.
    for (int h : {2, 3, 4, 6}) {
        const QString factor = (h % 2) ? QStringLiteral("%1.5").arg(h / 2) : QString::number(h / 2);
        scale_combo_->addItem(tr("%1x (%2x%3)").arg(factor)
                                  .arg(EmulatorWidget::NATIVE_W * h / 2)
                                  .arg(EmulatorWidget::DISPLAY_H * h / 2), h);
    }
    form->addRow(tr("Window scale:"), scale_combo_);

    // One CRT effect at a time, as a pop-up: Off, Scanlines (the software overlay), or one of the three
    // shader looks, which exist only in a build that has the shader. Without them
    // collect() hands the saved shader settings back (crt_shader_saved_).
    crt_combo_ = new QComboBox(tab);
    crt_combo_->addItem(tr("Off"), CRT_OFF);
    crt_combo_->addItem(tr("Scanlines"), CRT_SCANLINES);
    if (EmulatorWidget::crt_shader_built_in()) {
        crt_combo_->addItem(tr("Soft scanlines (GPU)"), CRT_SOFT);
        crt_combo_->addItem(tr("TV (curved, bloom)"), CRT_TV);
        crt_combo_->addItem(tr("PC monitor"), CRT_PC);
    }
    form->addRow(tr("CRT effect:"), crt_combo_);

    silent_check_ = new QCheckBox(tr("Start muted (no audio output)"), tab);
    silent_check_->setToolTip(tr("Applies on next launch - there is no live audio-device toggle."));
    form->addRow(QString(), silent_check_);

    tape_fast_check_ = new QCheckBox(tr("Tape fast-load by default"), tab);
    form->addRow(QString(), tape_fast_check_);
    tape_auto_check_ = new QCheckBox(tr("Tape: automatic play/stop"), tab);
    form->addRow(QString(), tape_auto_check_);

    return tab;
}

QWidget* PreferencesDialog::build_input_tab() {
    auto* tab = new QWidget(this);
    auto* form = new QFormLayout(tab);

    // One combo per connector, filled from joy_choices(): Automatic, each
    // present controller, a "(not connected)" entry for an assigned controller
    // that is absent, Cursor Keys + Space, None. Item data is a key string
    // ("auto" | "keys" | "none" | "dev:<id>"), the name rides in UserRole + 1.
    auto make_source_combo = [&](int conn) {
        auto* c = new QComboBox(tab);
        const auto choices = joy_choices(conn, joy_devices_, initial_joy_source_[conn],
                                         initial_joy_device_[conn]);
        for (const JoyChoice& ch : choices) {
            QString key;
            QString name;
            switch (ch.kind) {
            case JoyChoice::Kind::Auto: key = "auto"; break;
            case JoyChoice::Kind::Keys: key = "keys"; break;
            case JoyChoice::Kind::None: key = "none"; break;
            case JoyChoice::Kind::Device:
                key = "dev:" + QString::fromStdString(ch.id);
                for (const auto& d : joy_devices_)
                    if (d.id == ch.id) name = QString::fromStdString(d.name);
                break;
            case JoyChoice::Kind::Missing:
                key = "dev:" + QString::fromStdString(ch.id);
                name = QString::fromStdString(initial_joy_device_[conn].name);
                break;
            }
            // Device names are the controller's own text, never translated.
            const bool fixed = ch.kind != JoyChoice::Kind::Device &&
                               ch.kind != JoyChoice::Kind::Missing;
            c->addItem(fixed ? tr(ch.label.c_str()) : QString::fromStdString(ch.label), key);
            c->setItemData(c->count() - 1, name, Qt::UserRole + 1);
            if (ch.checked) c->setCurrentIndex(c->count() - 1);
        }
        return c;
    };
    joy1_source_combo_ = make_source_combo(0);
    joy2_source_combo_ = make_source_combo(1);
    form->addRow(tr("Joy 1 source (port 0x1F):"), joy1_source_combo_);
    form->addRow(tr("Joy 2 source (port 0x37):"), joy2_source_combo_);

    // The host cursor keys can drive only one connector, and one controller
    // only one connector. Picking either on one side resets the other side to
    // Automatic so the pair stays valid (the Emulator enforces the same rules).
    auto enforce_exclusive = [](QComboBox* changed, QComboBox* other) {
        const QString key = changed->currentData().toString();
        if ((key == QLatin1String("keys") || key.startsWith(QLatin1String("dev:"))) &&
            other->currentData().toString() == key) {
            other->setCurrentIndex(other->findData(QStringLiteral("auto")));
        }
    };
    connect(joy1_source_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [=]() { enforce_exclusive(joy1_source_combo_, joy2_source_combo_); });
    connect(joy2_source_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [=]() { enforce_exclusive(joy2_source_combo_, joy1_source_combo_); });

    // GH #365 — the gesture that releases a captured mouse. The labels are
    // pointer_capture::release_hint(), the same text the title and tips use.
    mouse_release_combo_ = new QComboBox(tab);
    mouse_release_combo_->setObjectName(QStringLiteral("mouseReleaseCombo"));
    for (auto m : { pointer_capture::ReleaseMethod::CtrlAlt,
                    pointer_capture::ReleaseMethod::MiddleClick,
                    pointer_capture::ReleaseMethod::Esc })
        mouse_release_combo_->addItem(
            QString::fromLatin1(pointer_capture::release_hint(m, kQtCtrlIsCommand)),
            static_cast<int>(m));
    form->addRow(tr("Release captured mouse with:"), mouse_release_combo_);
    auto* release_note = new QLabel(
        tr("The chosen click or key is not passed to the program while the "
           "mouse is captured."), tab);
    release_note->setWordWrap(true);
    form->addRow(QString(), release_note);

    // hungarian-ui - typing by the host keyboard layout's characters
    // (platform/host_layout_keys.h). Takes effect at once, like the mouse release.
    windows_layout_combo_ = new QComboBox(tab);
    windows_layout_combo_->setObjectName(QStringLiteral("windowsLayoutCombo"));
    windows_layout_combo_->addItem(tr("Automatic (on with the Hungarian interface)"),
                                   static_cast<int>(host_layout::Mode::Auto));
    windows_layout_combo_->addItem(tr("On"), static_cast<int>(host_layout::Mode::On));
    windows_layout_combo_->addItem(tr("Off"), static_cast<int>(host_layout::Mode::Off));
    windows_layout_combo_->setToolTip(
        tr("Each key types the character it types in the host keyboard layout (for example "
           "Hungarian), not the key in its place on a US keyboard. The extended-mode symbols "
           "[ ] { } \\ | ~ and the copyright sign are typed as a short key sequence."));
    form->addRow(tr("Host keyboard layout:"), windows_layout_combo_);
    alt_digit_check_ = new QCheckBox(tr("Left Alt + digit types Caps Shift + digit"), tab);
    alt_digit_check_->setObjectName(QStringLiteral("altDigitCapsShiftCheck"));
    alt_digit_check_->setToolTip(
        tr("EDIT, CAPS LOCK, TRUE/INV VIDEO, the cursor keys, GRAPH and DELETE, on a keyboard "
           "whose Shift + digit types other characters. While a debugger script is loaded, "
           "Alt+1..8 stay its keys."));
    form->addRow(QString(), alt_digit_check_);

    auto* note = new QLabel(
        tr("Cursor keys drive one connector's directions with Space as fire;\n"
           "while active, the arrows and Space stop acting as ZX keys."), tab);
    note->setWordWrap(true);
    form->addRow(QString(), note);

    return tab;
}

QWidget* PreferencesDialog::build_audio_tab() {
    auto* tab = new QWidget(this);
    auto* form = new QFormLayout(tab);

    auto add_gain = [&](const QString& label, const QString& object_name,
                        const QString& tooltip, QSlider*& slider) {
        form->addRow(label, make_gain_slider(tab, object_name, tooltip, slider));
    };

    add_gain(tr("Master:"), QStringLiteral("audioGainDbSlider"),
             tr("Host output gain after the complete mix."),
             audio_gain_slider_);
    add_gain(tr("Beeper:"), QStringLiteral("audioGainBeeperDbSlider"),
             tr("Host gain for EAR, MIC and tape-EAR sound."),
             audio_beeper_gain_slider_);
    add_gain(tr("AY #0:"), QStringLiteral("audioGainAy0DbSlider"),
             tr("Host gain for TurboSound AY chip 0."), audio_ay_gain_slider_[0]);
    add_gain(tr("AY #1:"), QStringLiteral("audioGainAy1DbSlider"),
             tr("Host gain for TurboSound AY chip 1."), audio_ay_gain_slider_[1]);
    add_gain(tr("AY #2:"), QStringLiteral("audioGainAy2DbSlider"),
             tr("Host gain for TurboSound AY chip 2."), audio_ay_gain_slider_[2]);
    add_gain(tr("DAC:"), QStringLiteral("audioGainDacDbSlider"),
             tr("Host gain for Specdrum, Soundrive and Covox DAC audio."),
             audio_dac_gain_slider_);

    const int line = tab->fontMetrics().height();
    form->addItem(new QSpacerItem(0, line, QSizePolicy::Minimum, QSizePolicy::Fixed));
    form->addRow(build_ay_channels_group(tab));
    form->addItem(new QSpacerItem(0, line, QSizePolicy::Minimum, QSizePolicy::Fixed));
    form->addRow(build_dac_channels_group(tab));

    // Across the whole tab, justified (Viktor). A FitNoteLabel: a plain wrapped label asks for several
    // lines more than it needs, so the dialog opened at its minimum and Windows grew it when it was moved.
    auto* note = new FitNoteLabel(
        tr("0 dB preserves each source. Subsystem gains are applied before the master gain; "
           "large boosts may clip. The DAC channel gains multiply with the DAC gain."), tab);
    note->setObjectName(QStringLiteral("audioNoteLabel"));
    form->addRow(note);

    audio_oversample_check_ = new QCheckBox(tr("Oversampled sound (4x)"), tab);
    audio_oversample_check_->setObjectName(QStringLiteral("audioOversampleCheck"));
    audio_oversample_check_->setToolTip(
        tr("Mixes at 176.4 kHz and filters down to 44.1 kHz: clearer high notes for a few "
           "per cent more CPU. Takes effect at once; recordings include it."));
    form->addRow(audio_oversample_check_);

    return tab;
}

QWidget* PreferencesDialog::make_gain_slider(QWidget* parent, const QString& object_name, const QString& tooltip,
                                             QSlider*& slider) {
    auto* row = new QWidget(parent);
    auto* h = new QHBoxLayout(row);
    h->setContentsMargins(0, 0, 0, 0);
    slider = new QSlider(Qt::Horizontal, row);
    slider->setObjectName(object_name);
    slider->setRange(-24, 24);
    slider->setSingleStep(1);
    slider->setPageStep(6);
    slider->setTickPosition(QSlider::TicksBelow);
    slider->setTickInterval(6);
    slider->setToolTip(tooltip);
    auto* value = new QLabel(row);
    value->setObjectName(object_name + QStringLiteral("Value"));
    value->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    value->setMinimumWidth(
        value->fontMetrics().horizontalAdvance(QStringLiteral("-24 dB")));
    auto update_value = [value](int db) {
        value->setText(QStringLiteral("%1%2 dB")
                           .arg(db > 0 ? QStringLiteral("+") : QString())
                           .arg(db));
    };
    connect(slider, &QSlider::valueChanged, value, update_value);
    update_value(slider->value());
    h->addWidget(slider, 1);
    h->addWidget(value);
    return row;
}

// hungarian-ui - Audio > AY channels: the stereo override, and what the machine has
// now beside what is heard with it.
QWidget* PreferencesDialog::build_ay_channels_group(QWidget* parent) {
    auto* group = new QGroupBox(tr("AY channels"), parent);
    auto* grid = new QGridLayout(group);

    auto* row = new QWidget(group);
    auto* h = new QHBoxLayout(row);
    h->setContentsMargins(0, 0, 0, 0);
    ay_stereo_combo_ = new QComboBox(row);
    ay_stereo_combo_->setObjectName(QStringLiteral("ayStereoCombo"));
    ay_stereo_combo_->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    ay_stereo_combo_->addItem(tr("As the machine"), static_cast<int>(AyStereoOrder::Machine));
    ay_stereo_combo_->addItem(QStringLiteral("ABC"), static_cast<int>(AyStereoOrder::Abc));
    ay_stereo_combo_->addItem(QStringLiteral("ACB"), static_cast<int>(AyStereoOrder::Acb));
    ay_stereo_combo_->setToolTip(
        tr("The channel order of all three AY chips. ABC: A left, B centre, C right. ACB: A left, "
           "C centre, B right. Programs still read what they set."));
    h->addWidget(ay_stereo_combo_);
    h->addSpacing(row->fontMetrics().height());
    h->addWidget(new QLabel(tr("Mono:"), row));
    for (int chip = 0; chip < 3; ++chip) {
        ay_mono_check_[chip] = new QCheckBox(QStringLiteral("AY #%1").arg(chip), row);
        ay_mono_check_[chip]->setObjectName(QStringLiteral("ay%1MonoCheck").arg(chip));
        ay_mono_check_[chip]->setToolTip(
            tr("Hear this chip in mono. A chip the program itself set to mono stays mono without it."));
        h->addWidget(ay_mono_check_[chip]);
    }
    h->addStretch(1);
    grid->addWidget(new QLabel(tr("AY stereo:"), group), 0, 0);
    grid->addWidget(row, 0, 1, 1, 4);

    // One grid for both rows, so a chip's two texts stand under each other.
    auto add_row = [&](int line, const QString& title, const QString& tooltip, const QString& name, QLabel* (&labels)[3]) {
        auto* head = new QLabel(title, group);
        head->setToolTip(tooltip);
        grid->addWidget(head, line, 0);
        for (int chip = 0; chip < 3; ++chip) {
            labels[chip] = new QLabel(group);
            labels[chip]->setObjectName(name.arg(chip));
            labels[chip]->setTextFormat(Qt::RichText);
            labels[chip]->setToolTip(tooltip);
            grid->addWidget(labels[chip], line, 1 + chip);
        }
    };
    add_row(1, tr("Machine now:"), tr("What the running program (or config.ini) has set."),
            QStringLiteral("ayMachine%1Label"), ay_machine_label_);
    add_row(2, tr("As heard:"), tr("The same with the settings above applied."),
            QStringLiteral("ayHeard%1Label"), ay_heard_label_);
    // Wide enough for the longest text, so nothing jumps when a program changes it.
    AyChipLayout longest[4];
    for (int i = 0; i < 4; ++i) {
        longest[i].sounds = i != 0;
        longest[i].left = i != 1;
        longest[i].right = i != 2;
        longest[i].mono = i != 3;
    }
    int width = 0;
    QLabel probe;
    probe.setTextFormat(Qt::RichText);
    for (const AyChipLayout& l : longest) {
        for (int mono = 0; mono < 2; ++mono) {
            AyChipLayout v = l;
            v.mono = mono != 0;
            probe.setText(QStringLiteral("AY #0: %1").arg(ay_layout_text(v)));
            width = qMax(width, probe.sizeHint().width());
        }
    }
    for (int chip = 0; chip < 3; ++chip) grid->setColumnMinimumWidth(1 + chip, width + parent->fontMetrics().height());
    grid->setColumnStretch(4, 1);

    ay_layout_timer_ = new QTimer(this);
    ay_layout_timer_->setInterval(500);
    connect(ay_layout_timer_, &QTimer::timeout, this, &PreferencesDialog::refresh_ay_layouts);
    refresh_ay_layouts();
    return group;
}

QString PreferencesDialog::ay_layout_text(const AyChipLayout& chip) {
    if (!chip.sounds) return tr("silent");
    const QString order = QStringLiteral("<b>%1</b>").arg(chip.mono ? tr("mono") : chip.acb ? QStringLiteral("ACB") : QStringLiteral("ABC"));
    if (chip.left && chip.right) return order;
    return chip.left ? tr("%1, left only").arg(order) : tr("%1, right only").arg(order);
}

void PreferencesDialog::set_ay_layout_source(std::function<AyLayouts()> source) {
    ay_layout_source_ = std::move(source);
    if (ay_layout_source_) ay_layout_timer_->start(); else ay_layout_timer_->stop();
    refresh_ay_layouts();
}

void PreferencesDialog::refresh_ay_layouts() {
    const AyLayouts now = ay_layout_source_ ? ay_layout_source_() : AyLayouts{};
    for (int chip = 0; chip < 3; ++chip) {
        const auto text = [&](const AyChipLayout& l) {
            return QStringLiteral("AY #%1: %2").arg(chip).arg(ay_layout_source_ ? ay_layout_text(l) : QStringLiteral("-"));
        };
        ay_machine_label_[chip]->setText(text(now.machine[chip]));
        ay_heard_label_[chip]->setText(text(now.heard[chip]));
    }
}

// hungarian-ui - Audio > DAC channels: volume and place of each Soundrive channel.
QWidget* PreferencesDialog::build_dac_channels_group(QWidget* parent) {
    auto* group = new QGroupBox(tr("DAC channels"), parent);
    auto* grid = new QGridLayout(group);

    dac_layout_combo_ = new QComboBox(group);
    dac_layout_combo_->setObjectName(QStringLiteral("dacLayoutCombo"));
    dac_layout_combo_->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    dac_layout_combo_->setProperty("derived", true);   // the buttons below are the setting
    dac_layout_combo_->addItem(tr("As the machine (A, B left; C, D right)"));
    dac_layout_combo_->addItem(tr("Left, centre, right (A left; B, C centre; D right)"));
    dac_layout_combo_->addItem(tr("Mono"));
    dac_layout_combo_->addItem(tr("Custom"));
    dac_layout_combo_->setToolTip(tr("Sets the places of the four channels below. Their gains are not changed."));
    grid->addWidget(new QLabel(tr("Layout:"), group), 0, 0);
    grid->addWidget(dac_layout_combo_, 0, 1, 1, 2, Qt::AlignLeft);

    const QString places[3] = {tr("Left"), tr("Centre"), tr("Right")};
    const char* const place_names[3] = {"Left", "Centre", "Right"};
    for (int ch = 0; ch < DAC_MIX_CHANNELS; ++ch) {
        const QChar letter = QLatin1Char(static_cast<char>('A' + ch));
        grid->addWidget(new QLabel(QStringLiteral("%1:").arg(letter), group), 1 + ch, 0);
        grid->addWidget(make_gain_slider(group, QStringLiteral("audioDac%1GainDbSlider").arg(letter),
                                         tr("Host gain of DAC channel %1, on top of the DAC gain.").arg(letter),
                                         dac_channel_slider_[ch]), 1 + ch, 1);
        auto* row = new QWidget(group);
        auto* h = new QHBoxLayout(row);
        h->setContentsMargins(row->fontMetrics().height() / 2, 0, 0, 0);
        dac_pan_group_[ch] = new QButtonGroup(row);
        for (int place = 0; place < 3; ++place) {
            auto* button = new QRadioButton(places[place], row);
            button->setObjectName(QStringLiteral("dac%1%2Radio").arg(letter).arg(QLatin1String(place_names[place])));
            button->setToolTip(tr("Where DAC channel %1 is heard. Centre gives half of it to each side.").arg(letter));
            dac_pan_group_[ch]->addButton(button, place);
            h->addWidget(button);
            connect(button, &QRadioButton::toggled, this, &PreferencesDialog::sync_dac_layout_combo);
        }
        grid->addWidget(row, 1 + ch, 2);
    }
    grid->setColumnStretch(1, 1);

    connect(dac_layout_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
        using P = DacPan;
        static constexpr DacPan presets[3][DAC_MIX_CHANNELS] = {
            {dac_machine_pan(0), dac_machine_pan(1), dac_machine_pan(2), dac_machine_pan(3)},
            {P::Left, P::Centre, P::Centre, P::Right},
            {P::Centre, P::Centre, P::Centre, P::Centre},
        };
        if (index >= 0 && index < 3) set_dac_layout(presets[index]);
        sync_dac_layout_combo();   // "Custom" is no layout to pick: the list goes back to what the buttons show
    });
    return group;
}

void PreferencesDialog::set_dac_layout(const DacPan (&pans)[DAC_MIX_CHANNELS]) {
    for (int ch = 0; ch < DAC_MIX_CHANNELS; ++ch)
        dac_pan_group_[ch]->button(static_cast<int>(pans[ch]))->setChecked(true);
}

void PreferencesDialog::sync_dac_layout_combo() {
    int code = 0;   // the four places as a base-3 number, A first
    for (int ch = 0; ch < DAC_MIX_CHANNELS; ++ch) {
        const int id = dac_pan_group_[ch] ? dac_pan_group_[ch]->checkedId() : -1;
        if (id < 0) return;   // still being filled
        code = code * 3 + id;
    }
    const QSignalBlocker block(dac_layout_combo_);   // it follows the buttons here, it does not set them
    dac_layout_combo_->setCurrentIndex(code == 8 ? 0 : code == 14 ? 1 : code == 40 ? 2 : 3);
}

QWidget* PreferencesDialog::build_network_tab() {
    // GH #25 — the persistent form of --esp and --esp-allow. The two fields
    // were already loaded and saved by AppConfig; what was missing was any way
    // to see or set them, which also meant collect() silently reset them (see
    // the header comment).
    auto* tab = new QWidget(this);
    auto* form = new QFormLayout(tab);

    esp_enabled_check_ = new QCheckBox(tr("Enable the emulated ESP-01 WiFi module"), tab);
    esp_enabled_check_->setObjectName(QStringLiteral("espEnabledCheck"));
    esp_enabled_check_->setToolTip(
        tr("Lets guest software (NXtel, nextsync) open real TCP connections "
           "from your machine. Off by default."));
    form->addRow(QString(), esp_enabled_check_);

    esp_hosts_edit_ = new QPlainTextEdit(tab);
    esp_hosts_edit_->setObjectName(QStringLiteral("espAllowedHostsEdit"));
    esp_hosts_edit_->setPlaceholderText(tr("nx.nxtel.org"));
    esp_hosts_edit_->setToolTip(
        tr("One host per line (a comma separates too). Matching is exact and "
           "case-insensitive: no wildcards, and an IP address must be listed "
           "as itself."));
    // Four lines is enough for the handful of literal hosts this is for, and
    // keeps the tab the same height as the others.
    esp_hosts_edit_->setFixedHeight(4 * esp_hosts_edit_->fontMetrics().lineSpacing() + 12);
    form->addRow(tr("Allowed hosts:"), esp_hosts_edit_);

    // The allowlist is meaningless while the module is off, and showing it as
    // editable would imply otherwise. Kept in sync from here on; the initial
    // state is set with the rest of the fields in the constructor.
    connect(esp_enabled_check_, &QCheckBox::toggled,
            esp_hosts_edit_, &QPlainTextEdit::setEnabled);

    auto* note = new QLabel(
        tr("An EMPTY list places no restriction on the host name - the guest "
           "may name any host. Listing hosts narrows that.\n\n"
           "Connections to loopback, link-local and cloud-metadata addresses "
           "are always refused; your own LAN is reachable.\n\n"
           "This setting is not applied to the machine that is already "
           "running: it takes effect on the next hard reset (F1, or "
           "Machine ▸ Power Reset) and on the next launch. --no-esp overrides "
           "it for a single run."), tab);
    note->setWordWrap(true);
    form->addRow(QString(), note);

    return tab;
}

QWidget* PreferencesDialog::build_nextpi_tab() {
    // The persistent form of --nextpi, plus the settings that have no CLI
    // form at all (jnext gives a feature one enabler and defaults the rest).
    // An empty field means the default shown as its placeholder.
    auto* tab = new QWidget(this);
    auto* form = new QFormLayout(tab);

    nextpi_enabled_check_ = new QCheckBox(tr("Start NextPi (under QEMU) when jnext starts the ZX Next"), tab);
    nextpi_enabled_check_->setObjectName(QStringLiteral("nextpiEnabledCheck"));
    nextpi_enabled_check_->setToolTip(
        tr("Boots NextPi under QEMU and connects it to UART 1, the Pi GPIO "
           "header's UART, so NextZXOS's Pi tools (.pisend, the NextPi UI) "
           "talk to it. Needs QEMU installed. Off by default."));
    form->addRow(QString(), nextpi_enabled_check_);

    auto path_row = [&](QLineEdit*& edit, const QString& object_name, const QString& placeholder,
                        bool directory) {
        auto* row = new QWidget(tab);
        auto* h = new QHBoxLayout(row);
        h->setContentsMargins(0, 0, 0, 0);
        edit = new QLineEdit(row);
        edit->setObjectName(object_name);
        edit->setPlaceholderText(placeholder);
        auto* browse_btn = new QPushButton(tr("Browse..."), row);
        h->addWidget(edit);
        h->addWidget(browse_btn);
        QLineEdit* target = edit;
        connect(browse_btn, &QPushButton::clicked, this, [this, target, directory]() {
            if (directory) {
                browse_directory(target);
            } else {
                const QString path = QFileDialog::getOpenFileName(
                    this, tr("Select QEMU"), target->text(), tr("All Files (*)"));
                if (!path.isEmpty()) target->setText(path);
            }
        });
        return row;
    };

    form->addRow(tr("NextPi directory:"),
                 path_row(nextpi_dir_edit_, QStringLiteral("nextpiDirEdit"),
                          QString::fromStdString(nextpi::default_dir()), true));

    nextpi_release_combo_ = new QComboBox(tab);
    nextpi_release_combo_->setObjectName(QStringLiteral("nextpiReleaseCombo"));
    nextpi_release_combo_->setEditable(true);
    nextpi_release_combo_->addItems({QString(), QString::fromLatin1(nextpi::kDefaultRelease),
                                     QString::fromLatin1(nextpi::kLatest)});
    nextpi_release_combo_->lineEdit()->setPlaceholderText(
        tr("%1 (default)").arg(QString::fromLatin1(nextpi::kDefaultRelease)));
    nextpi_release_combo_->setToolTip(
        tr("The NextPi release to download on first use: a release name from "
           "the mirror, or \"latest\" for the newest one listed there."));
    form->addRow(tr("NextPi release:"), nextpi_release_combo_);

    form->addRow(tr("QEMU:"),
                 path_row(nextpi_qemu_edit_, QStringLiteral("nextpiQemuEdit"),
                          tr("qemu-system-arm (from PATH)"), false));

    nextpi_audio_combo_ = new QComboBox(tab);
    nextpi_audio_combo_->setObjectName(QStringLiteral("nextpiAudioCombo"));
    nextpi_audio_combo_->setEditable(true);
    nextpi_audio_combo_->addItems({QString(), QStringLiteral("host"), QStringLiteral("none"),
                                   QStringLiteral("coreaudio"),
                                   QStringLiteral("pa"), QStringLiteral("pipewire"),
                                   QStringLiteral("alsa"), QStringLiteral("sdl"),
                                   QStringLiteral("dsound")});
    nextpi_audio_combo_->lineEdit()->setPlaceholderText(
        tr("the Next's mixer (default); \"host\": this computer's default output"));
    nextpi_audio_combo_->setToolTip(
        tr("Where the Pi's sound goes. Empty: into the Next's mixer, as on the "
           "real machine - NextREG 0xA2 controls it, and recordings include it. "
           "\"host\": straight to this computer's default output. "
           "Or a QEMU audio driver to play it straight to this computer, "
           "\"none\" to mute it, or wav:FILE to record it on its own."));
    form->addRow(tr("Pi audio:"), nextpi_audio_combo_);

    nextpi_qemu_args_edit_ = new QLineEdit(tab);
    nextpi_qemu_args_edit_->setObjectName(QStringLiteral("nextpiQemuArgsEdit"));
    nextpi_qemu_args_edit_->setPlaceholderText(
        tr("none - e.g. -serial telnet:127.0.0.1:2357,server,nowait"));
    nextpi_qemu_args_edit_->setToolTip(
        tr("Added after jnext's own QEMU arguments when NextPi starts. Split on "
           "spaces; '...' or \"...\" keep spaces in one argument; a backslash is an "
           "ordinary character. There is no shell: nothing is expanded."));
    form->addRow(tr("Extra QEMU arguments:"), nextpi_qemu_args_edit_);

    // Every other control follows the enable tick, so a disabled Pi does not
    // look half-configured.
    for (QWidget* w : {static_cast<QWidget*>(nextpi_dir_edit_->parentWidget()),
                       static_cast<QWidget*>(nextpi_release_combo_),
                       static_cast<QWidget*>(nextpi_qemu_edit_->parentWidget()),
                       static_cast<QWidget*>(nextpi_audio_combo_),
                       static_cast<QWidget*>(nextpi_qemu_args_edit_)}) {
        connect(nextpi_enabled_check_, &QCheckBox::toggled, w, &QWidget::setEnabled);
        w->setEnabled(false);   // the constructor's fill toggles it on
    }

    auto* note = new QLabel(
        tr("The first time NextPi starts, jnext offers to download NextPi "
           "(about 6 GB; about 22 GB free needed while it is unpacked) into the "
           "directory above, as it does for the SD card image.\n\n"
           "These settings are not applied to the machine that is already "
           "running: they take effect the next time jnext starts. --nextpi and "
           "--no-nextpi override the tick for a single run. "
           "The tick starts NextPi only when jnext starts on the ZX Next; "
           "--nextpi starts it with any machine. Changing the machine while "
           "jnext runs neither starts nor stops it."), tab);
    note->setWordWrap(true);
    form->addRow(QString(), note);

    return tab;
}

QWidget* PreferencesDialog::build_paths_tab() {
    auto* tab = new QWidget(this);
    auto* form = new QFormLayout(tab);

    auto add_path_row = [&](const QString& label, QLineEdit*& edit,
                             void (PreferencesDialog::*browse)(QLineEdit*)) {
        auto* row = new QWidget(tab);
        auto* h = new QHBoxLayout(row);
        h->setContentsMargins(0, 0, 0, 0);
        edit = new QLineEdit(row);
        auto* browse_btn = new QPushButton(tr("Browse..."), row);
        h->addWidget(edit);
        h->addWidget(browse_btn);
        connect(browse_btn, &QPushButton::clicked, this, [this, edit, browse]() {
            (this->*browse)(edit);
        });
        form->addRow(label, row);
    };

    add_path_row(tr("Last load directory:"), last_load_dir_edit_,
                 &PreferencesDialog::browse_directory);
    add_path_row(tr("Default SD card image:"), sd_card_path_edit_,
                 &PreferencesDialog::browse_sd_image);
    // GH #505 - empty = nothing in the second socket. NextZXOS maps it to the next
    // drive (D:) and finds it at start-up, so it takes effect at the next reset.
    add_path_row(tr("Second SD card image:"), sd_card2_path_edit_,
                 &PreferencesDialog::browse_sd_image);
    add_path_row(tr("Screenshot directory:"), screenshot_dir_edit_,
                 &PreferencesDialog::browse_directory);

#ifdef ENABLE_DEBUGGER
    // GH #420 - where the debugger's Source tab looks for the files a source map names.
    // One directory per line, as the ESP hosts are edited.
    {
        auto* row = new QWidget(tab);
        auto* v = new QVBoxLayout(row);
        v->setContentsMargins(0, 0, 0, 0);
        source_dirs_edit_ = new QPlainTextEdit(row);
        source_dirs_edit_->setObjectName(QStringLiteral("sourceDirsEdit"));
        source_dirs_edit_->setToolTip(
            tr("One directory per line. The Source tab tries them after the source map's own "
               "directory and its parents, longest matching part of the file name first. "
               "--source-dir overrides this list for a single run."));
        source_dirs_edit_->setFixedHeight(4 * source_dirs_edit_->fontMetrics().lineSpacing() + 12);
        auto* add = new QPushButton(tr("Add Directory..."), row);
        add->setObjectName(QStringLiteral("sourceDirsAddButton"));
        connect(add, &QPushButton::clicked, this, [this]() {
            const QString dir = QFileDialog::getExistingDirectory(this, tr("Select Directory"));
            if (dir.isEmpty()) return;
            QString text = source_dirs_edit_->toPlainText();
            if (!text.isEmpty() && !text.endsWith(QLatin1Char('\n'))) text += QLatin1Char('\n');
            source_dirs_edit_->setPlainText(text + dir);
        });
        v->addWidget(source_dirs_edit_);
        v->addWidget(add, 0, Qt::AlignLeft);
        form->addRow(tr("Source search path:"), row);
    }
#endif

    // GH #19 — File > Quick Screenshot. Its own directory, not the one above:
    // that one FOLLOWS the Save Screenshot dialog around, this one is chosen.
    add_path_row(tr("Quick screenshot directory:"), quick_screenshot_dir_edit_,
                 &PreferencesDialog::browse_directory);
    quick_screenshot_dir_edit_->setObjectName(QStringLiteral("quickScreenshotDirEdit"));
    quick_screenshot_dir_edit_->setPlaceholderText(
        QString::fromStdString(default_quick_screenshot_dir()));

    quick_screenshot_fmt_combo_ = new QComboBox(tab);
    quick_screenshot_fmt_combo_->setObjectName(QStringLiteral("quickScreenshotFormatCombo"));
    quick_screenshot_fmt_combo_->addItem(tr("PNG (composited picture)"),
                                         static_cast<int>(ScreenshotFormat::Png));
    quick_screenshot_fmt_combo_->addItem(tr("SCR (ULA screen memory)"),
                                         static_cast<int>(ScreenshotFormat::Scr));
    form->addRow(tr("Quick screenshot format:"), quick_screenshot_fmt_combo_);

    // GH #321 — how many Quick Save snapshots to keep in ~/.jnext/snapshots.
    quick_keep_spin_ = new QSpinBox(tab);
    quick_keep_spin_->setObjectName(QStringLiteral("quickKeepSpin"));
    quick_keep_spin_->setRange(0, 1000000);
    quick_keep_spin_->setSpecialValueText(tr("Unlimited"));
    quick_keep_spin_->setToolTip(tr("Quick Save deletes the oldest quick snapshots (files named "
                                    "jnext-<date>-<time>.jns) beyond this many. Other files in the "
                                    "folder are never touched. --quick-snapshot-keep overrides this."));
    form->addRow(tr("Quick snapshots to keep:"), quick_keep_spin_);

    auto* quick_note = new QLabel(
        host_shortcuts::quick_screenshot_note(), tab);
    quick_note->setWordWrap(true);
    form->addRow(QString(), quick_note);

    return tab;
}

void PreferencesDialog::browse_directory(QLineEdit* target) {
    QString dir = QFileDialog::getExistingDirectory(this, tr("Select Directory"), target->text());
    if (!dir.isEmpty()) target->setText(dir);
}

void PreferencesDialog::browse_sd_image(QLineEdit* target) {
    QString path = QFileDialog::getOpenFileName(
        this, tr("Select SD Card Image"), target->text(),
        tr("Disk Images (*.img *.bin *.raw);;All Files (*)"));
    if (!path.isEmpty()) target->setText(path);
}

// GH #420 - jnext.conf and --set hold the list as ONE string joined by the platform
// separator, so a directory that contains it would load back as two. Refused here as
// --source-dir refuses it.
bool PreferencesDialog::source_dirs_ok() {
    if (!source_dirs_edit_) return true;
    const QChar sep = QLatin1Char(source_search::kListSeparator);
    for (const QString& line : source_dirs_edit_->toPlainText().split(QLatin1Char('\n'))) {
        if (!line.contains(sep)) continue;
        QMessageBox::warning(this, tr("Source search path"),
                             tr("The directory \"%1\" contains '%2', which separates directories in "
                                "the saved list and in --set. Rename the directory or reach it through "
                                "a link.").arg(line.trimmed(), sep));
        return false;
    }
    return true;
}

void PreferencesDialog::watch_controls() {
    // Spin boxes and editable combo boxes have a QLineEdit inside, which is watched through
    // its owner: what counts is the value, not a half-typed text.
    const auto inside = [](const QWidget* w) {
        return qobject_cast<QAbstractSpinBox*>(w->parentWidget()) || qobject_cast<QComboBox*>(w->parentWidget());
    };
    const auto changed = [this]() { update_apply_button(); };
    for (QWidget* w : findChildren<QWidget*>()) {
        if (w->property("derived").toBool()) continue;   // shows other controls' state
        if (auto* combo = qobject_cast<QComboBox*>(w)) {
            connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, changed);
            connect(combo, &QComboBox::editTextChanged, this, changed);
        } else if (auto* spin = qobject_cast<QSpinBox*>(w)) {
            connect(spin, QOverload<int>::of(&QSpinBox::valueChanged), this, changed);
        } else if (auto* slider = qobject_cast<QSlider*>(w)) {
            connect(slider, &QSlider::valueChanged, this, changed);
        } else if (auto* edit = qobject_cast<QPlainTextEdit*>(w)) {
            connect(edit, &QPlainTextEdit::textChanged, this, changed);
        } else if (auto* line = qobject_cast<QLineEdit*>(w); line && !inside(line)) {
            connect(line, &QLineEdit::textChanged, this, changed);
        } else if (auto* button = qobject_cast<QAbstractButton*>(w); button && button->isCheckable()) {
            connect(button, &QAbstractButton::toggled, this, changed);
        } else {
            continue;
        }
        control_list_.append(w);
    }
}

PreferencesDialog::ControlState PreferencesDialog::control_state() const {
    ControlState st;
    for (const QWidget* w : control_list_) {
        if (auto* combo = qobject_cast<const QComboBox*>(w))
            st.values.append(combo->isEditable() ? QVariant(combo->currentText()) : QVariant(combo->currentIndex()));
        else if (auto* spin = qobject_cast<const QSpinBox*>(w))
            st.values.append(spin->value());
        else if (auto* slider = qobject_cast<const QSlider*>(w))
            st.values.append(slider->value());
        else if (auto* edit = qobject_cast<const QPlainTextEdit*>(w))
            st.values.append(edit->toPlainText());
        else if (auto* line = qobject_cast<const QLineEdit*>(w))
            st.values.append(line->text());
        else if (auto* button = qobject_cast<const QAbstractButton*>(w))
            st.values.append(button->isChecked());
    }
    st.keys = debug_keys_;
    return st;
}

void PreferencesDialog::mark_applied() {
    applied_ = control_state();
    update_apply_button();
}

void PreferencesDialog::update_apply_button() {
    if (!apply_button_) return;   // the tabs are still being built
    const ControlState now = control_state();
    apply_button_->setEnabled(now.values != applied_.values || now.keys != applied_.keys);
}

AppConfigData PreferencesDialog::collect() const {
    AppConfigData cfg;
    cfg.machine_type = static_cast<MachineType>(machine_combo_->currentData().toInt());
    cfg.cpu_speed    = static_cast<CpuSpeed>(cpu_speed_combo_->currentData().toInt());
    cfg.emulator_speed_percent = emu_speed_spin_->value();
    cfg.fast_boot_seconds = fast_boot_spin_->value();
    cfg.fast_boot_hide_screen = fast_boot_hide_check_->isChecked();
    cfg.ui_language = static_cast<ui_language::Choice>(language_combo_->currentData().toInt());
    cfg.when_slow_prefer = static_cast<audio_pacing::WhenSlowPrefer>(
        when_slow_combo_->currentData().toInt());
    cfg.window_scale_x2 = scale_combo_->currentData().toInt();
    cfg.mouse_release = static_cast<pointer_capture::ReleaseMethod>(
        mouse_release_combo_->currentData().toInt());
    cfg.windows_layout = static_cast<host_layout::Mode>(windows_layout_combo_->currentData().toInt());
    cfg.alt_digit_caps_shift = alt_digit_check_->isChecked();
    const int crt_choice = crt_combo_->currentData().toInt();
    cfg.crt_filter = crt_choice == CRT_SCANLINES;
    if (crt_combo_->findData(CRT_TV) >= 0) {
        cfg.crt_shader = crt_choice == CRT_TV || crt_choice == CRT_PC || crt_choice == CRT_SOFT;
        cfg.crt_shader_style = crt_choice == CRT_PC ? crt_shader::Style::Pc
                             : crt_choice == CRT_TV ? crt_shader::Style::Tv
                             : crt_choice == CRT_SOFT ? crt_shader::Style::Lite
                                                    : crt_shader_style_saved_;
    } else {
        // No shader choices here: hand the saved ones back, so nothing is lost when the file
        // moves to a build that has them. The pop-up may show a saved Soft scanlines as
        // Scanlines; only a change of the choice replaces what was saved.
        cfg.crt_shader_style = crt_shader_style_saved_;
        if (crt_choice == crt_initial_choice_) {
            cfg.crt_filter = crt_filter_saved_;
            cfg.crt_shader = crt_shader_saved_ && !crt_filter_saved_;   // both set: one effect, as before
        } else {
            // A saved Soft scanlines was shown as Scanlines: changing the choice replaces it.
            cfg.crt_shader = crt_shader_saved_ && !cfg.crt_filter &&
                             crt_shader_style_saved_ != crt_shader::Style::Lite;
        }
    }
    cfg.silent       = silent_check_->isChecked();
    cfg.tape_fast_load = tape_fast_check_->isChecked();
    cfg.tape_auto_play = tape_auto_check_->isChecked();
    cfg.audio_gain_db = static_cast<float>(audio_gain_slider_->value());
    cfg.audio_gain_beeper_db = static_cast<float>(audio_beeper_gain_slider_->value());
    for (int chip = 0; chip < 3; ++chip)
        cfg.audio_gain_ay_db[chip] =
            static_cast<float>(audio_ay_gain_slider_[chip]->value());
    cfg.audio_gain_dac_db = static_cast<float>(audio_dac_gain_slider_->value());
    cfg.audio_oversample = audio_oversample_check_->isChecked();
    cfg.audio_channels.ay_stereo = static_cast<AyStereoOrder>(ay_stereo_combo_->currentData().toInt());
    for (int chip = 0; chip < 3; ++chip) cfg.audio_channels.ay_mono[chip] = ay_mono_check_[chip]->isChecked();
    for (int ch = 0; ch < DAC_MIX_CHANNELS; ++ch) {
        cfg.audio_channels.dac_gain_db[ch] = static_cast<float>(dac_channel_slider_[ch]->value());
        cfg.audio_channels.dac_pan[ch] = static_cast<DacPan>(dac_pan_group_[ch]->checkedId());
    }
    QComboBox* const joy_combo[2] = { joy1_source_combo_, joy2_source_combo_ };
    for (int i = 0; i < 2; ++i) {
        const QString key = joy_combo[i]->currentData().toString();
        cfg.joy_source[i] = key == QLatin1String("keys") ? JoySource::CursorKeys
                          : key == QLatin1String("none") ? JoySource::None
                                                         : JoySource::Sdl;
        if (key.startsWith(QLatin1String("dev:"))) {
            cfg.joy_device[i]      = key.mid(4);
            cfg.joy_device_name[i] = joy_combo[i]->currentData(Qt::UserRole + 1).toString();
        }
    }
    cfg.esp_enabled = esp_enabled_check_->isChecked();
    cfg.nextpi_enabled     = nextpi_enabled_check_->isChecked();
    cfg.nextpi_dir         = nextpi_dir_edit_->text().trimmed();
    cfg.nextpi_release     = nextpi_release_combo_->currentText().trimmed();
    cfg.nextpi_qemu_binary = nextpi_qemu_edit_->text().trimmed();
    cfg.nextpi_audio       = nextpi_audio_combo_->currentText().trimmed();
    cfg.nextpi_qemu_args   = nextpi_qemu_args_edit_->text().trimmed();
    {
        // Through EspHostPolicy::add, exactly as AppConfig::load() and the CLI
        // do: blanks (every stray newline in a text box is one) dropped,
        // duplicates collapsed. A security control must not mean three
        // different things depending on where the value was typed.
        //
        // A COMMA SEPARATES TOO, not just a newline. `allowed_hosts` in
        // ~/.jnext/jnext.conf is a comma-separated list (QSettings writes
        // string lists that way), so a user who has read that file — or the man
        // page's FILES section — will paste a comma-separated line in here.
        // Taking it literally would store one unmatchable host name and refuse
        // every connection while displaying what looks like a correct list. No
        // legal host name contains a comma, so splitting is unambiguous. The
        // CLI, where the option is one-host-per-occurrence, rejects a comma
        // loudly instead — see main.cpp's EspAllow case.
        EspHostPolicy policy;
        const QStringList entries = esp_hosts_edit_->toPlainText().split(
            QRegularExpression(QStringLiteral("[\n,]")));
        for (const QString& entry : entries) policy.add(entry.trimmed().toStdString());
        cfg.esp_allowed_hosts = policy.allowed_hosts;
    }
    cfg.last_load_dir  = last_load_dir_edit_->text();
    cfg.sd_card_path   = sd_card_path_edit_->text();
    cfg.sd_card2_path  = sd_card2_path_edit_->text();
    cfg.screenshot_dir = screenshot_dir_edit_->text();
    // GH #420 - trimmed, blank lines and exact duplicates dropped, order kept. No
    // existence check: a missing directory just finds nothing.
    cfg.source_dirs = source_dirs_saved_;
    if (source_dirs_edit_) {
        cfg.source_dirs.clear();
        for (const QString& line : source_dirs_edit_->toPlainText().split(QLatin1Char('\n'))) {
            const QString dir = line.trimmed();
            if (!dir.isEmpty() && !cfg.source_dirs.contains(dir)) cfg.source_dirs << dir;
        }
    }
    cfg.source_scheme = source_scheme_combo_
        ? static_cast<SourceScheme>(source_scheme_combo_->currentData().toInt())
        : source_scheme_;   // GH #422 stage 2 - no control: the value given
    cfg.recent_files = recent_files_;   // GH #406 - kept by File > Recent Files, no control here
    cfg.debug_keys = debug_keys_;   // GH #1 — including the preserved unknowns
    cfg.quick_screenshot_dir = quick_screenshot_dir_edit_->text();
    cfg.quick_screenshot_format = static_cast<ScreenshotFormat>(
        quick_screenshot_fmt_combo_->currentData().toInt());
    cfg.quick_snapshot_keep = quick_keep_spin_->value();
    return cfg;
}

#ifdef ENABLE_DEBUGGER
// ---------------------------------------------------------------------------
// GH #422 stage 2 - Debugger
// ---------------------------------------------------------------------------

QWidget* PreferencesDialog::build_debugger_tab() {
    auto* tab  = new QWidget(this);
    auto* form = new QFormLayout(tab);
    source_scheme_combo_ = new QComboBox(tab);
    source_scheme_combo_->setObjectName(QStringLiteral("sourceSchemeCombo"));
    source_scheme_combo_->setToolTip(
        tr("The Source tab's background, text and syntax colours. "
           "--set source.colour_scheme=WORD sets it for one run."));
    // Item data is the SourceScheme value, which is also the index of the config word.
    source_scheme_combo_->addItem(tr("Auto (follow the application)"), static_cast<int>(SourceScheme::Auto));
    source_scheme_combo_->addItem(tr("Light"), static_cast<int>(SourceScheme::Light));
    source_scheme_combo_->addItem(tr("Dark"), static_cast<int>(SourceScheme::Dark));
    source_scheme_combo_->addItem(tr("Borland C++ 3.1"), static_cast<int>(SourceScheme::Borland));
    form->addRow(tr("Source tab colours:"), source_scheme_combo_);
    return tab;
}

// ---------------------------------------------------------------------------
// GH #1 — Debugger Keys
// ---------------------------------------------------------------------------

QWidget* PreferencesDialog::build_debug_keys_tab(
    const std::vector<jnext::dbgkeys::LoadIssue>& key_issues) {
    using namespace jnext::dbgkeys;

    auto* tab    = new QWidget(this);
    auto* outer  = new QVBoxLayout(tab);

    auto* intro = new QLabel(
        tr("Click a shortcut to change it, then press the key combination you "
           "want. Only F1-F12 may be used on their own; anything else needs "
           "Ctrl, Alt or Meta, because a bare key would be taken away from "
           "whichever debugger panel has focus.\n"
           "Only the bindings you change are written to the configuration "
           "file."), tab);
    intro->setWordWrap(true);
    outer->addWidget(intro);

    // Everything AppConfig refused in [debugger_keys], shown where the user can
    // act on it. The log says the same thing at startup; a config error that
    // only ever appears on stderr is one a GUI user never sees.
    if (!key_issues.empty()) {
        QStringList lines;
        for (const auto& i : key_issues) {
            lines << tr("%1 = %2 - %3")
                         .arg(QString::fromStdString(i.action_id).toHtmlEscaped(),
                              QString::fromStdString(i.text).toHtmlEscaped(),
                              QString::fromStdString(i.reason).toHtmlEscaped());
        }
        auto* bad = new QLabel(
            tr("<b>Problems found in the configuration file:</b><br>%1")
                .arg(lines.join(QStringLiteral("<br>"))),
            tab);
        bad->setWordWrap(true);
        palette_tint::follow(bad, [](const QPalette& p) {   // GH #363: legible on dark, live
            return "QLabel { color: " + palette_tint::css(palette_tint::ink(p, QColor(0xB0, 0x00, 0x00))) +
                   "; }";
        });
        outer->addWidget(bad);
    }

    auto* grid_host = new QWidget(tab);
    auto* grid = new QGridLayout(grid_host);
    grid->setColumnStretch(0, 1);
    for (int i = 0; i < ACTION_COUNT; ++i) {
        const ActionInfo& a = info(static_cast<Action>(i));
        grid->addWidget(new QLabel(tr(a.label), grid_host), i, 0);

        key_buttons_[i] = new ShortcutCaptureButton(grid_host);
        key_buttons_[i]->setMinimumWidth(160);
        connect(key_buttons_[i], &ShortcutCaptureButton::combo_captured, this,
                [this, i](Combo c) { on_key_captured(i, c); });
        connect(key_buttons_[i], &ShortcutCaptureButton::combo_refused, this,
                [this](const QString& why) {
                    if (key_message_) key_message_->setText(why);
                });
        grid->addWidget(key_buttons_[i], i, 1);

        auto* reset = new QPushButton(tr("Reset"), grid_host);
        connect(reset, &QPushButton::clicked, this, [this, i]() { reset_key(i); });
        grid->addWidget(reset, i, 2);

        refresh_key_row(i);
    }

    // The table is taller than the other tabs, and the dialog must stay usable
    // on a small screen.
    auto* scroll = new QScrollArea(tab);
    scroll->setWidget(grid_host);
    scroll->setWidgetResizable(true);
    outer->addWidget(scroll, 1);

    auto* row = new QHBoxLayout();
    auto* reset_all = new QPushButton(tr("Reset All to Defaults"), tab);
    connect(reset_all, &QPushButton::clicked, this, [this]() {
        debug_keys_.reset_all();
        for (int i = 0; i < ACTION_COUNT; ++i) refresh_key_row(i);
        if (key_message_)
            key_message_->setText(tr("All debugger keys reset to their defaults."));
    });
    row->addWidget(reset_all);
    row->addStretch(1);
    outer->addLayout(row);

    key_message_ = new QLabel(tab);
    // Named so a test can find THIS label rather than guessing at the first
    // word-wrapping one — the Input tab has help text that also wraps.
    key_message_->setObjectName(QStringLiteral("debug_keys_message"));
    key_message_->setWordWrap(true);
    outer->addWidget(key_message_);

    return tab;
}

void PreferencesDialog::refresh_key_row(int index) {
    using namespace jnext::dbgkeys;
    if (index < 0 || index >= ACTION_COUNT || !key_buttons_[index]) return;
    key_buttons_[index]->set_combo(debug_keys_.combo(static_cast<Action>(index)));
    update_apply_button();
}

void PreferencesDialog::on_key_captured(int index, jnext::dbgkeys::Combo combo) {
    using namespace jnext::dbgkeys;
    const Action target = static_cast<Action>(index);

    if (const ActionInfo* owner = debug_keys_.action_for(combo)) {
        if (owner->action != target) {
            // Refused, not "last one wins". Qt makes two identical sequences
            // AMBIGUOUS and fires them round-robin, so allowing this would
            // break the binding the user just made AND the one they forgot
            // about (GH #124). The old value goes back on the button.
            if (key_message_) {
                key_message_->setText(
                    tr("%1 is already bound to \"%2\". Change that one first.")
                        .arg(QString::fromStdString(render_combo(combo)),
                             tr(owner->label)));
            }
            refresh_key_row(index);
            return;
        }
    }

    debug_keys_.set(target, combo);
    refresh_key_row(index);
    if (!key_message_) return;

    // Accepted — now say whether anything else in jnext answers to it.
    //
    // A chord the EMULATOR window claims is NOT refused, and the reason is
    // measured rather than assumed: the two windows are separate top levels,
    // so Qt::WindowShortcut matches against whichever is ACTIVE and each one
    // keeps its own binding. There is no ambiguity and nothing breaks — the
    // debugger command works in the debugger window, and the emulator window
    // goes on doing what it did. Refusing this class would make F11 unbindable,
    // and F11 = Step Into is precisely what GH #1's reporter asked for.
    //
    // What WAS wrong is that this said nothing, so a user who bound Run to
    // Ctrl+F5 was told "Run is now Ctrl+F5" and then found the emulator window
    // starting a recording. The collision is named, with both outcomes.
    if (const HostChord* h = find_host_chord(host_chords_, combo)) {
        key_message_->setText(
            tr("\"%1\" is now %2 - but the emulator window already uses %2 for "
               "\"%3\". It will work in the debugger window; in the emulator "
               "window %2 still does \"%3\".")
                .arg(tr(info(target).label),
                     QString::fromStdString(render_combo(combo)),
                     h->label));
        return;
    }

    key_message_->setText(tr("\"%1\" is now %2.")
                              .arg(tr(info(target).label),
                                   QString::fromStdString(render_combo(combo))));
}

void PreferencesDialog::reset_key(int index) {
    using namespace jnext::dbgkeys;
    if (index < 0 || index >= ACTION_COUNT) return;
    const Action target = static_cast<Action>(index);
    const Combo def = info(target).def;

    // A reset can collide too: if the user moved another action onto this
    // one's default, restoring it would create exactly the ambiguity the
    // capture path refuses. Same answer, same reason.
    if (const ActionInfo* owner = debug_keys_.action_for(def)) {
        if (owner->action != target) {
            if (key_message_) {
                key_message_->setText(
                    tr("Cannot restore %1 for \"%2\": it is bound to \"%3\".")
                        .arg(QString::fromStdString(render_combo(def)),
                             tr(info(target).label), tr(owner->label)));
            }
            return;
        }
    }

    debug_keys_.reset(target);
    refresh_key_row(index);
    if (key_message_) {
        key_message_->setText(tr("\"%1\" reset to %2.")
                                  .arg(tr(info(target).label),
                                       QString::fromStdString(render_combo(def))));
    }
}
#endif  // ENABLE_DEBUGGER
