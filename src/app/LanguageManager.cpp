#include "LanguageManager.h"

#include "AppSettings.h"

#include <QCoreApplication>
#include <QLibraryInfo>
#include <QLocale>
#include <QLoggingCategory>
#include <QQmlEngine>

Q_LOGGING_CATEGORY(lcLanguage, "easepass.app.language")

namespace EasePass::App {

LanguageManager::LanguageManager(Core::AppSettings *settings, QObject *parent)
    : QObject(parent), m_settings(settings) {
    if (m_settings) {
        connect(m_settings, &Core::AppSettings::languageChanged, this, [this](const QString &tag) { apply(tag); });
        apply(m_settings->language());
    } else {
        apply(QStringLiteral("en-US"));
    }
}

LanguageManager::~LanguageManager() {
    if (m_appTranslator)
        QCoreApplication::removeTranslator(m_appTranslator.get());
    if (m_qtTranslator)
        QCoreApplication::removeTranslator(m_qtTranslator.get());
}

void LanguageManager::setEngine(QQmlEngine *engine) {
    m_engine = engine;
    if (m_engine)
        m_engine->retranslate();
}

QStringList LanguageManager::availableLanguages() {
    return {QStringLiteral("en-US"), QStringLiteral("de-DE"), QStringLiteral("pt-BR")};
}

QString LanguageManager::qmSuffix(const QString &tag) {
    const QString t = tag.trimmed().toLower().replace(QLatin1Char('_'), QLatin1Char('-'));
    if (t == QLatin1String("de-de") || t == QLatin1String("de"))
        return QStringLiteral("de");
    if (t == QLatin1String("pt-br") || t == QLatin1String("pt"))
        return QStringLiteral("pt_BR");
    if (t == QLatin1String("en-us") || t == QLatin1String("en"))
        return QStringLiteral("en");
    return QString();
}

bool LanguageManager::apply(const QString &requested) {
    QString suffix = qmSuffix(requested);
    QString tag = requested;
    if (suffix.isEmpty()) {
        qCWarning(lcLanguage) << "unknown language" << requested << "- using English";
        suffix = QStringLiteral("en");
        tag = QStringLiteral("en-US");
    }

    if (m_appTranslator)
        QCoreApplication::removeTranslator(m_appTranslator.get());
    if (m_qtTranslator)
        QCoreApplication::removeTranslator(m_qtTranslator.get());

    const QLocale locale(QString(tag).replace(QLatin1Char('-'), QLatin1Char('_')));
    QLocale::setDefault(locale);

    m_appTranslator = std::make_unique<QTranslator>();
    const bool loaded = m_appTranslator->load(QStringLiteral(":/i18n/easepass_") + suffix);
    if (loaded)
        QCoreApplication::installTranslator(m_appTranslator.get());
    else
        qCWarning(lcLanguage) << "no translation file for" << tag;

    // Qt's own strings (standard dialog buttons etc.), if the system ships them.
    m_qtTranslator = std::make_unique<QTranslator>();
    if (m_qtTranslator->load(locale, QStringLiteral("qtbase"), QStringLiteral("_"),
                             QLibraryInfo::path(QLibraryInfo::TranslationsPath)))
        QCoreApplication::installTranslator(m_qtTranslator.get());

    const bool changed = tag != m_language;
    m_language = tag;
    if (m_engine)
        m_engine->retranslate();
    if (changed)
        emit languageChanged();
    return loaded;
}

} // namespace EasePass::App
