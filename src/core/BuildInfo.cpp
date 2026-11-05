#include "BuildInfo.h"

#include <QtGlobal>
#include <openssl/crypto.h>

#if __has_include(<ZXing/Version.h>)
#include <ZXing/Version.h>
#else
#include <ZXing/ZXVersion.h>
#endif

#ifndef EASEPASS_VERSION
#error "EASEPASS_VERSION must be defined by the build (CMake project version)"
#endif

namespace EasePass::Core::BuildInfo {

QString appVersion() { return QStringLiteral(EASEPASS_VERSION); }

QList<Dependency> dependencies() {
    // OpenSSL_version(OPENSSL_VERSION) is "OpenSSL 3.x.y <date>"; keep the number.
    QString openssl = QString::fromLatin1(OpenSSL_version(OPENSSL_VERSION));
    const QStringList parts = openssl.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (parts.size() >= 2)
        openssl = parts.at(1);
    const QString zxing = QStringLiteral("%1.%2.%3")
                              .arg(ZXING_VERSION_MAJOR)
                              .arg(ZXING_VERSION_MINOR)
                              .arg(ZXING_VERSION_PATCH);
    return {
        {QStringLiteral("Qt"), QString::fromLatin1(qVersion()), QStringLiteral("LGPL-3.0-only"),
         QStringLiteral("https://www.qt.io/")},
        {QStringLiteral("OpenSSL"), openssl, QStringLiteral("Apache-2.0"), QStringLiteral("https://www.openssl.org/")},
        {QStringLiteral("Argon2"), QString(), QStringLiteral("CC0-1.0 OR Apache-2.0"),
         QStringLiteral("https://github.com/P-H-C/phc-winner-argon2")},
        {QStringLiteral("zxing-cpp"), zxing, QStringLiteral("Apache-2.0"),
         QStringLiteral("https://github.com/zxing-cpp/zxing-cpp")},
    };
}

} // namespace EasePass::Core::BuildInfo
