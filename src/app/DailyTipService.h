#pragma once

#include <QDate>
#include <QObject>
#include <QStringList>

namespace EasePass::Core { class IClock; }

namespace EasePass::App {

/**
 * "Tip of the day" for the login page (steps.txt T13). C#: Helper/App/DailyTipHelper.cs.
 * The C# app downloads resources/DailyTips_<lang>.txt from GitHub on every start; here the
 * files are bundled in qrc (:/dailytips/), so no network request is made (privacy, offline).
 * One non-empty line per tip; the tip changes once per day and is the same all day.
 * Unknown languages fall back to en-US.
 */
class DailyTipService : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString language READ language WRITE setLanguage NOTIFY languageChanged)
    Q_PROPERTY(QString tip READ tip NOTIFY tipChanged)
    Q_PROPERTY(bool hasTip READ hasTip NOTIFY tipChanged)
public:
    static inline const QString FallbackLanguage = QStringLiteral("en-US");

    explicit DailyTipService(const Core::IClock *clock = nullptr, QObject *parent = nullptr);

    QString language() const { return m_language; }
    void setLanguage(const QString &language);
    QString tip() const { return m_tip; }
    bool hasTip() const { return !m_tip.isEmpty(); }

    /// Overrides the directory the tip files are read from (default ":/dailytips"). Tests only.
    void setTipDirectory(const QString &dir);

    /// Splits a tip file into tips (C#: Split on \r/\n, RemoveEmptyEntries).
    static QStringList parseTips(const QString &text);
    /// Deterministic choice for @p date. C# seeds .NET's Random with MD5(date); that generator
    /// cannot be reproduced portably, so the index is MD5(ISO date) mod count instead.
    static int tipIndexForDate(const QDate &date, int count);

    Q_INVOKABLE void reload();

signals:
    void languageChanged();
    void tipChanged();

private:
    QStringList loadTips(const QString &language) const;

    const Core::IClock *m_clock;
    QString m_language = FallbackLanguage;
    QString m_directory = QStringLiteral(":/dailytips");
    QString m_tip;
};

} // namespace EasePass::App
