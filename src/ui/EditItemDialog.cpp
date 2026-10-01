#include "EditItemDialog.h"
#include "Manage2FADialog.h"
#include "PasswordGeneratorDialog.h"
#include "WebsiteIconManager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QMessageBox>
#include <QIcon>

namespace EasePass::UI {

EditItemDialog::EditItemDialog(QWidget* parent)
    : QDialog(parent), m_isEditMode(false) {
    setWindowTitle("New Password Entry");
    setMinimumWidth(480);
    setupUi();
}

EditItemDialog::EditItemDialog(const Core::PasswordItem& item, QWidget* parent)
    : QDialog(parent), m_item(item), m_isEditMode(true) {
    setWindowTitle("Edit Password Entry");
    setMinimumWidth(480);
    setupUi();
}

void EditItemDialog::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(14);

    auto* formLayout = new QFormLayout();
    formLayout->setSpacing(10);
    formLayout->setLabelAlignment(Qt::AlignRight);

    // Display Name
    m_displayNameEdit = new QLineEdit(this);
    m_displayNameEdit->setText(m_item.displayName);
    m_displayNameEdit->setPlaceholderText("e.g. Google, GitHub, Bank");
    formLayout->addRow("Display Name*:", m_displayNameEdit);

    // Website
    auto* webLayout = new QHBoxLayout();
    m_websiteIconLabel = new QLabel(this);
    m_websiteIconLabel->setFixedSize(24, 24);

    m_websiteEdit = new QLineEdit(this);
    m_websiteEdit->setText(m_item.website);
    m_websiteEdit->setPlaceholderText("https://example.com");
    connect(m_websiteEdit, &QLineEdit::textChanged, this, &EditItemDialog::onWebsiteChanged);

    webLayout->addWidget(m_websiteIconLabel);
    webLayout->addWidget(m_websiteEdit);
    formLayout->addRow("Website:", webLayout);

    // Username
    m_usernameEdit = new QLineEdit(this);
    m_usernameEdit->setText(m_item.username);
    m_usernameEdit->setPlaceholderText("Username");
    formLayout->addRow("Username:", m_usernameEdit);

    // Email
    m_emailEdit = new QLineEdit(this);
    m_emailEdit->setText(m_item.email);
    m_emailEdit->setPlaceholderText("user@example.com");
    formLayout->addRow("Email:", m_emailEdit);

    // Password
    auto* pwLayout = new QHBoxLayout();
    m_passwordEdit = new QLineEdit(this);
    m_passwordEdit->setEchoMode(QLineEdit::Password);
    m_passwordEdit->setText(m_item.password);

    m_togglePasswordBtn = new QPushButton("Show", this);
    m_togglePasswordBtn->setFixedWidth(60);
    connect(m_togglePasswordBtn, &QPushButton::clicked, this, &EditItemDialog::onTogglePasswordEcho);

    auto* genBtn = new QPushButton("Generate", this);
    connect(genBtn, &QPushButton::clicked, this, &EditItemDialog::onGeneratePassword);

    pwLayout->addWidget(m_passwordEdit);
    pwLayout->addWidget(m_togglePasswordBtn);
    pwLayout->addWidget(genBtn);
    formLayout->addRow("Password:", pwLayout);

    // 2FA button
    m_configure2FABtn = new QPushButton(this);
    connect(m_configure2FABtn, &QPushButton::clicked, this, &EditItemDialog::onConfigure2FA);
    update2FAButtonState();
    formLayout->addRow("2FA Authentication:", m_configure2FABtn);

    // Tags
    m_tagsEdit = new QLineEdit(this);
    m_tagsEdit->setText(m_item.tags.join(", "));
    m_tagsEdit->setPlaceholderText("Comma-separated (e.g. Work, Social, Finance)");
    formLayout->addRow("Tags:", m_tagsEdit);

    // Notes
    m_notesEdit = new QTextEdit(this);
    m_notesEdit->setPlainText(m_item.notes);
    m_notesEdit->setFixedHeight(80);
    formLayout->addRow("Notes:", m_notesEdit);

    mainLayout->addLayout(formLayout);

    // Dialog buttons
    auto* btnLayout = new QHBoxLayout();
    btnLayout->addStretch();

    auto* saveBtn = new QPushButton(m_isEditMode ? "Save Changes" : "Create Entry", this);
    saveBtn->setDefault(true);
    connect(saveBtn, &QPushButton::clicked, this, [this]() {
        if (m_displayNameEdit->text().trimmed().isEmpty()) {
            QMessageBox::warning(this, "Missing Name", "Please enter a Display Name for this entry.");
            m_displayNameEdit->setFocus();
            return;
        }
        accept();
    });

    auto* cancelBtn = new QPushButton("Cancel", this);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);

    btnLayout->addWidget(cancelBtn);
    btnLayout->addWidget(saveBtn);
    mainLayout->addLayout(btnLayout);

    onWebsiteChanged();
}

void EditItemDialog::onWebsiteChanged() {
    QString site = m_websiteEdit->text().trimmed();
    if (!site.isEmpty()) {
        QPixmap icon = Core::WebsiteIconManager::instance().getIcon(site, 24);
        if (!icon.isNull()) {
            m_websiteIconLabel->setPixmap(icon);
            return;
        }
    }
    m_websiteIconLabel->clear();
}

void EditItemDialog::update2FAButtonState() {
    if (m_item.has2FA()) {
        m_configure2FABtn->setText(QString("✓ 2FA Enabled (%1, %2 digits)").arg(m_item.algorithm, m_item.digits));
        m_configure2FABtn->setStyleSheet("color: #28a745; font-weight: bold;");
    } else {
        m_configure2FABtn->setText("Set Up 2FA...");
        m_configure2FABtn->setStyleSheet("");
    }
}

void EditItemDialog::onConfigure2FA() {
    Manage2FADialog dlg(m_item, this);
    if (dlg.exec() == QDialog::Accepted) {
        m_item = dlg.getUpdatedItem();
        update2FAButtonState();
    }
}

void EditItemDialog::onGeneratePassword() {
    PasswordGeneratorDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        m_passwordEdit->setText(dlg.generatedPassword());
        m_passwordEdit->setEchoMode(QLineEdit::Normal);
        m_togglePasswordBtn->setText("Hide");
    }
}

void EditItemDialog::onTogglePasswordEcho() {
    if (m_passwordEdit->echoMode() == QLineEdit::Password) {
        m_passwordEdit->setEchoMode(QLineEdit::Normal);
        m_togglePasswordBtn->setText("Hide");
    } else {
        m_passwordEdit->setEchoMode(QLineEdit::Password);
        m_togglePasswordBtn->setText("Show");
    }
}

Core::PasswordItem EditItemDialog::getItem() const {
    Core::PasswordItem res = m_item;
    res.displayName = m_displayNameEdit->text().trimmed();
    res.website = m_websiteEdit->text().trimmed();
    res.username = m_usernameEdit->text().trimmed();
    res.email = m_emailEdit->text().trimmed();
    res.password = m_passwordEdit->text();
    res.notes = m_notesEdit->toPlainText();

    QString tagsStr = m_tagsEdit->text();
    QStringList tagList;
    for (const auto& t : tagsStr.split(",")) {
        QString trimmed = t.trimmed();
        if (!trimmed.isEmpty() && !tagList.contains(trimmed)) {
            tagList.append(trimmed);
        }
    }
    res.tags = tagList;

    return res;
}

} // namespace EasePass::UI
