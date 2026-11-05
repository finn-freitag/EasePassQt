#pragma once
// Real App layer (AppContext) with fakes for QML page tests (steps.txt T14): no network, no
// real clipboard, in-memory settings, fake clock, cheap KDF. Exposed as `testContext`.

#include "../app/AppTestSupport.h"

#include <QObject>
#include <QTemporaryDir>
#include <QVariantMap>
#include <memory>

namespace EasePass::App { class AppContext; }

class QmlTestContext : public QObject {
    Q_OBJECT
    Q_PROPERTY(QObject *listModel READ listModel CONSTANT)
    Q_PROPERTY(QObject *item READ item CONSTANT)
    Q_PROPERTY(QObject *actions READ actions CONSTANT)
    Q_PROPERTY(QObject *settings READ settings CONSTANT)
    Q_PROPERTY(QObject *controller READ controller CONSTANT)
    Q_PROPERTY(QObject *clipboard READ clipboard CONSTANT)
    Q_PROPERTY(QObject *editor READ editor CONSTANT)
    Q_PROPERTY(QObject *generator READ generator CONSTANT)
    Q_PROPERTY(QObject *qrImport READ qrImport CONSTANT)
    Q_PROPERTY(QObject *screenScan READ screenScan CONSTANT)
    Q_PROPERTY(QObject *webcamScan READ webcamScan CONSTANT)
    Q_PROPERTY(QString clipboardText READ clipboardText NOTIFY clipboardChanged)
    Q_PROPERTY(int itemCount READ itemCount NOTIFY itemsChanged)
public:
    explicit QmlTestContext(QObject *parent = nullptr);
    ~QmlTestContext() override;

    QObject *listModel() const;
    QObject *item() const;
    QObject *actions() const;
    QObject *settings() const;
    QObject *controller() const;
    QObject *clipboard() const;
    QObject *editor() const;
    QObject *generator() const;
    QObject *qrImport() const;
    QObject *screenScan() const;
    QObject *webcamScan() const;
    QString clipboardText() const { return m_clip->content; }
    int itemCount() const;

    /// Closes any open database and opens the sample items (waits for the unlock).
    Q_INVOKABLE bool openSample();
    Q_INVOKABLE bool openEmpty();
    Q_INVOKABLE void setReadOnly(bool readOnly);
    Q_INVOKABLE void advanceClock(int seconds);
    Q_INVOKABLE qint64 now() const { return m_clock.now; }
    Q_INVOKABLE QString totpFor(qint64 when) const;
    Q_INVOKABLE void clearClipboard();
    Q_INVOKABLE int clicksOf(int index) const;
    Q_INVOKABLE QString nameOf(int index) const;
    /// Stored item fields (PasswordItem JSON keys, e.g. "Username", "Secret") for assertions.
    Q_INVOKABLE QVariantMap itemAt(int index) const;
    Q_INVOKABLE void setClipboardText(const QString &text);
    /// Puts a QR code image with @p text on the fake clipboard (and clears its text).
    Q_INVOKABLE void setClipboardQr(const QString &text);
    /// The fake screen shows QR codes with these texts (empty list = no code).
    Q_INVOKABLE void setScreenQrCodes(const QStringList &texts);
    /// The fake webcam reports @p text as detected QR code.
    Q_INVOKABLE void webcamDetect(const QString &text);
    Q_INVOKABLE bool webcamActive() const;
    /// Writes a QR code PNG with @p text to a temporary file; returns its file:// URL.
    Q_INVOKABLE QString writeQrFile(const QString &text);
    Q_INVOKABLE QString otpauthFor(const QString &secret, const QString &issuer) const;
    Q_INVOKABLE void setGeneratorSettings(int length, const QString &chars);

signals:
    void clipboardChanged();
    void itemsChanged();

private:
    bool openItems(const QList<EasePass::Core::PasswordItem> &items);

    AppTest::FakeClock m_clock;
    std::shared_ptr<AppTest::FakeClipboard> m_clip;
    std::shared_ptr<AppTest::FakeScreenCapture> m_screen;
    std::shared_ptr<AppTest::FakeWebcamScanner> m_webcam;
    QTemporaryDir m_tempDir;
    std::shared_ptr<FakeDatabaseSource> m_source;
    std::unique_ptr<EasePass::App::AppContext> m_ctx;
};
