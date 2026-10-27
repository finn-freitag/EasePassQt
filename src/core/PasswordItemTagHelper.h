#pragma once

#include "PasswordItem.h"

#include <QColor>
#include <QString>
#include <QStringList>

namespace EasePass::Core {

/**
 * @brief Parses and formats item tags, and computes deterministic tag badge colors.
 *
 * C#: Helper/PasswordItemTagHelper.cs, Controls/TagRenderControl.xaml.cs.
 */
class PasswordItemTagHelper {
public:
    /// C#: PasswordItemTagHelper.ParseToTags: splits `raw` on whitespace (`SkipEmptyParts`).
    static QStringList parseTags(const QString &raw);
    static void parseToTags(const QString &raw, PasswordItem &item);

    /// C#: PasswordItemTagHelper.FillTags: joins tags with `" "`.
    static QString formatTags(const QStringList &tags);
    static QString formatTags(const PasswordItem &item) { return formatTags(item.tags); }

    /// C#: TagRenderControl.UpdateTagsUI: MD5(UTF8(tag)) -> Color.FromArgb(255, bytes[1], bytes[0], bytes[2])
    static QColor tagColor(const QString &tag);
};

} // namespace EasePass::Core
