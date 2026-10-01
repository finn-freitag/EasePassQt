#include "TotpHelper.h"

#include <QUrl>
#include <QUrlQuery>
#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <cmath>

namespace EasePass::Core {

QByteArray TotpHelper::base32Decode(const QString& input) {
    QString clean = input.trimmed().remove(' ').remove('-').toUpper();
    while (clean.endsWith('=')) {
        clean.chop(1);
    }
    if (clean.isEmpty()) return QByteArray();

    int byteCount = clean.length() * 5 / 8;
    QByteArray returnArray(byteCount, 0);

    uint8_t curByte = 0;
    int bitsRemaining = 8;
    int arrayIndex = 0;

    auto charToValue = [](QChar c) -> int {
        char ch = c.toLatin1();
        if (ch >= 'A' && ch <= 'Z') return ch - 'A';
        if (ch >= '2' && ch <= '7') return ch - '2' + 26;
        return -1;
    };

    for (int i = 0; i < clean.length(); ++i) {
        int val = charToValue(clean[i]);
        if (val < 0) continue;

        if (bitsRemaining > 5) {
            int mask = val << (bitsRemaining - 5);
            curByte |= static_cast<uint8_t>(mask);
            bitsRemaining -= 5;
        } else {
            int mask = val >> (5 - bitsRemaining);
            curByte |= static_cast<uint8_t>(mask);
            if (arrayIndex < byteCount) {
                returnArray[arrayIndex++] = static_cast<char>(curByte);
            }
            curByte = static_cast<uint8_t>((val << (3 + bitsRemaining)) & 0xFF);
            bitsRemaining += 3;
        }
    }

    if (arrayIndex < byteCount) {
        returnArray[arrayIndex] = static_cast<char>(curByte);
    }

    return returnArray;
}

QString TotpHelper::generateToken(const QString& secret,
                                  qint64 timestampSeconds,
                                  int digits,
                                  int period,
                                  const QString& algorithm) {
    if (period <= 0) period = 30;
    if (digits <= 0) digits = 6;

    QByteArray key = base32Decode(secret);
    if (key.isEmpty()) return QString();

    uint64_t timeCounter = static_cast<uint64_t>(timestampSeconds / period);
    uint8_t timeBytes[8];
    for (int i = 7; i >= 0; --i) {
        timeBytes[i] = static_cast<uint8_t>(timeCounter & 0xFF);
        timeCounter >>= 8;
    }

    const EVP_MD* md = EVP_sha1();
    if (algorithm.compare("SHA256", Qt::CaseInsensitive) == 0) {
        md = EVP_sha256();
    } else if (algorithm.compare("SHA512", Qt::CaseInsensitive) == 0) {
        md = EVP_sha512();
    }

    unsigned int hmacLen = 0;
    uint8_t hmacRes[EVP_MAX_MD_SIZE];
    if (!HMAC(md, key.constData(), key.size(), timeBytes, 8, hmacRes, &hmacLen)) {
        return QString();
    }

    int offset = hmacRes[hmacLen - 1] & 0x0F;
    uint32_t binary = ((static_cast<uint32_t>(hmacRes[offset]) & 0x7F) << 24) |
                      ((static_cast<uint32_t>(hmacRes[offset + 1]) & 0xFF) << 16) |
                      ((static_cast<uint32_t>(hmacRes[offset + 2]) & 0xFF) << 8) |
                      (static_cast<uint32_t>(hmacRes[offset + 3]) & 0xFF);

    uint32_t mod = 1;
    for (int i = 0; i < digits; ++i) {
        mod *= 10;
    }
    uint32_t code = binary % mod;

    return QString("%1").arg(code, digits, 10, QChar('0'));
}

QString TotpHelper::generateCurrentToken(const QString& secret,
                                         int digits,
                                         int period,
                                         const QString& algorithm) {
    return generateToken(secret, QDateTime::currentSecsSinceEpoch(), digits, period, algorithm);
}

int TotpHelper::getRemainingSeconds(int period, qint64 timestampSeconds) {
    if (period <= 0) period = 30;
    qint64 t = (timestampSeconds > 0) ? timestampSeconds : QDateTime::currentSecsSinceEpoch();
    int rem = period - static_cast<int>(t % period);
    return (rem == 0) ? period : rem;
}

double TotpHelper::getProgress(int period, qint64 timestampSeconds) {
    if (period <= 0) period = 30;
    return static_cast<double>(getRemainingSeconds(period, timestampSeconds)) / static_cast<double>(period);
}

bool TotpHelper::parseOtpauthUri(const QString& uri, TotpParameters& outParams) {
    if (!uri.startsWith("otpauth://totp/", Qt::CaseInsensitive)) {
        return false;
    }

    QUrl url(uri);
    outParams.algorithm = "SHA1";
    outParams.digits = 6;
    outParams.period = 30;

    QString path = url.path();
    if (path.startsWith("/")) path = path.mid(1);

    if (path.contains(":")) {
        QStringList parts = path.split(":");
        outParams.issuer = QUrl::fromPercentEncoding(parts[0].toUtf8());
        outParams.account = QUrl::fromPercentEncoding(parts[1].toUtf8());
    } else {
        outParams.account = QUrl::fromPercentEncoding(path.toUtf8());
    }

    QUrlQuery query(url.query());
    if (query.hasQueryItem("secret")) {
        outParams.secret = query.queryItemValue("secret");
    }
    if (query.hasQueryItem("issuer")) {
        outParams.issuer = query.queryItemValue("issuer");
    }
    if (query.hasQueryItem("algorithm")) {
        outParams.algorithm = query.queryItemValue("algorithm").toUpper();
    }
    if (query.hasQueryItem("digits")) {
        bool ok = false;
        int d = query.queryItemValue("digits").toInt(&ok);
        if (ok && d > 0) outParams.digits = d;
    }
    if (query.hasQueryItem("period")) {
        bool ok = false;
        int p = query.queryItemValue("period").toInt(&ok);
        if (ok && p > 0) outParams.period = p;
    }

    return !outParams.secret.isEmpty();
}

QString TotpHelper::generateOtpauthUri(const TotpParameters& params) {
    QUrl url;
    url.setScheme("otpauth");
    url.setHost("totp");

    QString label;
    if (!params.issuer.isEmpty()) {
        label = params.issuer + ":" + params.account;
    } else {
        label = params.account;
    }
    url.setPath("/" + label);

    QUrlQuery query;
    query.addQueryItem("secret", params.secret);
    if (!params.issuer.isEmpty()) {
        query.addQueryItem("issuer", params.issuer);
    }
    query.addQueryItem("algorithm", params.algorithm);
    query.addQueryItem("digits", QString::number(params.digits));
    query.addQueryItem("period", QString::number(params.period));
    url.setQuery(query);

    return url.toString();
}

} // namespace EasePass::Core
