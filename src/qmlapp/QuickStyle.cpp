#include "QuickStyle.h"

#include <QLibraryInfo>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QPalette>
#include <QQuickStyle>
#include <QStyleHints>
#include <QStringList>

namespace EasePass::App {

QString chooseQuickStyle(const QString &envOverride, bool fluentAvailable) {
    if (!envOverride.isEmpty())
        return envOverride;
    return fluentAvailable ? QStringLiteral("FluentWinUI3") : QStringLiteral("Basic");
}

Qt::ColorScheme resolveColorScheme(Qt::ColorScheme reported, const QColor &windowColor) {
    if (reported != Qt::ColorScheme::Unknown)
        return reported;
    return windowColor.lightness() < 128 ? Qt::ColorScheme::Dark : Qt::ColorScheme::Light;
}

QString applyQuickStyle() {
    if (auto *hints = QGuiApplication::styleHints()) {
        const Qt::ColorScheme reported = hints->colorScheme();
        if (reported == Qt::ColorScheme::Unknown)
            hints->setColorScheme(resolveColorScheme(reported, QGuiApplication::palette().color(QPalette::Window)));
    }
    const QString env = qEnvironmentVariable("QT_QUICK_CONTROLS_STYLE");
    // Qt's own QML dir plus the extra import dirs (Nix keeps every Qt module in its own prefix).
    QStringList dirs{QLibraryInfo::path(QLibraryInfo::QmlImportsPath)};
    for (const char *var : {"QML_IMPORT_PATH", "QML2_IMPORT_PATH", "NIXPKGS_QT6_QML_IMPORT_PATH"})
        dirs += qEnvironmentVariable(var).split(QDir::listSeparator(), Qt::SkipEmptyParts);
    bool fluent = false;
    for (const QString &dir : std::as_const(dirs))
        fluent = fluent || QFileInfo::exists(QDir(dir).filePath(QStringLiteral("QtQuick/Controls/FluentWinUI3/qmldir")));
    const QString style = chooseQuickStyle(env, fluent);
    QQuickStyle::setStyle(style);
    return style;
}

} // namespace EasePass::App
