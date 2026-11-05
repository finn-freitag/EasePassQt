#include "DailyTipService.h"

#include "IClock.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QFile>
#include <QRegularExpression>
#include <QtEndian>

namespace EasePass::App {

DailyTipService::DailyTipService(const Core::IClock *clock, QObject *parent)
    : QObject(parent), m_clock(clock ? clock : &Core::SystemClock::instance()) {
    reload();
}

void DailyTipService::setLanguage(const QString &language) {
    const QString lang = language.isEmpty() ? FallbackLanguage : language;
    if (lang == m_language)
        return;
    m_language = lang;
    emit languageChanged();
    reload();
}

void DailyTipService::setTipDirectory(const QString &dir) {
    m_directory = dir;
    reload();
}

QStringList DailyTipService::parseTips(const QString &text) {
    QStringList tips;
    for (const QString &line : text.split(QRegularExpression(QStringLiteral("[\r\n]")), Qt::SkipEmptyParts)) {
        const QString t = line.trimmed();
        if (!t.isEmpty())
            tips << t;
    }
    return tips;
}

int DailyTipService::tipIndexForDate(const QDate &date, int count) {
    if (count <= 0)
        return -1;
    const QByteArray hash =
        QCryptographicHash::hash(date.toString(Qt::ISODate).toUtf8(), QCryptographicHash::Md5);
    const quint32 v = qFromLittleEndian<quint32>(hash.constData());
    return int(v % quint32(count));
}

QStringList DailyTipService::loadTips(const QString &language) const {
    QFile f(m_directory + QStringLiteral("/DailyTips_%1.txt").arg(language));
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    return parseTips(QString::fromUtf8(f.readAll()));
}

void DailyTipService::reload() {
    QStringList tips = loadTips(m_language);
    if (tips.isEmpty() && m_language != FallbackLanguage)
        tips = loadTips(FallbackLanguage);
    const QDate today = QDateTime::fromSecsSinceEpoch(m_clock->currentSecsSinceEpoch()).date();
    const int idx = tipIndexForDate(today, int(tips.size()));
    const QString tip = idx >= 0 ? tips.at(idx) : QString();
    if (tip == m_tip)
        return;
    m_tip = tip;
    emit tipChanged();
}

} // namespace EasePass::App
