#pragma once

#include <QObject>
#include <QUrl>
#include <QVariantList>
#include <functional>

namespace EasePass::App {

/**
 * About page (steps.txt T18; C#: Views/AboutPage.xaml(.cs), Assets/changelog.txt).
 * Version from the CMake project version, developers, links, dependencies with licenses and the
 * bundled changelog. Links open through an injectable opener (default QDesktopServices) so
 * tests never start a browser; only http(s) and mailto URLs are opened.
 * The privacy policy and disclaimer are translatable texts in AboutPage.qml.
 */
class AboutViewModel : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString version READ version CONSTANT)
    Q_PROPERTY(QVariantList developers READ developers CONSTANT)
    Q_PROPERTY(QVariantList contributors READ contributors CONSTANT)
    Q_PROPERTY(QVariantList dependencies READ dependencies CONSTANT)
    Q_PROPERTY(QVariantList changelog READ changelog CONSTANT)
    Q_PROPERTY(QString websiteUrl READ websiteUrl CONSTANT)
    Q_PROPERTY(QString repositoryUrl READ repositoryUrl CONSTANT)
    Q_PROPERTY(QString originalRepositoryUrl READ originalRepositoryUrl CONSTANT)
    Q_PROPERTY(QString issuesUrl READ issuesUrl CONSTANT)
    Q_PROPERTY(QString contactEmail READ contactEmail CONSTANT)
public:
    using UrlOpener = std::function<bool(const QUrl &)>;

    explicit AboutViewModel(QObject *parent = nullptr);

    /// Replaces how URLs are opened (tests). Default: QDesktopServices::openUrl.
    void setUrlOpener(UrlOpener opener) { m_opener = std::move(opener); }

    static QString version();
    /// {name, url} per developer (C#: Finn Freitag, Julius Kirsch).
    static QVariantList developers();
    static QVariantList contributors();
    /// {name, version, license, url} per library (Core::BuildInfo).
    static QVariantList dependencies();
    /// {title, lines} per version block of the bundled changelog.
    QVariantList changelog() const { return m_changelog; }
    static QString websiteUrl();
    static QString repositoryUrl();
    static QString originalRepositoryUrl();
    static QString issuesUrl();
    static QString contactEmail();

    /**
     * C# changelog format: a line starting with '#' is a headline (starts a new block),
     * "---" starts a paragraph (kept as an empty line), other lines are entries; leading
     * whitespace is trimmed and blank lines are dropped.
     */
    static QVariantList parseChangelog(const QString &text);

    /// Opens @p url (http, https or mailto only). Returns false for other schemes or failures.
    Q_INVOKABLE bool openUrl(const QString &url);

signals:
    /// Emitted for every URL handed to the opener.
    void urlOpened(const QString &url);

private:
    UrlOpener m_opener;
    QVariantList m_changelog;
};

} // namespace EasePass::App
