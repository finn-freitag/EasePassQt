#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QPushButton>

namespace EasePass::UI {

class CreateDatabaseDialog : public QDialog {
    Q_OBJECT
public:
    explicit CreateDatabaseDialog(QWidget* parent = nullptr);

    QString createdPath() const { return m_createdPath; }
    QString masterPassword() const { return m_masterPassword; }

private slots:
    void onBrowsePath();
    void onToggleShowPassword();
    void onCreate();

private:
    void setupUi();

    QLineEdit* m_pathEdit = nullptr;
    QLineEdit* m_passwordEdit = nullptr;
    QLineEdit* m_confirmPasswordEdit = nullptr;
    QPushButton* m_togglePasswordBtn = nullptr;

    QString m_createdPath;
    QString m_masterPassword;
};

} // namespace EasePass::UI
