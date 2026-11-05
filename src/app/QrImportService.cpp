#include "QrImportService.h"

#include "IClipboard.h"
#include "QrCodeScanner.h"
#include "TotpHelper.h"

#include <QFileInfo>
#include <QImage>
#include <QUrl>

namespace EasePass::App {

using namespace EasePass::Core;

QrImportService::QrImportService(std::shared_ptr<IClipboard> clipboard, QObject *parent)
    : QObject(parent), m_clipboard(std::move(clipboard)) {}

bool QrImportService::scanFile(const QString &pathOrUrl) {
    const QUrl url(pathOrUrl);
    const QString path = url.isLocalFile() ? url.toLocalFile() : pathOrUrl;
    const QImage image(path);
    if (image.isNull()) {
        emit scanFailed(tr("The file %1 could not be read as an image.").arg(QFileInfo(path).fileName()));
        return false;
    }
    const QString text = QrCodeScanner::scanImage(image);
    if (text.isEmpty()) {
        emit scanFailed(tr("No QR code found in the image."));
        return false;
    }
    return accept(text);
}

bool QrImportService::scanClipboard() {
    const QString text = m_clipboard ? m_clipboard->text().trimmed() : QString();
    if (text.startsWith(QLatin1String("otpauth://"), Qt::CaseInsensitive))
        return accept(text);
    const QImage image = m_clipboard ? m_clipboard->image() : QImage();
    const QString scanned = image.isNull() ? QString() : QrCodeScanner::scanImage(image);
    if (scanned.isEmpty()) {
        emit scanFailed(tr("The clipboard contains neither a QR code image nor an otpauth link."));
        return false;
    }
    return accept(scanned);
}

bool QrImportService::accept(const QString &text) {
    TotpParameters params;
    if (!TotpHelper::parseOtpauthUri(text.trimmed(), params) || params.secret.isEmpty()) {
        emit scanFailed(tr("The QR code is not a valid two-factor authentication code."));
        return false;
    }
    emit otpauthFound(text.trimmed());
    return true;
}

QString QrImportService::describe(const QString &text) {
    TotpParameters params;
    if (!TotpHelper::parseOtpauthUri(text.trimmed(), params))
        return text.left(40);
    if (params.issuer.isEmpty())
        return params.account;
    if (params.account.isEmpty())
        return params.issuer;
    return QStringLiteral("%1 (%2)").arg(params.issuer, params.account);
}

} // namespace EasePass::App
