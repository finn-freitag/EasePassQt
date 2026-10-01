#include "PasswordItem.h"

#include <QJsonArray>
#include <QCryptographicHash>
#include <QDateTime>

namespace EasePass::Core {

QJsonObject PasswordItem::toJson() const {
    QJsonObject obj;
    obj["Password"] = password;
    obj["Username"] = username;
    obj["Email"] = email;
    obj["Notes"] = notes;
    obj["Secret"] = secret;
    obj["Digits"] = digits.isEmpty() ? "6" : digits;
    obj["Interval"] = interval.isEmpty() ? "30" : interval;
    obj["Algorithm"] = algorithm.isEmpty() ? "SHA1" : algorithm;

    QJsonArray clicksArray;
    for (const auto& c : clicks) {
        clicksArray.append(c);
    }
    obj["Clicks"] = clicksArray;

    QJsonArray tagsArray;
    for (const auto& t : tags) {
        tagsArray.append(t);
    }
    obj["Tags"] = tagsArray;

    obj["DisplayName"] = displayName;
    obj["Website"] = website;

    return obj;
}

PasswordItem PasswordItem::fromJson(const QJsonObject& json) {
    PasswordItem item;
    item.password = json.value("Password").toString();
    item.username = json.value("Username").toString();
    item.email = json.value("Email").toString();
    item.notes = json.value("Notes").toString();
    item.secret = json.value("Secret").toString();
    item.digits = json.value("Digits").toString("6");
    item.interval = json.value("Interval").toString("30");
    item.algorithm = json.value("Algorithm").toString("SHA1");

    QJsonArray clicksArray = json.value("Clicks").toArray();
    for (const auto& val : clicksArray) {
        item.clicks.append(val.toString());
    }

    QJsonArray tagsArray = json.value("Tags").toArray();
    for (const auto& val : tagsArray) {
        item.tags.append(val.toString());
    }

    item.displayName = json.value("DisplayName").toString();
    item.website = json.value("Website").toString();

    return item;
}

QColor PasswordItem::avatarColor() const {
    QByteArray hash = QCryptographicHash::hash(displayName.toUtf8(), QCryptographicHash::Md5);
    if (hash.size() < 3) {
        return QColor(70, 130, 180);
    }
    int r = static_cast<uint8_t>(hash[0]);
    int g = static_cast<uint8_t>(hash[1]);
    int b = static_cast<uint8_t>(hash[2]);
    return QColor(r, g, b);
}

QColor PasswordItem::avatarTextColor() const {
    QColor c = avatarColor();
    double luminance = 0.299 * c.red() + 0.587 * c.green() + 0.114 * c.blue();
    return (luminance > 140.0) ? QColor(20, 20, 20) : QColor(255, 255, 255);
}

QString PasswordItem::avatarLetter() const {
    QString trimmed = displayName.trimmed();
    if (trimmed.isEmpty()) {
        trimmed = website.trimmed();
    }
    if (trimmed.isEmpty()) {
        return "?";
    }
    return trimmed.left(1).toUpper();
}

void PasswordItem::registerClick() {
    clicks.append(QDate::currentDate().toString("d.M.yyyy"));
}

} // namespace EasePass::Core
