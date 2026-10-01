#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QSpinBox>
#include <QComboBox>
#include <QLabel>
#include <QProgressBar>
#include <QTimer>
#include "PasswordItem.h"

namespace EasePass::UI {

class Manage2FADialog : public QDialog {
    Q_OBJECT
public:
    explicit Manage2FADialog(const Core::PasswordItem& item, QWidget* parent = nullptr);

    Core::PasswordItem getUpdatedItem() const;

private slots:
    void onScanFile();
    void onScanClipboard();
    void onScanScreen();
    void onRemove2FA();
    void updatePreview();
    void onToggleShowSecret();

private:
    void setupUi();
    void loadFromUriOrSecret(const QString& text);

    Core::PasswordItem m_item;

    QLineEdit* m_secretEdit = nullptr;
    QSpinBox* m_digitsSpin = nullptr;
    QSpinBox* m_intervalSpin = nullptr;
    QComboBox* m_algorithmCombo = nullptr;

    QLabel* m_previewCodeLabel = nullptr;
    QProgressBar* m_validityProgressBar = nullptr;
    QLabel* m_validityTimeLabel = nullptr;
    QTimer* m_timer = nullptr;
    QPushButton* m_toggleSecretBtn = nullptr;
};

} // namespace EasePass::UI
