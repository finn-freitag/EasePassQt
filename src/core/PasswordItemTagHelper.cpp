#include "PasswordItemTagHelper.h"

#include <QCryptographicHash>
#include <QRegularExpression>

namespace EasePass::Core {

QStringList PasswordItemTagHelper::parseTags(const QString &raw) {
    // C#: raw.Split(" ", StringSplitOptions.RemoveEmptyEntries)
    if (raw.isEmpty()) {
        return {};
    }
    static const QRegularExpression wsRe(QStringLiteral("\\s+"));
    return raw.split(wsRe, Qt::SkipEmptyParts);
}

void PasswordItemTagHelper::parseToTags(const QString &raw, PasswordItem &item) {
    item.tags = parseTags(raw);
}

QString PasswordItemTagHelper::formatTags(const QStringList &tags) {
    // C#: string.Join(" ", item.Tags)
    if (tags.isEmpty()) {
        return {};
    }
    return tags.join(u' ');
}

QColor PasswordItemTagHelper::tagColor(const QString &tag) {
    // C#: Controls/TagRenderControl.xaml.cs:
    // byte[] bytes = md5Hasher.ComputeHash(Encoding.UTF8.GetBytes(tag));
    // var tagColor = new SolidColorBrush(Color.FromArgb(255, bytes[1], bytes[0], bytes[2]));
    const QByteArray hash = QCryptographicHash::hash(tag.toUtf8(), QCryptographicHash::Md5);
    if (hash.size() < 3) {
        return QColor(70, 130, 180);
    }
    const int r = static_cast<uint8_t>(hash[1]);
    const int g = static_cast<uint8_t>(hash[0]);
    const int b = static_cast<uint8_t>(hash[2]);
    return QColor(r, g, b);
}

} // namespace EasePass::Core
