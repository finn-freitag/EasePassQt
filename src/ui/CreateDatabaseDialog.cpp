#include "CreateDatabaseDialog.h"
#include "DatabaseManager.h"
#include "DatabaseFile.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QFileDialog>
#include <QMessageBox>
#include <QFileInfo>
#include <QDir>

namespace EasePass::UI {

CreateDatabaseDialog::CreateDatabaseDialog(QWidget* parent)
    : QDialog(parent) {
    setWindowTitle("Create New Database");
    setMinimumWidth(480);
    setupUi();
}

void CreateDatabaseDialog::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(14);

    auto* formLayout = new QFormLayout();
    formLayout->setSpacing(10);

    // Path
    auto* pathLayout = new QHBoxLayout();
    m_pathEdit = new QLineEdit(this);
    m_pathEdit->setText(Core::DatabaseManager::getDefaultDatabasePath());

    auto* browseBtn = new QPushButton("Browse...", this);
    connect(browseBtn, &QPushButton::clicked, this, &CreateDatabaseDialog::onBrowsePath);

    pathLayout->addWidget(m_pathEdit);
    pathLayout->addWidget(browseBtn);
    formLayout->addRow("Database Location:", pathLayout);

    // Password
    auto* pwLayout = new QHBoxLayout();
    m_passwordEdit = new QLineEdit(this);
    m_passwordEdit->setEchoMode(QLineEdit::Password);
    m_passwordEdit->setPlaceholderText("Minimum 4 characters");

    m_togglePasswordBtn = new QPushButton("Show", this);
    m_togglePasswordBtn->setFixedWidth(60);
    connect(m_togglePasswordBtn, &QPushButton::clicked, this, &CreateDatabaseDialog::onToggleShowPassword);

    pwLayout->addWidget(m_passwordEdit);
    pwLayout->addWidget(m_togglePasswordBtn);
    formLayout->addRow("Master Password:", pwLayout);

    // Confirm Password
    m_confirmPasswordEdit = new QLineEdit(this);
    m_confirmPasswordEdit->setEchoMode(QLineEdit::Password);
    m_confirmPasswordEdit->setPlaceholderText("Re-enter master password");
    formLayout->addRow("Confirm Password:", m_confirmPasswordEdit);

    mainLayout->addLayout(formLayout);

    // Bottom action buttons
    auto* btnLayout = new QHBoxLayout();
    btnLayout->addStretch();

    auto* createBtn = new QPushButton("Create Database", this);
    createBtn->setDefault(true);
    connect(createBtn, &QPushButton::clicked, this, &CreateDatabaseDialog::onCreate);

    auto* cancelBtn = new QPushButton("Cancel", this);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);

    btnLayout->addWidget(cancelBtn);
    btnLayout->addWidget(createBtn);
    mainLayout->addLayout(btnLayout);
}

void CreateDatabaseDialog::onBrowsePath() {
    QString def = m_pathEdit->text();
    QString path = QFileDialog::getSaveFileName(this, "Save New Database", def,
                                               "EasePass Database (*.epdb)");
    if (!path.isEmpty()) {
        if (!path.endsWith(".epdb", Qt::CaseInsensitive)) {
            path += ".epdb";
        }
        m_pathEdit->setText(path);
    }
}

void CreateDatabaseDialog::onToggleShowPassword() {
    if (m_passwordEdit->echoMode() == QLineEdit::Password) {
        m_passwordEdit->setEchoMode(QLineEdit::Normal);
        m_confirmPasswordEdit->setEchoMode(QLineEdit::Normal);
        m_togglePasswordBtn->setText("Hide");
    } else {
        m_passwordEdit->setEchoMode(QLineEdit::Password);
        m_confirmPasswordEdit->setEchoMode(QLineEdit::Password);
        m_togglePasswordBtn->setText("Show");
    }
}

void CreateDatabaseDialog::onCreate() {
    QString path = m_pathEdit->text().trimmed();
    if (path.isEmpty()) {
        QMessageBox::warning(this, "Invalid Path", "Please select a destination path for the database.");
        return;
    }

    if (!path.endsWith(".epdb", Qt::CaseInsensitive)) {
        path += ".epdb";
    }

    QString pw = m_passwordEdit->text();
    if (pw.length() < 4) {
        QMessageBox::warning(this, "Weak Password", "The master password must be at least 4 characters long.");
        return;
    }

    if (pw != m_confirmPasswordEdit->text()) {
        QMessageBox::warning(this, "Password Mismatch", "The master passwords do not match. Please re-enter.");
        return;
    }

    QFileInfo fi(path);
    QDir().mkpath(fi.absolutePath());

    if (fi.exists()) {
        auto res = QMessageBox::question(this, "File Exists",
                                         "A database file already exists at this location. Overwrite it?",
                                         QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (res != QMessageBox::Yes) {
            return;
        }
    }

    // Create and save empty database
    Core::DatabaseFile db;
    db.filePath = path;
    db.masterPassword = pw;
    db.databaseFileType = 0;
    db.version = 1.4;
    db.useSecondFactor = false;
    db.secondFactorType = 0;

    if (!db.saveToFile(path)) {
        QMessageBox::critical(this, "Error", "Failed to create database file.");
        return;
    }

    m_createdPath = path;
    m_masterPassword = pw;
    accept();
}

} // namespace EasePass::UI
