#include "IClipboard.h"

#include <QClipboard>
#include <QGuiApplication>
#include <QMimeData>

namespace EasePass::Core {

namespace {
QClipboard *systemClipboard() {
    return qobject_cast<QGuiApplication *>(QCoreApplication::instance()) ? QGuiApplication::clipboard() : nullptr;
}
} // namespace

QString QtClipboard::text() const {
    QClipboard *cb = systemClipboard();
    return cb ? cb->text(QClipboard::Clipboard) : QString();
}

QImage QtClipboard::image() const {
    QClipboard *cb = systemClipboard();
    return cb ? cb->image(QClipboard::Clipboard) : QImage();
}

void QtClipboard::setText(const QString &text, bool secret) {
    QClipboard *cb = systemClipboard();
    if (!cb)
        return;

    auto *mime = new QMimeData;
    mime->setText(text);
    if (secret) {
        // KDE Klipper / Plasma: do not store in clipboard history.
        mime->setData(QStringLiteral("x-kde-passwordManagerHint"), QByteArrayLiteral("secret"));
#ifdef Q_OS_WIN
        // Windows clipboard history / cloud clipboard and monitoring tools.
        // Qt maps this mime type to the registered clipboard format of that name.
        mime->setData(QStringLiteral("application/x-qt-windows-mime;value=\"ExcludeClipboardContentFromMonitorProcessing\""),
                      QByteArray(4, '\0'));
        mime->setData(QStringLiteral("application/x-qt-windows-mime;value=\"CanIncludeInClipboardHistory\""),
                      QByteArray(4, '\0'));
        mime->setData(QStringLiteral("application/x-qt-windows-mime;value=\"CanUploadToCloudClipboard\""),
                      QByteArray(4, '\0'));
#endif
        // TODO(macOS): org.nspasteboard.ConcealedType once macOS is targeted.
    }
    cb->setMimeData(mime, QClipboard::Clipboard); // takes ownership
}

void QtClipboard::clear() {
    if (QClipboard *cb = systemClipboard())
        cb->clear(QClipboard::Clipboard);
}

} // namespace EasePass::Core
