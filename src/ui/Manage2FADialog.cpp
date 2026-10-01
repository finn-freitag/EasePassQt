#include "Manage2FADialog.h"
#include "TotpHelper.h"
#include "QrCodeScanner.h"
#include "ScreenCaptureDialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QPushButton>
#include <QFileDialog>
#include <QMessageBox>
#include <QFont>

namespace EasePass::UI {

Manage2FADialog::Manage2FADialog(const Core::PasswordItem& item, QWidget* parent)
    : QDialog(parent), m_item(item) {
    setWindowTitle("Two-Factor Authentication (2FA)");
    setMinimumWidth(440);
    setupUi();

    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &Manage2FADialog::updatePreview);
    m_timer->start(200);

    updatePreview();
}

Core::PasswordItem Manage2FADialog::getUpdatedItem() const {
    Core::PasswordItem res = m_item;
    res.secret = m_secretEdit->text().trimmed();
    res.digits = QString::number(m_digitsSpin->value());
    res.interval = QString::number(m_intervalSpin->value());
    res.algorithm = m_algorithmCombo->currentText();
    return res;
}

void Manage2FADialog::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(16);

    // QR Code actions box
    auto* qrGroup = new QGroupBox("Scan 2FA Setup QR Code", this);
    auto* qrLayout = new QHBoxLayout(qrGroup);
    qrLayout->setSpacing(10);

    auto* scanFileBtn = new QPushButton("Scan Image...", this);
    auto* scanClipBtn = new QPushButton("Scan Clipboard", this);
    auto* scanScreenBtn = new QPushButton("Scan Screen...", this);

    connect(scanFileBtn, &QPushButton::clicked, this, &Manage2FADialog::onScanFile);
    connect(scanClipBtn, &QPushButton::clicked, this, &Manage2FADialog::onScanClipboard);
    connect(scanScreenBtn, &QPushButton::clicked, this, &Manage2FADialog::onScanScreen);

    qrLayout->addWidget(scanFileBtn);
    qrLayout->addWidget(scanClipBtn);
    qrLayout->addWidget(scanScreenBtn);
    mainLayout->addWidget(qrGroup);

    // Parameters box
    auto* paramGroup = new QGroupBox("2FA Secret & Parameters", this);
    auto* formLayout = new QFormLayout(paramGroup);
    formLayout->setSpacing(10);

    auto* secretLayout = new QHBoxLayout();
    m_secretEdit = new QLineEdit(this);
    m_secretEdit->setEchoMode(QLineEdit::Password);
    m_secretEdit->setText(m_item.secret);
    m_secretEdit->setPlaceholderText("Base32 secret key (e.g. JBSWY3DPEHPK3PXP)");
    connect(m_secretEdit, &QLineEdit::textChanged, this, &Manage2FADialog::updatePreview);

    m_toggleSecretBtn = new QPushButton("Show", this);
    m_toggleSecretBtn->setFixedWidth(60);
    connect(m_toggleSecretBtn, &QPushButton::clicked, this, &Manage2FADialog::onToggleShowSecret);

    secretLayout->addWidget(m_secretEdit);
    secretLayout->addWidget(m_toggleSecretBtn);
    formLayout->addRow("Secret Key:", secretLayout);

    m_digitsSpin = new QSpinBox(this);
    m_digitsSpin->setRange(6, 8);
    m_digitsSpin->setValue(m_item.digits.toInt() > 0 ? m_item.digits.toInt() : 6);
    connect(m_digitsSpin, &QSpinBox::valueChanged, this, &Manage2FADialog::updatePreview);
    formLayout->addRow("Digits:", m_digitsSpin);

    m_intervalSpin = new QSpinBox(this);
    m_intervalSpin->setRange(5, 300);
    m_intervalSpin->setValue(m_item.interval.toInt() > 0 ? m_item.interval.toInt() : 30);
    m_intervalSpin->setSuffix(" seconds");
    connect(m_intervalSpin, &QSpinBox::valueChanged, this, &Manage2FADialog::updatePreview);
    formLayout->addRow("Interval:", m_intervalSpin);

    m_algorithmCombo = new QComboBox(this);
    m_algorithmCombo->addItems({"SHA1", "SHA256", "SHA512"});
    int algIdx = m_algorithmCombo->findText(m_item.algorithm, Qt::MatchFixedString);
    m_algorithmCombo->setCurrentIndex(algIdx >= 0 ? algIdx : 0);
    connect(m_algorithmCombo, &QComboBox::currentIndexChanged, this, &Manage2FADialog::updatePreview);
    formLayout->addRow("Algorithm:", m_algorithmCombo);

    mainLayout->addWidget(paramGroup);

    // Live preview box
    auto* previewGroup = new QGroupBox("Live Token Preview", this);
    auto* prevLayout = new QVBoxLayout(previewGroup);
    prevLayout->setSpacing(8);

    m_previewCodeLabel = new QLabel("--- ---", this);
    QFont font("Monospace", 22, QFont::Bold);
    m_previewCodeLabel->setFont(font);
    m_previewCodeLabel->setAlignment(Qt::AlignCenter);
    prevLayout->addWidget(m_previewCodeLabel);

    m_validityProgressBar = new QProgressBar(this);
    m_validityProgressBar->setRange(0, 100);
    m_validityProgressBar->setTextVisible(false);
    m_validityProgressBar->setFixedHeight(8);
    prevLayout->addWidget(m_validityProgressBar);

    m_validityTimeLabel = new QLabel(this);
    m_validityTimeLabel->setAlignment(Qt::AlignCenter);
    m_validityTimeLabel->setStyleSheet("color: gray; font-size: 11px;");
    prevLayout->addWidget(m_validityTimeLabel);

    mainLayout->addWidget(previewGroup);

    // Dialog buttons
    auto* btnLayout = new QHBoxLayout();
    auto* removeBtn = new QPushButton("Remove 2FA", this);
    removeBtn->setStyleSheet("color: #d9534f;");
    connect(removeBtn, &QPushButton::clicked, this, &Manage2FADialog::onRemove2FA);

    auto* okBtn = new QPushButton("Save", this);
    okBtn->setDefault(true);
    connect(okBtn, &QPushButton::clicked, this, &QDialog::accept);

    auto* cancelBtn = new QPushButton("Cancel", this);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);

    btnLayout->addWidget(removeBtn);
    btnLayout->addStretch();
    btnLayout->addWidget(cancelBtn);
    btnLayout->addWidget(okBtn);

    mainLayout->addLayout(btnLayout);
}

