#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>

namespace EasePass::Core {
class AppSettings;
class DatabaseService;
} // namespace EasePass::Core

namespace EasePass::App {

/**
 * Settings page view model (steps.txt T12, T17). AppSettings is already a QObject with
 * validated (clamped) Q_PROPERTYs, so QML binds to `settings` directly; this class adds the
 * limits the UI needs for its spin boxes, the language list, the C# text-field semantics of
 * the generator settings and the reset actions.
 * C#: Views/SettingsPage.xaml.cs.
 */
class SettingsViewModel : public QObject {
    Q_OBJECT
    Q_PROPERTY(EasePass::Core::AppSettings *settings READ settings CONSTANT)
    Q_PROPERTY(int minPasswordLength READ minPasswordLength CONSTANT)
    Q_PROPERTY(int maxPasswordLength READ maxPasswordLength CONSTANT)
    Q_PROPERTY(int minInactivityMinutes READ minInactivityMinutes CONSTANT)
    Q_PROPERTY(int maxInactivityMinutes READ maxInactivityMinutes CONSTANT)
    Q_PROPERTY(int minClipboardSeconds READ minClipboardSeconds CONSTANT)
    Q_PROPERTY(int maxClipboardSeconds READ maxClipboardSeconds CONSTANT)
    Q_PROPERTY(int minSaveDelaySeconds READ minSaveDelaySeconds CONSTANT)
    Q_PROPERTY(int maxSaveDelaySeconds READ maxSaveDelaySeconds CONSTANT)
    Q_PROPERTY(bool passwordCharsValid READ passwordCharsValid NOTIFY passwordCharsValidChanged)
    Q_PROPERTY(QVariantList languages READ languages CONSTANT)
    Q_PROPERTY(int languageIndex READ languageIndex WRITE setLanguageIndex NOTIFY languageChanged)
    Q_PROPERTY(bool restartRequired READ restartRequired NOTIFY languageChanged)
    Q_PROPERTY(bool canResetPopularity READ canResetPopularity NOTIFY canResetPopularityChanged)
public:
    explicit SettingsViewModel(Core::AppSettings *settings, QObject *parent = nullptr);

    /// Needed for "Reset popularity" (works on the open database).
    void setDatabaseService(Core::DatabaseService *service);

    Core::AppSettings *settings() const { return m_settings; }
    static int minPasswordLength() { return 8; }
    static int maxPasswordLength() { return 999; }
    static int minInactivityMinutes() { return 1; }
    static int maxInactivityMinutes() { return 1000; }
    // C#: NumberBox clipboardClearTimeout Minimum=1 Maximum=100000
    static int minClipboardSeconds() { return 1; }
    static int maxClipboardSeconds() { return 100000; }
    static int minSaveDelaySeconds() { return 1; }
    static int maxSaveDelaySeconds() { return 3600; }
    bool passwordCharsValid() const;

    /// {tag, name} per available UI language (C#: Strings/LanguageList.resw; names are native).
    static QVariantList languages();
    int languageIndex() const;
    void setLanguageIndex(int index);
    /// The language differs from the one the app started with (C#: "To apply please restart the app").
    bool restartRequired() const;
    bool canResetPopularity() const;

    /**
     * C#: pswd_length_TextChanging/TextChanged: non-digits are dropped; empty or "0" restores the
     * default, other values are raised to the minimum (and capped at the maximum).
     * Returns the digits-only text the field should show.
     */
    Q_INVOKABLE QString applyPasswordLengthText(const QString &text);
    /// C#: pswd_chars_TextChanged - empty text stores the default characters.
    Q_INVOKABLE void applyPasswordCharsText(const QString &text);

    Q_INVOKABLE void resetToDefaults();
    Q_INVOKABLE void resetPasswordChars();
    Q_INVOKABLE void resetPasswordLength();
    /// C#: ResetPopularity_Click - clears the click history of all items. Emits message().
    Q_INVOKABLE bool resetPopularity();

signals:
    void passwordCharsValidChanged();
    void languageChanged();
    void canResetPopularityChanged();
    /// Severity as Theme.Severity (0 info, 1 success, 2 warning, 3 error).
    void message(int severity, const QString &text);

private:
    Core::AppSettings *m_settings;
    Core::DatabaseService *m_service = nullptr;
    QString m_startLanguage;
};

} // namespace EasePass::App
