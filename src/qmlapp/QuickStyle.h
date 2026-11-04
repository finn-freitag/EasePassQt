#pragma once

#include <QString>

namespace EasePass::App {

/// Qt Quick Controls style used by easepass_qml (steps.txt T11).
/// FluentWinUI3 is the preferred style (closest to the WinUI 3 look of the C# app; it ships
/// with Qt >= 6.8 on every platform). An explicit QT_QUICK_CONTROLS_STYLE wins; if the
/// Fluent style is missing from the Qt installation, "Basic" is used.
QString chooseQuickStyle(const QString &envOverride, bool fluentAvailable);

/// Applies chooseQuickStyle() via QQuickStyle::setStyle. Call before creating the engine.
QString applyQuickStyle();

} // namespace EasePass::App
