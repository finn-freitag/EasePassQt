#pragma once

#include <QObject>

namespace EasePass::Core { class AppSettings; }

namespace EasePass::App {

/**
 * Settings page view model (steps.txt T12). AppSettings is already a QObject with
 * validated (clamped) Q_PROPERTYs, so QML binds to `settings` directly; this class adds the
 * limits the UI needs for its spin boxes and the reset actions.
 * C#: Views/SettingsPage.xaml.cs.
 */
class SettingsViewModel : public QObject {
    Q_OBJECT
    Q_PROPERTY(EasePass::Core::AppSettings *settings READ settings CONSTANT)
    Q_PROPERTY(int minPasswordLength READ minPasswordLength CONSTANT)
    Q_PROPERTY(int maxPasswordLength READ maxPasswordLength CONSTANT)
    Q_PROPERTY(int minInactivityMinutes READ minInactivityMinutes CONSTANT)
    Q_PROPERTY(int maxInactivityMinutes READ maxInactivityMinutes CONSTANT)
    Q_PROPERTY(bool passwordCharsValid READ passwordCharsValid NOTIFY passwordCharsValidChanged)
public:
    explicit SettingsViewModel(Core::AppSettings *settings, QObject *parent = nullptr);

    Core::AppSettings *settings() const { return m_settings; }
    static int minPasswordLength() { return 8; }
    static int maxPasswordLength() { return 999; }
    static int minInactivityMinutes() { return 1; }
    static int maxInactivityMinutes() { return 1000; }
    bool passwordCharsValid() const;

    Q_INVOKABLE void resetToDefaults();
    Q_INVOKABLE void resetPasswordChars();
    Q_INVOKABLE void resetPasswordLength();

signals:
    void passwordCharsValidChanged();

private:
    Core::AppSettings *m_settings;
};

} // namespace EasePass::App
