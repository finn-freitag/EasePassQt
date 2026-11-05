#pragma once

#include <QObject>
#include <QPointer>
#include <QVariantList>
#include <functional>

namespace EasePass::Core {
class AppSettings;
class DatabaseService;
class LeakCheckService;
} // namespace EasePass::Core

namespace EasePass::App {

/**
 * Password generator (steps.txt T15; C#: Dialogs/GenPasswordDialog, Views/DialogPages/
 * GenPasswordPage, Helper/Security/Generator/PasswordHelper.GeneratePassword).
 * generate() uses the stored generator settings (AppSettings passwordLength/passwordChars)
 * and, when "verify not leaked" is on, draws again while the leak check reports the password
 * as leaked (an unreachable service counts as not leaked, like C#'s IsPwned() == null).
 * safetyChecks feeds PasswordSafetyChart for the current password (generated or typed).
 */
class PasswordGeneratorViewModel : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString password READ password WRITE setPassword NOTIFY passwordChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QVariantList safetyChecks READ safetyChecks NOTIFY safetyChanged)
public:
    enum class LeakResult { NotLeaked, Leaked, Unknown };
    /// Asynchronous leak check; the callback may run synchronously.
    using LeakChecker = std::function<void(const QString &password, std::function<void(LeakResult)> done)>;

    /// After this many leaked candidates the last one is used anyway (C# loops forever).
    static constexpr int MaxLeakRetries = 20;

    PasswordGeneratorViewModel(Core::AppSettings *settings, Core::DatabaseService *service,
                               Core::LeakCheckService *leak, QObject *parent = nullptr);

    /// Replaces the leak check (tests); an empty checker disables it.
    void setLeakChecker(LeakChecker checker);

    QString password() const { return m_password; }
    void setPassword(const QString &password);
    bool busy() const { return m_busy; }
    QVariantList safetyChecks() const { return m_safetyChecks; }
    /// Candidates drawn by the last generate() (tests).
    int lastAttempts() const { return m_attempts; }

    Q_INVOKABLE void generate();

signals:
    void passwordChanged();
    void busyChanged();
    void safetyChanged();
    /// generate() finished with @p password (also set as password).
    void generated(const QString &password);

private:
    bool leakCheckEnabled() const;
    void attempt();
    void finish(const QString &password, LeakResult leak);
    void updateSafety(const QVariant &leaked);

    QPointer<Core::AppSettings> m_settings;
    Core::DatabaseService *m_service;
    LeakChecker m_checker;
    QString m_password;
    bool m_busy = false;
    int m_attempts = 0;
    quint64 m_generation = 0;
    QVariantList m_safetyChecks;
};

} // namespace EasePass::App
