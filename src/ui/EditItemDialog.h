#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QTextEdit>
#include <QLabel>
#include <QPushButton>
#include "PasswordItem.h"

namespace EasePass::UI {

class EditItemDialog : public QDialog {
    Q_OBJECT
public:
    explicit EditItemDialog(QWidget* parent = nullptr);
    explicit EditItemDialog(const Core::PasswordItem& item, QWidget* parent = nullptr);

    Core::PasswordItem getItem() const;

private slots:
    void onGeneratePassword();
    void onTogglePasswordEcho();
    void onConfigure2FA();
    void onWebsiteChanged();

private:
    void setupUi();
    void update2FAButtonState();

    Core::PasswordItem m_item;
    bool m_isEditMode = false;

    QLineEdit* m_displayNameEdit = nullptr;
    QLineEdit* m_websiteEdit = nullptr;
    QLabel* m_websiteIconLabel = nullptr;
    QLineEdit* m_usernameEdit = nullptr;
    QLineEdit* m_emailEdit = nullptr;
    QLineEdit* m_passwordEdit = nullptr;
    QPushButton* m_togglePasswordBtn = nullptr;
    QLineEdit* m_tagsEdit = nullptr;
    QTextEdit* m_notesEdit = nullptr;
    QPushButton* m_configure2FABtn = nullptr;
};

} // namespace EasePass::UI
