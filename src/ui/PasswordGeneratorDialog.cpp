#include "PasswordGeneratorDialog.h"
#include "PasswordGenerator.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QGuiApplication>
#include <QClipboard>
#include <QFont>

namespace EasePass::UI {

PasswordGeneratorDialog::PasswordGeneratorDialog(QWidget* parent)
    : QDialog(parent) {
    setWindowTitle("Password Generator");
    setMinimumWidth(420);
    setupUi();
    regenerate();
}

QString PasswordGeneratorDialog::generatedPassword() const {
    return m_passwordEdit->text();
}

void PasswordGeneratorDialog::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(14);

    // Password display & quick actions
    auto* dispLayout = new QHBoxLayout();
    m_passwordEdit = new QLineEdit(this);
    m_passwordEdit->setFont(QFont("Monospace", 14, QFont::Bold));
    m_passwordEdit->setAlignment(Qt::AlignCenter);

    auto* regenBtn = new QPushButton("↻", this);
    regenBtn->setToolTip("Regenerate");
    regenBtn->setFixedWidth(36);
    connect(regenBtn, &QPushButton::clicked, this, &PasswordGeneratorDialog::regenerate);

    auto* copyBtn = new QPushButton("Copy", this);
    connect(copyBtn, &QPushButton::clicked, this, &PasswordGeneratorDialog::onCopy);

    dispLayout->addWidget(m_passwordEdit);
    dispLayout->addWidget(regenBtn);
    dispLayout->addWidget(copyBtn);
    mainLayout->addLayout(dispLayout);

    // Options group
    auto* optGroup = new QGroupBox("Options", this);
    auto* optLayout = new QVBoxLayout(optGroup);
    optLayout->setSpacing(10);

    // Length
    auto* lenLayout = new QHBoxLayout();
    m_lengthSlider = new QSlider(Qt::Horizontal, this);
    m_lengthSlider->setRange(4, 64);
    m_lengthSlider->setValue(16);

    m_lengthSpin = new QSpinBox(this);
    m_lengthSpin->setRange(4, 64);
    m_lengthSpin->setValue(16);

    connect(m_lengthSlider, &QSlider::valueChanged, m_lengthSpin, &QSpinBox::setValue);
    connect(m_lengthSpin, &QSpinBox::valueChanged, m_lengthSlider, &QSlider::setValue);
    connect(m_lengthSlider, &QSlider::valueChanged, this, &PasswordGeneratorDialog::regenerate);

    lenLayout->addWidget(m_lengthSlider);
    lenLayout->addWidget(m_lengthSpin);
    optLayout->addLayout(lenLayout);

    // Character categories
    m_upperCheck = new QCheckBox("Uppercase letters (A-Z)", this);
    m_upperCheck->setChecked(true);
    connect(m_upperCheck, &QCheckBox::toggled, this, &PasswordGeneratorDialog::regenerate);
    optLayout->addWidget(m_upperCheck);

    m_lowerCheck = new QCheckBox("Lowercase letters (a-z)", this);
    m_lowerCheck->setChecked(true);
    connect(m_lowerCheck, &QCheckBox::toggled, this, &PasswordGeneratorDialog::regenerate);
    optLayout->addWidget(m_lowerCheck);

    m_digitsCheck = new QCheckBox("Numbers (0-9)", this);
    m_digitsCheck->setChecked(true);
    connect(m_digitsCheck, &QCheckBox::toggled, this, &PasswordGeneratorDialog::regenerate);
    optLayout->addWidget(m_digitsCheck);

    m_symbolsCheck = new QCheckBox("Special symbols", this);
    m_symbolsCheck->setChecked(true);
    connect(m_symbolsCheck, &QCheckBox::toggled, this, &PasswordGeneratorDialog::regenerate);
    optLayout->addWidget(m_symbolsCheck);

    m_symbolsEdit = new QLineEdit("!@#$%^&*()-_=+[]{}|;:,.<>?", this);
    connect(m_symbolsEdit, &QLineEdit::textChanged, this, &PasswordGeneratorDialog::regenerate);
    optLayout->addWidget(m_symbolsEdit);

    mainLayout->addWidget(optGroup);

    // Bottom action buttons
    auto* btnLayout = new QHBoxLayout();
    btnLayout->addStretch();

    auto* useBtn = new QPushButton("Use Password", this);
    useBtn->setDefault(true);
    connect(useBtn, &QPushButton::clicked, this, &QDialog::accept);

    auto* cancelBtn = new QPushButton("Cancel", this);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);

    btnLayout->addWidget(cancelBtn);
    btnLayout->addWidget(useBtn);
    mainLayout->addLayout(btnLayout);
}

void PasswordGeneratorDialog::regenerate() {
    Core::PasswordGeneratorOptions opts;
    opts.length = m_lengthSpin->value();
    opts.useUpper = m_upperCheck->isChecked();
    opts.useLower = m_lowerCheck->isChecked();
    opts.useDigits = m_digitsCheck->isChecked();
    opts.useSymbols = m_symbolsCheck->isChecked();
    opts.customSymbols = m_symbolsEdit->text();

    m_passwordEdit->setText(Core::PasswordGenerator::generate(opts));
}

void PasswordGeneratorDialog::onCopy() {
    QClipboard* clipboard = QGuiApplication::clipboard();
    if (clipboard) {
        clipboard->setText(m_passwordEdit->text());
    }
}

} // namespace EasePass::UI
