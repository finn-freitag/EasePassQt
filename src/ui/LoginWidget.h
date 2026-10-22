#pragma once

#include <QWidget>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QProgressBar>
#include <memory>

#include "DatabaseFile.h"
#include "SecretString.h"

namespace EasePass::Core {
class DatabaseService;
class IDatabaseSource;
} // namespace EasePass::Core

namespace EasePass::UI {

class LoginWidget : public QWidget {
    Q_OBJECT
public:
    explicit LoginWidget(Core::DatabaseService* service, QWidget* parent = nullptr);

    void reset();

signals:
    void loginSuccess();
    void createNewDatabaseRequested();
    void aboutRequested();

private slots:
    void onDatabaseComboChanged(int index);
    void onUnlockClicked();
    void onToggleShowPassword();
    void onBrowseDatabase();
    void onOpenFinished(Core::LoadResult result);

private:
    void setupUi();
    void populateDatabases();
    void setBusy(bool busy);
    void finishUnlock();

    Core::DatabaseService* m_service = nullptr;
    // State of an unlock started by this widget (the service is shared with MainWindow)
    bool m_unlockInProgress = false;
    std::shared_ptr<Core::IDatabaseSource> m_pendingSource;
    QString m_pendingPath;
    Core::SecretString m_pendingPassword;

    QComboBox* m_dbComboBox = nullptr;
    QLineEdit* m_passwordEdit = nullptr;
    QPushButton* m_togglePasswordBtn = nullptr;
    QPushButton* m_unlockBtn = nullptr;
    QLabel* m_errorLabel = nullptr;
    QProgressBar* m_busyBar = nullptr;
};

} // namespace EasePass::UI
