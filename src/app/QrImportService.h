#pragma once

#include <QObject>
#include <memory>

namespace EasePass::Core {
class IClipboard;
}

namespace EasePass::App {

/**
 * 2FA QR import from an image file or the clipboard (steps.txt T15; C#: Add2FAPage only had
 * screen + webcam, the legacy Qt Manage2FADialog added file + clipboard). Every source ends
 * in accept(): only otpauth:// URIs that parse are reported via otpauthFound, everything else
 * via scanFailed with a user-facing message. Screen and webcam scans feed accept() too.
 */
class QrImportService : public QObject {
    Q_OBJECT
public:
    explicit QrImportService(std::shared_ptr<Core::IClipboard> clipboard, QObject *parent = nullptr);

    /// @p pathOrUrl: local path or file:// URL of an image with a QR code.
    Q_INVOKABLE bool scanFile(const QString &pathOrUrl);
    /// Clipboard text that is an otpauth URI, else a QR code in the clipboard image.
    Q_INVOKABLE bool scanClipboard();
    /// Validates a scanned text (QR content or pasted URI).
    Q_INVOKABLE bool accept(const QString &text);
    /// Short label "issuer (account)" for a scanned URI, to choose between several codes.
    Q_INVOKABLE static QString describe(const QString &text);

signals:
    void otpauthFound(const QString &uri);
    void scanFailed(const QString &message);

private:
    std::shared_ptr<Core::IClipboard> m_clipboard;
};

} // namespace EasePass::App
