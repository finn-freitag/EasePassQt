#pragma once

class QQmlApplicationEngine;
class QQuickWindow;

namespace EasePass::App {

class AppContext;

/**
 * Loads the main window (QML module "EasePass", type Main) into @p engine with the objects of
 * @p context as initial properties, the image providers and live language switching
 * (steps.txt T12-T20). Shared by main.cpp and the application smoke test (T24), so both run
 * exactly the same start-up path. The executable must import the static QML plugin
 * (Q_IMPORT_QML_PLUGIN(EasePassPlugin)).
 *
 * Main.qml starts hidden; the caller restores the geometry and shows the window.
 * Returns nullptr if the QML could not be loaded.
 */
QQuickWindow *loadMainWindow(QQmlApplicationEngine &engine, AppContext &context);

} // namespace EasePass::App
