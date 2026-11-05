#pragma once

#include <QObject>
#include <QPointer>
#include <QTranslator>
#include <memory>

class QQmlEngine;

namespace EasePass::Core { class AppSettings; }

namespace EasePass::App {

/**
 * UI language (steps.txt T20; C#: Manager/LocalizationManager.cs). Loads
 * ":/i18n/easepass_<lang>.qm" for AppSettings::language (en-US, de-DE, pt-BR) plus Qt's own
 * translations when the system has them, sets QLocale::setDefault and retranslates the QML
 * engine, so changing the language in the settings takes effect immediately (C# needed a
 * restart). Missing translations fall back to the English source texts; an unknown language
 * tag falls back to English.
 *
 * Strings that C++ already formatted (e.g. an open info message) keep their language until
 * they are produced again.
 */
class LanguageManager : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString language READ language NOTIFY languageChanged)
public:
    explicit LanguageManager(Core::AppSettings *settings, QObject *parent = nullptr);
    ~LanguageManager() override;

    /// QML engine to retranslate on changes (set once it exists).
    void setEngine(QQmlEngine *engine);

    QString language() const { return m_language; }
    /// Tags with a bundled translation file (en-US, de-DE, pt-BR).
    static QStringList availableLanguages();
    /// "de-DE" -> "de", "pt-BR" -> "pt_BR", "en-US" -> "en"; empty for unknown tags.
    static QString qmSuffix(const QString &tag);

    /// Applies @p tag (normally driven by AppSettings::languageChanged). Returns false if no
    /// translation file was found (English is used then).
    bool apply(const QString &tag);

signals:
    void languageChanged();

private:
    QPointer<Core::AppSettings> m_settings;
    QPointer<QQmlEngine> m_engine;
    std::unique_ptr<QTranslator> m_appTranslator;
    std::unique_ptr<QTranslator> m_qtTranslator;
    QString m_language;
};

} // namespace EasePass::App