void Manage2FADialog::onToggleShowSecret() {
    if (m_secretEdit->echoMode() == QLineEdit::Password) {
        m_secretEdit->setEchoMode(QLineEdit::Normal);
        m_toggleSecretBtn->setText("Hide");
    } else {
        m_secretEdit->setEchoMode(QLineEdit::Password);
        m_toggleSecretBtn->setText("Show");
    }
}

void Manage2FADialog::loadFromUriOrSecret(const QString& text) {
    if (text.isEmpty()) return;

    Core::TotpParameters params;
    if (Core::TotpHelper::parseOtpauthUri(text, params)) {
        m_secretEdit->setText(params.secret);
        m_digitsSpin->setValue(params.digits);
        m_intervalSpin->setValue(params.period);
        int idx = m_algorithmCombo->findText(params.algorithm, Qt::MatchFixedString);
        if (idx >= 0) m_algorithmCombo->setCurrentIndex(idx);
    } else {
        m_secretEdit->setText(text.trimmed());
    }
}

void Manage2FADialog::onScanFile() {
    QString file = QFileDialog::getOpenFileName(this, "Select QR Code Image", QString(),
                                                "Image Files (*.png *.jpg *.jpeg *.bmp *.webp *.svg)");
    if (!file.isEmpty()) {
        QString text = Core::QrCodeScanner::scanImageFile(file);
        if (!text.isEmpty()) {
            loadFromUriOrSecret(text);
        } else {
            QMessageBox::warning(this, "Scan Failed", "No valid QR code found in the selected image.");
        }
    }
}

void Manage2FADialog::onScanClipboard() {
    QString text = Core::QrCodeScanner::scanClipboard();
    if (!text.isEmpty()) {
        loadFromUriOrSecret(text);
    } else {
        QMessageBox::warning(this, "Scan Failed", "No QR code image or otpauth URL found on clipboard.");
    }
}

void Manage2FADialog::onScanScreen() {
    hide();
    ScreenCaptureDialog screenDlg(this);
    if (screenDlg.exec() == QDialog::Accepted) {
        QString text = screenDlg.scannedResult();
        if (!text.isEmpty()) {
            loadFromUriOrSecret(text);
        }
    }
    show();
}

void Manage2FADialog::onRemove2FA() {
    m_secretEdit->clear();
    m_digitsSpin->setValue(6);
    m_intervalSpin->setValue(30);
    m_algorithmCombo->setCurrentIndex(0);
    accept();
}

void Manage2FADialog::updatePreview() {
    QString secret = m_secretEdit->text().trimmed();
    int digits = m_digitsSpin->value();
    int interval = m_intervalSpin->value();
    QString alg = m_algorithmCombo->currentText();

    if (secret.isEmpty()) {
        m_previewCodeLabel->setText("--- ---");
        m_validityProgressBar->setValue(0);
        m_validityTimeLabel->setText("No secret entered");
        return;
    }

    QString code = Core::TotpHelper::generateCurrentToken(secret, digits, interval, alg);
    if (code.isEmpty()) {
        m_previewCodeLabel->setText("Invalid Key");
        m_validityProgressBar->setValue(0);
        m_validityTimeLabel->setText("Base32 secret format error");
        return;
    }

    // Format code: e.g. "123 456"
    if (code.length() == 6) {
        m_previewCodeLabel->setText(code.left(3) + " " + code.mid(3));
    } else if (code.length() == 8) {
        m_previewCodeLabel->setText(code.left(4) + " " + code.mid(4));
    } else {
        m_previewCodeLabel->setText(code);
    }

    int remaining = Core::TotpHelper::getRemainingSeconds(interval);
    double progress = Core::TotpHelper::getProgress(interval);
    m_validityProgressBar->setValue(static_cast<int>(progress * 100));

    // Dynamic color: green -> orange when < 5s
    if (remaining <= 5) {
        m_validityProgressBar->setStyleSheet("QProgressBar::chunk { background-color: #d9534f; border-radius: 4px; }");
    } else {
        m_validityProgressBar->setStyleSheet("QProgressBar::chunk { background-color: #28a745; border-radius: 4px; }");
    }

    m_validityTimeLabel->setText(QString("Valid for %1 seconds").arg(remaining));
}

} // namespace EasePass::UI
