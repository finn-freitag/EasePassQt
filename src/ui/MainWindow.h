#pragma once

#include <QMainWindow>
#include <QStackedWidget>
#include "LoginWidget.h"
#include "PasswordsWidget.h"

namespace EasePass::Core {
class DatabaseService;
} // namespace EasePass::Core

namespace EasePass::UI {

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    void onLoginSuccess();
    void onLogoutRequested();
    void onCreateNewDatabase();
    void onOpenDatabase();
    void onAboutRequested();
    void onOpenPasswordGenerator();

private:
    void setupUi();
    void setupMenuBar();

    Core::DatabaseService* m_service = nullptr;
    QStackedWidget* m_stack = nullptr;
    LoginWidget* m_loginWidget = nullptr;
    PasswordsWidget* m_passwordsWidget = nullptr;
};

} // namespace EasePass::UI
