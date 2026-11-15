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
 *
 * Plugins of type password-generator are offered next to the built-in generator
 * (setExternalGenerators; the choice is stored in AppSettings::passwordGeneratorPlugin).
 * What a plugin returns is untrusted: it is checked for leaks and rated like any other
 * password, and a failing plugin is reported (errorMessage, failed) - the built-in
 * generator never silently takes over.
 */
class PasswordGeneratorViewModel : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString password READ password WRITE setPassword NOTIFY passwordChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QVariantList safetyChecks READ safetyChecks NOTIFY safetyChanged)
    Q_PROPERTY(QVariantList generators READ generators NOTIFY generatorsChanged)
    Q_PROPERTY(QString generatorId READ generatorId WRITE setGeneratorId NOTIFY generatorIdChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorMessageChanged)
public:
    enum class LeakResult { NotLeaked, Leaked, Unknown };
    /// Asynchronous leak check; the callback may run synchronously.
    using LeakChecker = std::function<void(const QString &password, std::function<void(LeakResult)> done)>;

    /// A generator that is not built in (a password-generator plugin).
    struct ExternalGenerator {
        using Done = std::function<void(const QString &password, const QString &error)>;
        QString id;
        QString name;
        /// Asynchronous; `error` is not empty if no password could be generated.
        std::function<void(int length, const QString &characters, bool verifyNotLeaked, Done done)> generate;
    };
    static constexpr int MaxExternalPasswordLength = 4096;

    /// After this many leaked candidates the last one is used anyway (C# loops forever).
    static constexpr int MaxLeakRetries = 20;

    PasswordGeneratorViewModel(Core::AppSettings *settings, Core::DatabaseService *service,
                               Core::LeakCheckService *leak, QObject *parent = nullptr);

    /// Replaces the leak check (tests); an empty checker disables it.
    void setLeakChecker(LeakChecker checker);

    void setExternalGenerators(QList<ExternalGenerator> generators);
    /// {id, name}; the first entry is the built-in generator (id "").
    QVariantList generators() const;
    /// Id of the generator in use; "" (built-in) if the stored one is not available.
    QString generatorId() const;
    void setGeneratorId(const QString &id);
    QString errorMessage() const { return m_errorMessage; }

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
    /// generate() could not produce a password (only with a plugin generator).
    void failed(const QString &message);
    void generatorsChanged();
    void generatorIdChanged();
    void errorMessageChanged();

private:
    bool leakCheckEnabled() const;
    void attempt();
    void check(const QString &candidate);
    void fail(const QString &message);
    void setErrorMessage(const QString &message);
    const ExternalGenerator *external() const;
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
    QList<ExternalGenerator> m_external;
    QString m_generatorId; ///< only used without settings
    QString m_errorMessage;
};

} // namespace EasePass::App
