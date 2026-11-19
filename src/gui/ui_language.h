#pragma once

// The language of the Qt window and the debugger (startup.language).
//
// The setting is a launch setting, like startup.fast_boot_seconds: it is read
// once, before the first QApplication exists, and a change in Preferences
// takes effect at the next start. Every QApplication the process makes (the
// SD-card download dialogs' temporary one and the session's own) calls
// install() right after it is constructed, so both speak the same language.
//
// The translations are compiled into the binary (translations/jnext_i18n.qrc):
// jnext's own catalogue and Qt's own (qtbase_<lang>.qm, the standard dialogs'
// buttons and the file dialog). English is the source language and needs
// neither.

#include <QString>
#include <QStringList>

#include <cstdint>

class QCoreApplication;

namespace ui_language {

/// The choice as stored. The order is the order of the config_keys words
/// "auto|en|hu" (app_config.cpp static_asserts it).
enum class Choice : uint8_t { Auto, English, Hungarian };

/// The language code a choice gives: "en" or "hu". Auto follows the first
/// entry of `system_ui_languages` (QLocale::system().uiLanguages(), e.g.
/// "hu-HU"): Hungarian when it is Hungarian, English otherwise.
QString resolve(Choice choice, const QStringList& system_ui_languages);

/// The choice of this run (the saved one with --set over it). main() sets it
/// before any QApplication is constructed; the default is Auto.
void set_session_choice(Choice choice);
Choice session_choice();

/// Install the translators of the session choice on `app`. The translators
/// are children of `app` and go with it. Returns the language code in effect
/// ("en" when nothing had to be installed or a catalogue failed to load).
QString install(QCoreApplication& app);

}  // namespace ui_language
