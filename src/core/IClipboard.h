#pragma once

#include <QImage>
#include <QString>

class QClipboard;

namespace EasePass::Core {

/**
 * Abstraction over the system clipboard so ClipboardManager can be tested with a fake (T7).
 */
class IClipboard {
public:
    virtual ~IClipboard() = default;

    /// Current plain-text content of the clipboard (empty if none or not text).
    virtual QString text() const = 0;

    /// Current image content (null if none). Used to scan 2FA QR codes from the clipboard (T15).
    virtual QImage image() const { return {}; }

    /**
     * Puts @p text on the clipboard. When @p secret is true the content is marked so
     * clipboard managers/history do not record it (KDE x-kde-passwordManagerHint=secret,
     * Windows ExcludeClipboardContentFromMonitorProcessing / CanIncludeInClipboardHistory=0).
     */
    virtual void setText(const QString &text, bool secret) = 0;

    virtual void clear() = 0;
};

/**
 * IClipboard backed by QGuiApplication::clipboard(). Needs a QGuiApplication.
 */
class QtClipboard : public IClipboard {
public:
    QtClipboard() = default;

    QString text() const override;
    QImage image() const override;
    void setText(const QString &text, bool secret) override;
    void clear() override;
};

} // namespace EasePass::Core
