#pragma once

#include <QString>
#include <QStringList>
#include <QJsonObject>
#include <QColor>
#include "IClock.h"

namespace EasePass::Core {

class PasswordItem {
public:
    QString password;
    QString username;
    QString email;
    QString notes;
    QString secret;
    QString digits = "6";
    QString interval = "30";
    QString algorithm = "SHA1";
    QStringList clicks;
    QStringList tags;
    QString displayName;
    QString website;

    bool has2FA() const { return !secret.trimmed().isEmpty(); }

    QJsonObject toJson() const;
    static PasswordItem fromJson(const QJsonObject &json);

    QColor avatarColor() const;
    QColor avatarTextColor() const;
    QString avatarLetter() const;

    void registerClick(const IClock *clock = nullptr);
};

} // namespace EasePass::Core
