#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QSlider>
#include <QSpinBox>
#include <QCheckBox>
#include <QPushButton>

namespace EasePass::UI {

class PasswordGeneratorDialog : public QDialog {
    Q_OBJECT
public:
    explicit PasswordGeneratorDialog(QWidget* parent = nullptr);

    QString generatedPassword() const;

private slots:
    void regenerate();
    void onCopy();

private:
    void setupUi();

    QLineEdit* m_passwordEdit = nullptr;
    QSlider* m_lengthSlider = nullptr;
    QSpinBox* m_lengthSpin = nullptr;
    QCheckBox* m_upperCheck = nullptr;
    QCheckBox* m_lowerCheck = nullptr;
    QCheckBox* m_digitsCheck = nullptr;
    QCheckBox* m_symbolsCheck = nullptr;
    QLineEdit* m_symbolsEdit = nullptr;
};

} // namespace EasePass::UI
