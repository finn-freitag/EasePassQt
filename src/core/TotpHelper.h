#pragma once

#include <QString>
#include <QDateTime>
#include <cstdint>
#include <optional>
#include "IClock.h"

namespace EasePass::Core {

struct TotpParameters {
    QString secret;
    QString issuer;
    QString account;
    QString algorithm = "SHA1";
    int digits = 6;
    int period = 30;
};

class TotpHelper {
public:
    static QString generateToken(const QString &secret, qint64 timestampSeconds, int digits = 6, int period = 30,
                                 const QString &algorithm = "SHA1");

    static QString generateCurrentToken(const QString &secret, int digits = 6, int period = 30,
                                        const QString &algorithm = "SHA1", const IClock *clock = nullptr);

    static int getRemainingSeconds(int period = 30, std::optional<qint64> timestampSeconds = std::nullopt,
                                   const IClock *clock = nullptr);
    static double getProgress(int period = 30, std::optional<qint64> timestampSeconds = std::nullopt,
                              const IClock *clock = nullptr);

    static bool parseOtpauthUri(const QString &uri, TotpParameters &outParams);
    static QString generateOtpauthUri(const TotpParameters &params);

    static QByteArray base32Decode(const QString &input);
};

} // namespace EasePass::Core
