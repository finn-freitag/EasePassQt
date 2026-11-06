#pragma once

#include <QColor>
#include <QString>

namespace EasePass::App {

/// Qt Quick Controls style used by the app (steps.txt T11).
/// FluentWinUI3 is the preferred style (closest to the WinUI 3 look of the C# app; it ships
/// with Qt >= 6.8 on every platform). An explicit QT_QUICK_CONTROLS_STYLE wins; if the
/// Fluent style is missing from the Qt installation, "Basic" is used.
QString chooseQuickStyle(const QString &envOverride, bool fluentAvailable);

/// Light/dark decision when the platform reports Qt::ColorScheme::Unknown: from the
/// lightness of the application palette's window colour.
Qt::ColorScheme resolveColorScheme(Qt::ColorScheme reported, const QColor &windowColor);

/// Applies chooseQuickStyle() via QQuickStyle::setStyle and makes the colour scheme explicit
/// (QStyleHints::setColorScheme) when the platform reports Unknown. Otherwise the Theme
/// singleton (light) and the Fluent style (dark palettes) can disagree, giving white text on
/// light backgrounds. Call before creating the engine.
QString applyQuickStyle();

} // namespace EasePass::App
