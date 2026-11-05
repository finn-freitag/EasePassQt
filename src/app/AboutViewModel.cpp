#include "AboutViewModel.h"

#include "BuildInfo.h"

#include <QDesktopServices>
#include <QFile>
#include <QVariantMap>

namespace EasePass::App {

AboutViewModel::AboutViewModel(QObject *parent)
    : QObject(parent), m_opener([](const QUrl &url) { return QDesktopServices::openUrl(url); }) {
    QFile file(QStringLiteral(":/changelog/changelog.txt"));
    if (file.open(QIODevice::ReadOnly))
        m_changelog = parseChangelog(QString::fromUtf8(file.readAll()));
}

QString AboutViewModel::version() { return Core::BuildInfo::appVersion(); }

QVariantList AboutViewModel::developers() {
    return {
        QVariantMap{{QStringLiteral("name"), QStringLiteral("Finn Freitag")},
                    {QStringLiteral("url"), QStringLiteral("https://github.com/finn-freitag")}},
        QVariantMap{{QStringLiteral("name"), QStringLiteral("Julius Kirsch")},
                    {QStringLiteral("url"), QStringLiteral("https://github.com/frozenassassine")}},
    };
}

QVariantList AboutViewModel::contributors() {
    return {QVariantMap{{QStringLiteral("name"), QStringLiteral("@Flamifly")},
                        {QStringLiteral("url"), QStringLiteral("https://github.com/Flamifly")}}};
}

QVariantList AboutViewModel::dependencies() {
    QVariantList list;
    for (const auto &d : Core::BuildInfo::dependencies())
        list << QVariantMap{{QStringLiteral("name"), d.name},
                            {QStringLiteral("version"), d.version},
                            {QStringLiteral("license"), d.license},
                            {QStringLiteral("url"), d.url}};
    return list;
}

QString AboutViewModel::websiteUrl() { return QStringLiteral("http://easepass.frozenassassine.de"); }
QString AboutViewModel::repositoryUrl() { return QStringLiteral("https://github.com/finn-freitag/EasePassQt"); }
QString AboutViewModel::originalRepositoryUrl() { return QStringLiteral("https://github.com/FrozenAssassine/EasePass"); }
QString AboutViewModel::issuesUrl() { return repositoryUrl() + QStringLiteral("/issues/new"); }
QString AboutViewModel::contactEmail() { return QStringLiteral("contact@frozenassassine.de"); }

QVariantList AboutViewModel::parseChangelog(const QString &text) {
    QVariantList blocks;
    QString title;
    QStringList lines;
    auto flush = [&] {
        while (!lines.isEmpty() && lines.last().isEmpty())
            lines.removeLast();
        if (!title.isEmpty() || !lines.isEmpty())
            blocks << QVariantMap{{QStringLiteral("title"), title}, {QStringLiteral("lines"), lines}};
        title.clear();
        lines.clear();
    };
    QString content = text;
    if (content.startsWith(QChar(0xFEFF)))
        content.remove(0, 1);
    const QStringList all = content.split(QLatin1Char('\n'));
    for (QString line : all) {
        line = line.trimmed();
        if (line.startsWith(QLatin1Char('#'))) {
            flush();
            title = line.mid(1).trimmed();
        } else if (line.startsWith(QLatin1String("---"))) {
            const QString rest = line.mid(3).trimmed();
            if (!lines.isEmpty())
                lines << QString(); // paragraph break
            if (!rest.isEmpty())
                lines << rest;
        } else if (!line.isEmpty()) {
            lines << line;
        }
    }
    flush();
    return blocks;
}

bool AboutViewModel::openUrl(const QString &text) {
    const QUrl url(text, QUrl::StrictMode);
    const QString scheme = url.scheme().toLower();
    if (!url.isValid() || !(scheme == QLatin1String("https") || scheme == QLatin1String("http")
                            || scheme == QLatin1String("mailto")))
        return false;
    if (!m_opener || !m_opener(url))
        return false;
    emit urlOpened(url.toString());
    return true;
}

} // namespace EasePass::App
