#include "MainWindow.h"
#include "DatabaseManager.h"
#include "CreateDatabaseDialog.h"
#include "PasswordGeneratorDialog.h"
#include "AboutDialog.h"

#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QFileDialog>
#include <QFileInfo>
#include <QIcon>

namespace EasePass::UI {

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent) {
    setWindowIcon(QIcon(":/assets/appicon.svg"));
    setWindowTitle("Ease Pass");
    resize(980, 640);
    setMinimumSize(700, 480);

    setupUi();
    setupMenuBar();
}

void MainWindow::setupUi() {
    m_stack = new QStackedWidget(this);

    m_loginWidget = new LoginWidget(this);
    connect(m_loginWidget, &LoginWidget::loginSuccess, this, &MainWindow::onLoginSuccess);
    connect(m_loginWidget, &LoginWidget::createNewDatabaseRequested, this, &MainWindow::onCreateNewDatabase);
    connect(m_loginWidget, &LoginWidget::aboutRequested, this, &MainWindow::onAboutRequested);
    m_stack->addWidget(m_loginWidget);

    m_passwordsWidget = new PasswordsWidget(this);
    connect(m_passwordsWidget, &PasswordsWidget::logoutRequested, this, &MainWindow::onLogoutRequested);
    connect(m_passwordsWidget, &PasswordsWidget::aboutRequested, this, &MainWindow::onAboutRequested);
    m_stack->addWidget(m_passwordsWidget);

    setCentralWidget(m_stack);
    m_stack->setCurrentWidget(m_loginWidget);
}

void MainWindow::setupMenuBar() {
    auto* mb = menuBar();

    // File Menu
    auto* fileMenu = mb->addMenu("&File");
    auto* newDbAct = fileMenu->addAction("&New Database...");
    newDbAct->setShortcut(QKeySequence::New);
    connect(newDbAct, &QAction::triggered, this, &MainWindow::onCreateNewDatabase);

    auto* openDbAct = fileMenu->addAction("&Open Database...");
    openDbAct->setShortcut(QKeySequence::Open);
    connect(openDbAct, &QAction::triggered, this, &MainWindow::onOpenDatabase);

    fileMenu->addSeparator();

    auto* lockAct = fileMenu->addAction("&Lock Database");
    lockAct->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_L));
    connect(lockAct, &QAction::triggered, this, &MainWindow::onLogoutRequested);

    fileMenu->addSeparator();

    auto* quitAct = fileMenu->addAction("&Quit");
    quitAct->setShortcut(QKeySequence::Quit);
    connect(quitAct, &QAction::triggered, this, &QWidget::close);

    // Tools Menu
    auto* toolsMenu = mb->addMenu("&Tools");
    auto* genPwAct = toolsMenu->addAction("Password &Generator");
    genPwAct->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_G));
    connect(genPwAct, &QAction::triggered, this, &MainWindow::onOpenPasswordGenerator);

    // Help Menu
    auto* helpMenu = mb->addMenu("&Help");
    auto* aboutAct = helpMenu->addAction("&About Ease Pass");
    aboutAct->setShortcut(QKeySequence::HelpContents);
    connect(aboutAct, &QAction::triggered, this, &MainWindow::onAboutRequested);
}

void MainWindow::onLoginSuccess() {
    auto* db = Core::DatabaseManager::instance().currentDatabase();
    if (db) {
        QFileInfo fi(db->filePath);
        setWindowTitle(QString("Ease Pass - %1").arg(fi.fileName()));
    } else {
        setWindowTitle("Ease Pass");
    }

    m_stack->setCurrentWidget(m_passwordsWidget);
    m_passwordsWidget->refreshList();
}

void MainWindow::onLogoutRequested() {
    Core::DatabaseManager::instance().closeDatabase();
    setWindowTitle("Ease Pass");
    m_loginWidget->reset();
    m_stack->setCurrentWidget(m_loginWidget);
}

void MainWindow::onCreateNewDatabase() {
    CreateDatabaseDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        QString path = dlg.createdPath();
        QString pass = dlg.masterPassword();

        Core::DatabaseManager::instance().addKnownDatabasePath(path);
        Core::DatabaseManager::instance().setLastUsedDatabasePath(path);

        // Directly open the newly created database
        auto db = std::make_unique<Core::DatabaseFile>();
        if (Core::DatabaseFile::loadFromFile(path, pass, *db) == Core::LoadResult::Success) {
            Core::DatabaseManager::instance().setDatabase(std::move(db));
            onLoginSuccess();
        } else {
            m_loginWidget->reset();
        }
    }
}

void MainWindow::onOpenDatabase() {
    QString def = Core::DatabaseManager::instance().getLastUsedDatabasePath();
    QString path = QFileDialog::getOpenFileName(this, "Open EasePass Database", def,
                                               "EasePass Database (*.epdb);;All Files (*)");
    if (!path.isEmpty()) {
        Core::DatabaseManager::instance().addKnownDatabasePath(path);
        Core::DatabaseManager::instance().setLastUsedDatabasePath(path);
        onLogoutRequested();
    }
}

void MainWindow::onOpenPasswordGenerator() {
    PasswordGeneratorDialog dlg(this);
    dlg.exec();
}

void MainWindow::onAboutRequested() {
    AboutDialog dlg(this);
    dlg.exec();
}

} // namespace EasePass::UI
