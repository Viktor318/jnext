#include "gui/ui_language.h"

#include "core/log.h"

#include <QCoreApplication>
#include <QLocale>
#include <QTranslator>

// The catalogues live in this static library. Nothing else refers to the
// generated qrc object, so the linker would drop it (and its registration)
// unless this call pulls it in. Q_INIT_RESOURCE must be called from the
// global namespace.
static void init_ui_language_resources() {
    Q_INIT_RESOURCE(jnext_i18n);
}

namespace ui_language {

namespace {
Choice g_session_choice = Choice::Auto;
}

QString resolve(Choice choice, const QStringList& system_ui_languages) {
    switch (choice) {
    case Choice::English:   return QStringLiteral("en");
    case Choice::Hungarian: return QStringLiteral("hu");
    case Choice::Auto:      break;
    }
    const QString first = system_ui_languages.value(0).toLower();
    if (first == QLatin1String("hu") || first.startsWith(QLatin1String("hu-"))
        || first.startsWith(QLatin1String("hu_")))
        return QStringLiteral("hu");
    return QStringLiteral("en");
}

void set_session_choice(Choice choice) { g_session_choice = choice; }
Choice session_choice() { return g_session_choice; }

QString install(QCoreApplication& app) {
    const QString lang = resolve(g_session_choice, QLocale::system().uiLanguages());
    if (lang == QLatin1String("en")) return lang;

    init_ui_language_resources();
    // Qt's own catalogue first, jnext's second: the last one installed is
    // asked first, and jnext's never holds a Qt context, so the order only
    // matters for clarity.
    auto* qt_tr = new QTranslator(&app);
    if (qt_tr->load(QStringLiteral(":/i18n/qtbase_") + lang + QStringLiteral(".qm")))
        app.installTranslator(qt_tr);
    else
        Log::platform()->warn("ui language: Qt's {} catalogue did not load", lang.toStdString());

    auto* own_tr = new QTranslator(&app);
    if (!own_tr->load(QStringLiteral(":/i18n/jnext_") + lang + QStringLiteral(".qm"))) {
        Log::platform()->warn("ui language: jnext's {} catalogue did not load; using English",
                              lang.toStdString());
        return QStringLiteral("en");
    }
    app.installTranslator(own_tr);
    Log::platform()->info("ui language: {}", lang.toStdString());
    return lang;
}

}  // namespace ui_language
