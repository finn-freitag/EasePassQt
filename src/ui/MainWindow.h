#pragma once

#include <QMainWindow>
#include <QStackedWidget>
#include "LoginWidget.h"
#include "PasswordsWidget.h"

namespace EasePass::UI {

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

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

    QStackedWidget* m_stack = nullptr;
    LoginWidget* m_loginWidget = nullptr;
    PasswordsWidget* m_passwordsWidget = nullptr;
};

} // namespace EasePass::UI
