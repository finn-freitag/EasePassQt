#include "LoginWidget.h"
#include "DatabaseManager.h"
#include "DatabaseFile.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QFileInfo>
#include <QThread>
#include <QInputDialog>
#include <QIcon>
#include <QFont>

namespace EasePass::UI {

LoginWidget::LoginWidget(QWidget* parent)
    : QWidget(parent) {
    setupUi();
    reset();
}

void LoginWidget::reset() {
    m_passwordEdit->clear();
    m_errorLabel->clear();
    setBusy(false);
    populateDatabases();
    m_passwordEdit->setFocus();
}

void LoginWidget::setupUi() {
    auto* outerLayout = new QVBoxLayout(this);
    outerLayout->setAlignment(Qt::AlignCenter);

    auto* card = new QWidget(this);
    card->setFixedWidth(380);
    card->setStyleSheet("QWidget#LoginCard { background: palette(alternate-base); border-radius: 12px; padding: 24px; }");
    card->setObjectName("LoginCard");

    auto* cardLayout = new QVBoxLayout(card);
    cardLayout->setSpacing(14);
    cardLayout->setAlignment(Qt::AlignCenter);

    // App Icon
    auto* iconLabel = new QLabel(card);
    QIcon icon(":/assets/appicon.svg");
    if (!icon.isNull()) {
        iconLabel->setPixmap(icon.pixmap(72, 72));
    }
    iconLabel->setAlignment(Qt::AlignCenter);
    cardLayout->addWidget(iconLabel);

    // App Title
    auto* titleLabel = new QLabel("Ease Pass", card);
    titleLabel->setFont(QFont("sans-serif", 20, QFont::Bold));
    titleLabel->setAlignment(Qt::AlignCenter);
    cardLayout->addWidget(titleLabel);

    auto* subLabel = new QLabel("Enter master password to unlock", card);
    subLabel->setStyleSheet("color: gray; font-size: 12px;");
    subLabel->setAlignment(Qt::AlignCenter);
    cardLayout->addWidget(subLabel);

    cardLayout->addSpacing(6);

    // Database selector
    auto* dbLabel = new QLabel("Database:", card);
    dbLabel->setStyleSheet("color: gray; font-size: 11px; font-weight: bold;");
    cardLayout->addWidget(dbLabel);

    m_dbComboBox = new QComboBox(card);
    connect(m_dbComboBox, &QComboBox::currentIndexChanged, this, &LoginWidget::onDatabaseComboChanged);
    cardLayout->addWidget(m_dbComboBox);

    // Password row
    auto* pwLabel = new QLabel("Master Password:", card);
    pwLabel->setStyleSheet("color: gray; font-size: 11px; font-weight: bold;");
    cardLayout->addWidget(pwLabel);

    auto* pwLayout = new QHBoxLayout();
    m_passwordEdit = new QLineEdit(card);
    m_passwordEdit->setEchoMode(QLineEdit::Password);
    m_passwordEdit->setPlaceholderText("Master Password");
    connect(m_passwordEdit, &QLineEdit::returnPressed, this, &LoginWidget::onUnlockClicked);

    m_togglePasswordBtn = new QPushButton("Show", card);
    m_togglePasswordBtn->setFixedWidth(56);
    connect(m_togglePasswordBtn, &QPushButton::clicked, this, &LoginWidget::onToggleShowPassword);

    pwLayout->addWidget(m_passwordEdit);
    pwLayout->addWidget(m_togglePasswordBtn);
    cardLayout->addLayout(pwLayout);

    // Error label
    m_errorLabel = new QLabel(card);
    m_errorLabel->setStyleSheet("color: #d9534f; font-size: 11px;");
    m_errorLabel->setAlignment(Qt::AlignCenter);
    m_errorLabel->setWordWrap(true);
    cardLayout->addWidget(m_errorLabel);

    // Busy bar
    m_busyBar = new QProgressBar(card);
    m_busyBar->setRange(0, 0);
    m_busyBar->setFixedHeight(4);
    m_busyBar->setTextVisible(false);
    m_busyBar->hide();
    cardLayout->addWidget(m_busyBar);

    // Unlock button
    m_unlockBtn = new QPushButton("Unlock Database", card);
    m_unlockBtn->setFixedHeight(36);
    m_unlockBtn->setStyleSheet("QPushButton { font-weight: bold; font-size: 13px; }");
    connect(m_unlockBtn, &QPushButton::clicked, this, &LoginWidget::onUnlockClicked);
    cardLayout->addWidget(m_unlockBtn);

    // Footer actions
    auto* footerLayout = new QHBoxLayout();
    auto* newDbBtn = new QPushButton("New Database", card);
    newDbBtn->setFlat(true);
    connect(newDbBtn, &QPushButton::clicked, this, &LoginWidget::createNewDatabaseRequested);

    auto* aboutBtn = new QPushButton("About", card);
    aboutBtn->setFlat(true);
    connect(aboutBtn, &QPushButton::clicked, this, &LoginWidget::aboutRequested);

    footerLayout->addWidget(newDbBtn);
    footerLayout->addStretch();
    footerLayout->addWidget(aboutBtn);
    cardLayout->addLayout(footerLayout);

    outerLayout->addWidget(card);
}

void LoginWidget::populateDatabases() {
    m_dbComboBox->blockSignals(true);
    m_dbComboBox->clear();

    QStringList paths = Core::DatabaseManager::instance().getKnownDatabasePaths();
    QString lastUsed = Core::DatabaseManager::instance().getLastUsedDatabasePath();

    int selectIdx = 0;
    for (int i = 0; i < paths.size(); ++i) {
        QFileInfo fi(paths[i]);
        m_dbComboBox->addItem(fi.fileName(), paths[i]);
        if (paths[i] == lastUsed) {
            selectIdx = i;
        }
    }

    m_dbComboBox->insertSeparator(m_dbComboBox->count());
    m_dbComboBox->addItem("📂 Browse other database...", "__browse__");
    m_dbComboBox->addItem("➕ Create new database...", "__create__");

    if (m_dbComboBox->count() > 0) {
        m_dbComboBox->setCurrentIndex(selectIdx);
    }

    m_dbComboBox->blockSignals(false);
}

void LoginWidget::onDatabaseComboChanged(int index) {
    if (index < 0) return;
    QString data = m_dbComboBox->itemData(index).toString();

    if (data == "__browse__") {
        onBrowseDatabase();
    } else if (data == "__create__") {
        emit createNewDatabaseRequested();
    } else {
        Core::DatabaseManager::instance().setLastUsedDatabasePath(data);
    }
}

void LoginWidget::onBrowseDatabase() {
    QString def = Core::DatabaseManager::instance().getLastUsedDatabasePath();
    QString path = QFileDialog::getOpenFileName(this, "Open EasePass Database", def,
                                               "EasePass Database (*.epdb);;All Files (*)");
    if (!path.isEmpty()) {
        Core::DatabaseManager::instance().addKnownDatabasePath(path);
        Core::DatabaseManager::instance().setLastUsedDatabasePath(path);
        populateDatabases();
    } else {
        populateDatabases();
    }
}

void LoginWidget::onToggleShowPassword() {
    if (m_passwordEdit->echoMode() == QLineEdit::Password) {
        m_passwordEdit->setEchoMode(QLineEdit::Normal);
        m_togglePasswordBtn->setText("Hide");
    } else {
        m_passwordEdit->setEchoMode(QLineEdit::Password);
        m_togglePasswordBtn->setText("Show");
    }
}

void LoginWidget::setBusy(bool busy) {
    m_passwordEdit->setEnabled(!busy);
    m_togglePasswordBtn->setEnabled(!busy);
    m_unlockBtn->setEnabled(!busy);
    m_dbComboBox->setEnabled(!busy);
    if (busy) {
        m_busyBar->show();
        m_unlockBtn->setText("Unlocking...");
    } else {
        m_busyBar->hide();
        m_unlockBtn->setText("Unlock Database");
    }
}

void LoginWidget::onUnlockClicked() {
    QString path = m_dbComboBox->currentData().toString();
    if (path.isEmpty() || path.startsWith("__")) {
        m_errorLabel->setText("Please select or create a database first.");
        return;
    }

    if (!QFile::exists(path)) {
        m_errorLabel->setText("Database file not found at the selected path.");
        return;
    }

    QString password = m_passwordEdit->text();
    if (password.isEmpty()) {
        m_errorLabel->setText("Please enter your master password.");
        return;
    }

    m_errorLabel->clear();
    setBusy(true);

    // Run Argon2 and AES decryption in worker thread
    auto* workerThread = QThread::create([this, path, password]() {
        auto db = std::make_unique<Core::DatabaseFile>();
        Core::LoadResult res = Core::DatabaseFile::loadFromFile(path, password, *db);

        QMetaObject::invokeMethod(this, [this, db = std::move(db), res, path, password]() mutable {
            if (res == Core::LoadResult::NeedsSecondFactor) {
                bool ok = false;
                QString token = QInputDialog::getText(this, "Second Factor Authentication",
                                                      "Please enter your 2FA token:",
                                                      QLineEdit::Password, QString(), &ok);
                if (!ok || token.isEmpty()) {
                    setBusy(false);
                    m_errorLabel->setText("Second factor required.");
                    return;
                }

                // Retry with second factor
                auto* retryThread = QThread::create([this, path, password, token]() {
                    auto db2 = std::make_unique<Core::DatabaseFile>();
                    Core::LoadResult res2 = Core::DatabaseFile::loadFromFile(path, password, *db2, token);
                    QMetaObject::invokeMethod(this, [this, db2 = std::move(db2), res2]() mutable {
                        setBusy(false);
                        if (res2 == Core::LoadResult::Success) {
                            Core::DatabaseManager::instance().setDatabase(std::move(db2));
                            emit loginSuccess();
                        } else if (res2 == Core::LoadResult::WrongPassword) {
                            m_errorLabel->setText("Incorrect master password or 2FA token.");
                        } else {
                            m_errorLabel->setText("Failed to decrypt database.");
                        }
                    });
                });
                connect(retryThread, &QThread::finished, retryThread, &QObject::deleteLater);
                retryThread->start();
                return;
            }

            setBusy(false);

            if (res == Core::LoadResult::Success) {
                Core::DatabaseManager::instance().setDatabase(std::move(db));
                emit loginSuccess();
            } else if (res == Core::LoadResult::WrongPassword) {
                m_errorLabel->setText("Incorrect master password.");
                m_passwordEdit->selectAll();
                m_passwordEdit->setFocus();
            } else if (res == Core::LoadResult::WrongFormat) {
                m_errorLabel->setText("Invalid database format or corrupted file.");
            } else if (res == Core::LoadResult::DatabaseNotFound) {
                m_errorLabel->setText("Database file could not be found.");
            } else {
                m_errorLabel->setText("An error occurred while loading the database.");
            }
        });
    });

    connect(workerThread, &QThread::finished, workerThread, &QObject::deleteLater);
    workerThread->start();
}

} // namespace EasePass::UI
