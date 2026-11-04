#pragma once

#include <QHash>
#include <QObject>
#include <functional>

namespace EasePass::App {

/**
 * Request/response bridge from C++ to QML dialogs (steps.txt T12). C++ calls ask(); QML
 * listens to dialogRequested, shows a ContentDialog and answers with respond(id, result).
 * Non-modal feedback (InfoBar) goes through showInfo() -> infoRequested.
 * cancelAll() answers every open request with None (e.g. when the database gets locked).
 */
class DialogService : public QObject {
    Q_OBJECT
    Q_PROPERTY(int pendingCount READ pendingCount NOTIFY pendingCountChanged)
public:
    enum class Result { None, Primary, Secondary };
    Q_ENUM(Result)
    enum class Severity { Info, Success, Warning, Error }; // same order as Theme.Severity
    Q_ENUM(Severity)
    using Callback = std::function<void(Result)>;

    using QObject::QObject;

    int pendingCount() const { return int(m_pending.size()); }
    bool isPending(int id) const { return m_pending.contains(id); }

    int ask(const QString &title, const QString &message, const QString &primaryText,
            const QString &secondaryText, const QString &closeText, Callback callback);
    /// Convenience: yes/cancel question.
    int confirm(const QString &title, const QString &message, const QString &primaryText, Callback callback);

    Q_INVOKABLE bool respond(int id, EasePass::App::DialogService::Result result);
    Q_INVOKABLE void cancelAll();
    Q_INVOKABLE void showInfo(EasePass::App::DialogService::Severity severity, const QString &title,
                              const QString &message);

signals:
    void dialogRequested(int id, const QString &title, const QString &message, const QString &primaryText,
                         const QString &secondaryText, const QString &closeText);
    void dialogClosed(int id, EasePass::App::DialogService::Result result);
    void infoRequested(EasePass::App::DialogService::Severity severity, const QString &title,
                       const QString &message);
    void pendingCountChanged();

private:
    int m_nextId = 1;
    QHash<int, Callback> m_pending;
};

} // namespace EasePass::App
