#pragma once

#include <QWidget>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QProgressBar>

namespace EasePass::UI {

class LoginWidget : public QWidget {
    Q_OBJECT
public:
    explicit LoginWidget(QWidget* parent = nullptr);

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

private:
    void setupUi();
    void populateDatabases();
    void setBusy(bool busy);

    QComboBox* m_dbComboBox = nullptr;
    QLineEdit* m_passwordEdit = nullptr;
    QPushButton* m_togglePasswordBtn = nullptr;
    QPushButton* m_unlockBtn = nullptr;
    QLabel* m_errorLabel = nullptr;
    QProgressBar* m_busyBar = nullptr;
};

} // namespace EasePass::UI
