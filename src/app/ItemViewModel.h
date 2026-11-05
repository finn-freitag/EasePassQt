#pragma once

#include "PasswordItem.h"

#include <QObject>
#include <QPointer>
#include <QStringList>
#include <QVariantList>

namespace EasePass::Core {
class AppSettings;
class DatabaseService;
class IClock;
class LeakCheckService;
} // namespace EasePass::Core

namespace EasePass::App {

class ClipboardService;
class TotpTimerService;

/**
 * Edit buffer for one password item (steps.txt T12). load(index) copies the item into the
 * buffer, the setters only change the buffer, commit() validates and writes it back
 * (add for new items), cancel() restores the loaded state.
 * Also: password strength checks for PasswordSafetyChart (8 entries, see safetyChecks), HIBP
 * leak state, and the TOTP code/remaining time driven by the shared TotpTimerService.
 * C#: Views/PasswordsPage (item view), Dialogs/AddItemPage, Controls/PasswordSafetyChart.
 */
class ItemViewModel : public QObject {
    Q_OBJECT
    Q_PROPERTY(int index READ index NOTIFY loadedChanged)
    Q_PROPERTY(bool isNew READ isNew NOTIFY loadedChanged)
    Q_PROPERTY(bool hasItem READ hasItem NOTIFY loadedChanged)
    Q_PROPERTY(QString displayName READ displayName WRITE setDisplayName NOTIFY bufferChanged)
    Q_PROPERTY(QString username READ username WRITE setUsername NOTIFY bufferChanged)
    Q_PROPERTY(QString email READ email WRITE setEmail NOTIFY bufferChanged)
    Q_PROPERTY(QString password READ password WRITE setPassword NOTIFY bufferChanged)
    Q_PROPERTY(QString website READ website WRITE setWebsite NOTIFY bufferChanged)
    Q_PROPERTY(QString notes READ notes WRITE setNotes NOTIFY bufferChanged)
    Q_PROPERTY(QStringList tags READ tags WRITE setTags NOTIFY bufferChanged)
    /// Tags as typed in the edit dialog, separated by spaces (C#: PasswordItemTagHelper).
    Q_PROPERTY(QString tagsText READ tagsText WRITE setTagsText NOTIFY bufferChanged)
    Q_PROPERTY(QString secret READ secret WRITE setSecret NOTIFY bufferChanged)
    Q_PROPERTY(QString digits READ digits WRITE setDigits NOTIFY bufferChanged)
    Q_PROPERTY(QString interval READ interval WRITE setInterval NOTIFY bufferChanged)
    Q_PROPERTY(QString algorithm READ algorithm WRITE setAlgorithm NOTIFY bufferChanged)
    Q_PROPERTY(bool has2FA READ has2FA NOTIFY bufferChanged)
    Q_PROPERTY(QString avatarLetter READ avatarLetter NOTIFY bufferChanged)
    Q_PROPERTY(bool dirty READ dirty NOTIFY bufferChanged)
    /// The buffer password is already used by another item (C#: AddItemPage red border).
    Q_PROPERTY(bool passwordReused READ passwordReused NOTIFY bufferChanged)
    /// Unique e-mail addresses of the open database (C#: EmailAddressIndexer), for suggestions.
    Q_PROPERTY(QStringList emailSuggestions READ emailSuggestions NOTIFY loadedChanged)
    Q_PROPERTY(bool valid READ valid NOTIFY bufferChanged)
    Q_PROPERTY(QStringList validationErrors READ validationErrors NOTIFY bufferChanged)
    Q_PROPERTY(QVariantList safetyChecks READ safetyChecks NOTIFY safetyChanged)
    Q_PROPERTY(LeakState leakState READ leakState NOTIFY safetyChanged)
    Q_PROPERTY(QString totpCode READ totpCode NOTIFY totpChanged)
    Q_PROPERTY(int totpRemaining READ totpRemaining NOTIFY totpChanged)
    Q_PROPERTY(double totpProgress READ totpProgress NOTIFY totpChanged)
public:
    enum class LeakState { Unknown, Checking, NotLeaked, Leaked };
    Q_ENUM(LeakState)

    ItemViewModel(Core::DatabaseService *service, TotpTimerService *totpTimer, QObject *parent = nullptr);
    ~ItemViewModel() override;

    /// Optional collaborators (nullptr = feature off).
    void setLeakCheckService(Core::LeakCheckService *leak);
    void setClipboardService(ClipboardService *clipboard);
    void setSettings(Core::AppSettings *settings);

