#pragma once

#include <QList>
#include <QObject>

namespace EasePass::Core { class DatabaseService; }

namespace EasePass::App {

/**
 * Page navigation for the QML shell (steps.txt T12). Pages that need an open database
 * (Passwords, Database) are rejected while it is closed. Opening a database jumps to
 * Passwords, closing/locking it jumps back to Login; both reset the back stack.
 * C#: MainWindow navigation (NavigationHelper / Frame.Navigate).
 */
class AppController : public QObject {
    Q_OBJECT
    Q_PROPERTY(Page page READ page NOTIFY pageChanged)
    Q_PROPERTY(bool canGoBack READ canGoBack NOTIFY pageChanged)
    Q_PROPERTY(bool databaseOpen READ databaseOpen NOTIFY databaseOpenChanged)
public:
    enum class Page { Login, Register, Passwords, Database, Settings, About, Plugins };
    Q_ENUM(Page)

    explicit AppController(Core::DatabaseService *service, QObject *parent = nullptr);

    Page page() const { return m_page; }
    bool canGoBack() const { return !m_history.isEmpty(); }
    bool databaseOpen() const;

    static bool requiresOpenDatabase(Page page);

    Q_INVOKABLE bool navigate(EasePass::App::AppController::Page page);
    Q_INVOKABLE bool goBack();
    /// Start page: Register (OOBE) on first run, else Login. Clears the back stack.
    Q_INVOKABLE void start(bool firstRun);

signals:
    void pageChanged(EasePass::App::AppController::Page page);
    void navigationRejected(EasePass::App::AppController::Page page);
    void databaseOpenChanged();

private:
    void resetTo(Page page);

    Core::DatabaseService *m_service;
    Page m_page = Page::Login;
    QList<Page> m_history;
};

} // namespace EasePass::App
