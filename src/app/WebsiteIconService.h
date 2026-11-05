#pragma once

#include <QObject>
#include <QQuickImageProvider>

namespace EasePass::Core {
class AppSettings;
class WebsiteIconManager;
} // namespace EasePass::Core

namespace EasePass::App {

/**
 * Website icons (favicons) for QML (steps.txt T14). iconUrl(website) returns an
 * "image://favicon/..." URL once the icon is cached, else "" (and starts a download if the
 * user allowed fetching, AppSettings::fetchFavicons). `revision` changes whenever a new icon
 * arrives so bindings re-evaluate. Respects AppSettings::showIcons.
 */
class WebsiteIconService : public QObject {
    Q_OBJECT
    Q_PROPERTY(int revision READ revision NOTIFY revisionChanged)
    Q_PROPERTY(bool enabled READ enabled NOTIFY revisionChanged)
public:
    static inline const QString ProviderId = QStringLiteral("favicon");

    WebsiteIconService(Core::WebsiteIconManager *manager, Core::AppSettings *settings, QObject *parent = nullptr);

    int revision() const { return m_revision; }
    bool enabled() const;
    Core::WebsiteIconManager *manager() const { return m_manager; }

    Q_INVOKABLE QString iconUrl(const QString &website);

    /// Image provider serving image://favicon/<domain>; owned by the QQmlEngine after addImageProvider.
    QQuickImageProvider *createImageProvider() const;

signals:
    void revisionChanged();

private:
    Core::WebsiteIconManager *m_manager;
    Core::AppSettings *m_settings;
    int m_revision = 0;
};

} // namespace EasePass::App