    int index() const { return m_index; }
    bool isNew() const { return m_hasItem && m_index < 0; }
    bool hasItem() const { return m_hasItem; }

    QString displayName() const { return m_buffer.displayName; }
    QString username() const { return m_buffer.username; }
    QString email() const { return m_buffer.email; }
    QString password() const { return m_buffer.password; }
    QString website() const { return m_buffer.website; }
    QString notes() const { return m_buffer.notes; }
    QStringList tags() const { return m_buffer.tags; }
    QString tagsText() const;
    void setTagsText(const QString &text);
    QString secret() const { return m_buffer.secret; }
    QString digits() const { return m_buffer.digits; }
    QString interval() const { return m_buffer.interval; }
    QString algorithm() const { return m_buffer.algorithm; }
    void setDisplayName(const QString &v);
    void setUsername(const QString &v);
    void setEmail(const QString &v);
    void setPassword(const QString &v);
    void setWebsite(const QString &v);
    void setNotes(const QString &v);
    void setTags(const QStringList &v);
    void setSecret(const QString &v);
    void setDigits(const QString &v);
    void setInterval(const QString &v);
    void setAlgorithm(const QString &v);

    bool has2FA() const { return m_buffer.has2FA(); }
    QString avatarLetter() const { return m_buffer.avatarLetter(); }
    bool dirty() const;
    bool passwordReused() const;
    QStringList emailSuggestions() const { return m_emailSuggestions; }
    bool valid() const { return validationErrors().isEmpty(); }
    QStringList validationErrors() const;
    QVariantList safetyChecks() const { return m_safetyChecks; }
    LeakState leakState() const { return m_leakState; }
    QString totpCode() const { return m_totpCode; }
    int totpRemaining() const { return m_totpRemaining; }
    double totpProgress() const { return m_totpProgress; }

    const Core::PasswordItem &buffer() const { return m_buffer; }

    Q_INVOKABLE bool load(int index);
    Q_INVOKABLE void newItem();
    Q_INVOKABLE void unload();
    Q_INVOKABLE bool commit();
    Q_INVOKABLE void cancel();
    Q_INVOKABLE bool deleteItem();
    /// Fills secret/digits/interval/algorithm from an otpauth:// URI (QR scan result).
    Q_INVOKABLE bool applyOtpauthUri(const QString &uri);
    /// Fills empty digits/interval/algorithm with the defaults 6/30/SHA1 (C#: Add2FAPage fields).
    Q_INVOKABLE void prepare2FA();
    /// Clears the 2FA secret (C#: AddItemPage.Remove2FA_Click); takes effect on commit().
    Q_INVOKABLE void remove2FA();
    /// otpauth:// URI of the buffer (C#: TOTP.EncodeUrl(name, username, email, ...)); empty without 2FA.
    Q_INVOKABLE QString otpauthUri() const;
    /// The otpauth URI as QR code PNG data URL for an Image (C#: AddItemPage.Export2FA_Click).
    Q_INVOKABLE QString otpauthQrCode() const;
    Q_INVOKABLE void copyPassword();
    Q_INVOKABLE void copyUsername();
    Q_INVOKABLE void copyEmail();
    Q_INVOKABLE void copyTotp();

signals:
    void loadedChanged();
    void bufferChanged();
    void safetyChanged();
    void totpChanged();
    void committed(int index);
    void commitFailed(const QStringList &errors);
    void deleted(int index);

private:
    template <typename T> void setField(T Core::PasswordItem::*field, const T &value);
    void setLoaded(int index, const Core::PasswordItem &item, bool hasItem);
    void afterBufferChange(bool passwordChanged, bool totpChanged);
    void updateSafety();
    void updateTotp(qint64 now);
    void updateTotpSubscription();
    void onItemsChanged();
    int minPasswordLength() const;

    Core::DatabaseService *m_service;
    TotpTimerService *m_totpTimer;
    QPointer<Core::LeakCheckService> m_leak;
    QPointer<ClipboardService> m_clipboard;
    QPointer<Core::AppSettings> m_settings;

    Core::PasswordItem m_original;
    Core::PasswordItem m_buffer;
    int m_index = -1;
    bool m_hasItem = false;
    QStringList m_emailSuggestions;

    QVariantList m_safetyChecks;
    LeakState m_leakState = LeakState::Unknown;
    quint64 m_leakGeneration = 0;

    bool m_totpSubscribed = false;
    QString m_totpCode;
    int m_totpRemaining = 0;
    double m_totpProgress = 0.0;
};

} // namespace EasePass::App
