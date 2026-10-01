#include "PasswordViewerWidget.h"
#include "Manage2FADialog.h"
#include "PasswordGeneratorDialog.h"
#include "WebsiteIconManager.h"
#include "TotpHelper.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QFrame>
#include <QGuiApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QUrl>
#include <QToolTip>
#include <QPainter>
#include <QFont>

namespace EasePass::UI {

PasswordViewerWidget::PasswordViewerWidget(QWidget* parent)
    : QWidget(parent) {
    setupUi();

    m_totpTimer = new QTimer(this);
    connect(m_totpTimer, &QTimer::timeout, this, &PasswordViewerWidget::updateTotpTimer);

    clear();
}

void PasswordViewerWidget::setupUi() {
    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);

    // Placeholder when no item is selected
    m_placeholderWidget = new QWidget(this);
    auto* phLayout = new QVBoxLayout(m_placeholderWidget);
    auto* phLabel = new QLabel("Select an entry to view details", m_placeholderWidget);
    phLabel->setAlignment(Qt::AlignCenter);
    phLabel->setStyleSheet("color: gray; font-size: 16px;");
    phLayout->addWidget(phLabel);
    rootLayout->addWidget(m_placeholderWidget);

    // Main content widget
    m_contentWidget = new QWidget(this);
    auto* contentLayout = new QVBoxLayout(m_contentWidget);
    contentLayout->setContentsMargins(24, 20, 24, 20);
    contentLayout->setSpacing(16);

    // Header layout
    auto* headerLayout = new QHBoxLayout();
    headerLayout->setSpacing(14);

    m_avatarLabel = new QLabel(this);
    m_avatarLabel->setFixedSize(48, 48);
    headerLayout->addWidget(m_avatarLabel);

    auto* titleCol = new QVBoxLayout();
    m_titleLabel = new QLabel(this);
    m_titleLabel->setFont(QFont("sans-serif", 18, QFont::Bold));
    m_websiteSubtitleLabel = new QLabel(this);
    m_websiteSubtitleLabel->setStyleSheet("color: gray; font-size: 12px;");
    titleCol->addWidget(m_titleLabel);
    titleCol->addWidget(m_websiteSubtitleLabel);
    headerLayout->addLayout(titleCol);

    headerLayout->addStretch();

    // Action buttons in header
    auto* editBtn = new QPushButton("Edit", this);
    connect(editBtn, &QPushButton::clicked, this, &PasswordViewerWidget::onEditClicked);

    auto* deleteBtn = new QPushButton("Delete", this);
    deleteBtn->setStyleSheet("color: #d9534f;");
    connect(deleteBtn, &QPushButton::clicked, this, &PasswordViewerWidget::onDeleteClicked);

    auto* twoFaBtn = new QPushButton("2FA", this);
    connect(twoFaBtn, &QPushButton::clicked, this, &PasswordViewerWidget::onManage2FAClicked);

    auto* genBtn = new QPushButton("Generate", this);
    connect(genBtn, &QPushButton::clicked, this, &PasswordViewerWidget::onOpenGeneratorClicked);

    headerLayout->addWidget(editBtn);
    headerLayout->addWidget(deleteBtn);
    headerLayout->addWidget(twoFaBtn);
    headerLayout->addWidget(genBtn);

    contentLayout->addLayout(headerLayout);

    // Scrollable details area
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    auto* cardWidget = new QWidget(scroll);
    auto* cardLayout = new QVBoxLayout(cardWidget);
    cardLayout->setContentsMargins(0, 8, 0, 0);
    cardLayout->setSpacing(12);

    auto makeFieldRow = [this, cardLayout](const QString& labelText, QLabel*& valLabel, QPushButton* extraBtn, auto copySlot) {
        auto* row = new QFrame(this);
        row->setFrameShape(QFrame::StyledPanel);
        row->setStyleSheet("QFrame { background: palette(alternate-base); border-radius: 6px; padding: 6px; }");
        auto* rLayout = new QHBoxLayout(row);
        rLayout->setContentsMargins(10, 8, 10, 8);

        auto* col = new QVBoxLayout();
        auto* lbl = new QLabel(labelText, row);
        lbl->setStyleSheet("color: gray; font-size: 11px; font-weight: bold;");
        valLabel = new QLabel(row);
        valLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
        valLabel->setFont(QFont("sans-serif", 13));
        col->addWidget(lbl);
        col->addWidget(valLabel);

        rLayout->addLayout(col);
        rLayout->addStretch();

        if (extraBtn) {
            rLayout->addWidget(extraBtn);
        }

        auto* copyBtn = new QPushButton("Copy", row);
        copyBtn->setFixedWidth(64);
        connect(copyBtn, &QPushButton::clicked, this, copySlot);
        rLayout->addWidget(copyBtn);

        cardLayout->addWidget(row);
    };

    // Username row
    makeFieldRow("USERNAME", m_usernameValue, nullptr, &PasswordViewerWidget::onCopyUsername);

    // Email row
    makeFieldRow("EMAIL ADDRESS", m_emailValue, nullptr, &PasswordViewerWidget::onCopyEmail);

    // Password row
    m_togglePasswordBtn = new QPushButton("Show", this);
    m_togglePasswordBtn->setFixedWidth(64);
    connect(m_togglePasswordBtn, &QPushButton::clicked, this, &PasswordViewerWidget::onTogglePassword);
    makeFieldRow("PASSWORD", m_passwordValue, m_togglePasswordBtn, &PasswordViewerWidget::onCopyPassword);

    // 2FA Card
    m_totpCard = new QFrame(this);
    m_totpCard->setFrameShape(QFrame::StyledPanel);
    m_totpCard->setStyleSheet("QFrame { background: palette(alternate-base); border: 1px solid palette(highlight); border-radius: 6px; padding: 8px; }");
    auto* totpCardLayout = new QVBoxLayout(m_totpCard);
    totpCardLayout->setContentsMargins(10, 8, 10, 8);

    auto* totpHeaderLayout = new QHBoxLayout();
    auto* totpTitle = new QLabel("TWO-FACTOR AUTHENTICATION (TOTP)", m_totpCard);
    totpTitle->setStyleSheet("color: palette(highlight); font-size: 11px; font-weight: bold;");
    auto* copyTotpBtn = new QPushButton("Copy Token", m_totpCard);
    copyTotpBtn->setFixedWidth(100);
    connect(copyTotpBtn, &QPushButton::clicked, this, &PasswordViewerWidget::onCopyTotp);

    totpHeaderLayout->addWidget(totpTitle);
    totpHeaderLayout->addStretch();
    totpHeaderLayout->addWidget(copyTotpBtn);
    totpCardLayout->addLayout(totpHeaderLayout);

    m_totpCodeLabel = new QLabel("--- ---", m_totpCard);
    m_totpCodeLabel->setFont(QFont("Monospace", 24, QFont::Bold));
    m_totpCodeLabel->setAlignment(Qt::AlignCenter);
    totpCardLayout->addWidget(m_totpCodeLabel);

    // Progress bar for remaining validity
    m_totpProgressBar = new QProgressBar(m_totpCard);
    m_totpProgressBar->setRange(0, 100);
    m_totpProgressBar->setTextVisible(false);
    m_totpProgressBar->setFixedHeight(6);
    totpCardLayout->addWidget(m_totpProgressBar);

    m_totpTimeLabel = new QLabel(m_totpCard);
    m_totpTimeLabel->setAlignment(Qt::AlignCenter);
    m_totpTimeLabel->setStyleSheet("color: gray; font-size: 11px;");
    totpCardLayout->addWidget(m_totpTimeLabel);

    cardLayout->addWidget(m_totpCard);

    // Website row
    auto* openWebBtn = new QPushButton("Open", this);
    openWebBtn->setFixedWidth(64);
    connect(openWebBtn, &QPushButton::clicked, this, &PasswordViewerWidget::onOpenWebsite);
    makeFieldRow("WEBSITE", m_websiteValue, openWebBtn, &PasswordViewerWidget::onCopyWebsite);

    // Notes row
    makeFieldRow("NOTES", m_notesValue, nullptr, &PasswordViewerWidget::onCopyNotes);

    // Tags section
    auto* tagsFrame = new QFrame(this);
    tagsFrame->setFrameShape(QFrame::StyledPanel);
    tagsFrame->setStyleSheet("QFrame { background: palette(alternate-base); border-radius: 6px; padding: 6px; }");
    auto* tagsFrameLayout = new QVBoxLayout(tagsFrame);
    tagsFrameLayout->setContentsMargins(10, 8, 10, 8);

    auto* tagsHeader = new QLabel("TAGS", tagsFrame);
    tagsHeader->setStyleSheet("color: gray; font-size: 11px; font-weight: bold;");
    tagsFrameLayout->addWidget(tagsHeader);

    m_tagsContainer = new QWidget(tagsFrame);
    auto* tLayout = new QHBoxLayout(m_tagsContainer);
    tLayout->setContentsMargins(0, 4, 0, 0);
    tLayout->setSpacing(6);
    tagsFrameLayout->addWidget(m_tagsContainer);

    cardLayout->addWidget(tagsFrame);
    cardLayout->addStretch();

    scroll->setWidget(cardWidget);
    contentLayout->addWidget(scroll);

    rootLayout->addWidget(m_contentWidget);
}

void PasswordViewerWidget::clear() {
    m_currentIndex = -1;
    m_showPassword = false;
    m_placeholderWidget->show();
    m_contentWidget->hide();
    m_totpTimer->stop();
}

void PasswordViewerWidget::setItem(const Core::PasswordItem& item, int index) {
    m_item = item;
    m_currentIndex = index;
    m_showPassword = false;
    m_togglePasswordBtn->setText("Show");

    m_titleLabel->setText(item.displayName.isEmpty() ? "(Unnamed Entry)" : item.displayName);
    m_websiteSubtitleLabel->setText(item.website);

    // Avatar / Icon
    QPixmap icon = Core::WebsiteIconManager::instance().getIcon(item.website, 44);
    if (!icon.isNull()) {
        m_avatarLabel->setPixmap(icon);
    } else {
        // Draw circular colored avatar
        QPixmap pix(48, 48);
        pix.fill(Qt::transparent);
        QPainter p(&pix);
        p.setRenderHint(QPainter::Antialiasing);
        p.setBrush(item.avatarColor());
        p.setPen(Qt::NoPen);
        p.drawEllipse(2, 2, 44, 44);
        p.setPen(item.avatarTextColor());
        QFont font("sans-serif", 18, QFont::Bold);
        p.setFont(font);
        p.drawText(pix.rect(), Qt::AlignCenter, item.avatarLetter());
        p.end();
        m_avatarLabel->setPixmap(pix);
    }

    m_usernameValue->setText(item.username.isEmpty() ? "—" : item.username);
    m_emailValue->setText(item.email.isEmpty() ? "—" : item.email);
    m_passwordValue->setText(item.password.isEmpty() ? "—" : "••••••••••••");
    m_websiteValue->setText(item.website.isEmpty() ? "—" : item.website);
    m_notesValue->setText(item.notes.isEmpty() ? "—" : item.notes);

    // Tags
    QLayoutItem* child;
    while ((child = m_tagsContainer->layout()->takeAt(0)) != nullptr) {
        delete child->widget();
        delete child;
    }

    if (item.tags.isEmpty()) {
        auto* noneLbl = new QLabel("No tags", m_tagsContainer);
        noneLbl->setStyleSheet("color: gray;");
        m_tagsContainer->layout()->addWidget(noneLbl);
    } else {
        for (const auto& tag : item.tags) {
            auto* tagBadge = new QLabel(tag, m_tagsContainer);
            tagBadge->setStyleSheet("QLabel { background: palette(highlight); color: palette(highlighted-text); border-radius: 4px; padding: 2px 8px; font-size: 11px; }");
            m_tagsContainer->layout()->addWidget(tagBadge);
        }
    }
    static_cast<QHBoxLayout*>(m_tagsContainer->layout())->addStretch();

    // 2FA Card
    if (item.has2FA()) {
        m_totpCard->show();
        updateTotpTimer();
        m_totpTimer->start(200);
    } else {
        m_totpCard->hide();
        m_totpTimer->stop();
    }

    m_placeholderWidget->hide();
    m_contentWidget->show();
}

void PasswordViewerWidget::updateTotpTimer() {
    if (!m_item.has2FA()) return;

    int digits = m_item.digits.toInt() > 0 ? m_item.digits.toInt() : 6;
    int interval = m_item.interval.toInt() > 0 ? m_item.interval.toInt() : 30;

    QString token = Core::TotpHelper::generateCurrentToken(m_item.secret, digits, interval, m_item.algorithm);
    if (token.isEmpty()) {
        m_totpCodeLabel->setText("Invalid Key");
        m_totpProgressBar->setValue(0);
        m_totpTimeLabel->setText("Check 2FA configuration");
        return;
    }

    if (token.length() == 6) {
        m_totpCodeLabel->setText(token.left(3) + " " + token.mid(3));
    } else if (token.length() == 8) {
        m_totpCodeLabel->setText(token.left(4) + " " + token.mid(4));
    } else {
        m_totpCodeLabel->setText(token);
    }

    int remaining = Core::TotpHelper::getRemainingSeconds(interval);
    double prog = Core::TotpHelper::getProgress(interval);
    m_totpProgressBar->setValue(static_cast<int>(prog * 100));

    if (remaining <= 5) {
        m_totpProgressBar->setStyleSheet("QProgressBar::chunk { background-color: #d9534f; border-radius: 3px; }");
    } else {
        m_totpProgressBar->setStyleSheet("QProgressBar::chunk { background-color: #28a745; border-radius: 3px; }");
    }

    m_totpTimeLabel->setText(QString("Expires in %1s").arg(remaining));
}

void PasswordViewerWidget::onTogglePassword() {
    m_showPassword = !m_showPassword;
    if (m_showPassword) {
        m_passwordValue->setText(m_item.password.isEmpty() ? "—" : m_item.password);
        m_togglePasswordBtn->setText("Hide");
    } else {
        m_passwordValue->setText(m_item.password.isEmpty() ? "—" : "••••••••••••");
        m_togglePasswordBtn->setText("Show");
    }
}

void PasswordViewerWidget::copyToClipboard(const QString& text, const QString& label) {
    if (text.isEmpty()) return;
    QClipboard* cb = QGuiApplication::clipboard();
    if (cb) {
        cb->setText(text);
        QToolTip::showText(QCursor::pos(), QString("%1 copied to clipboard!").arg(label), this);
    }
}

void PasswordViewerWidget::onCopyPassword() {
    copyToClipboard(m_item.password, "Password");
}

void PasswordViewerWidget::onCopyUsername() {
    copyToClipboard(m_item.username, "Username");
}

void PasswordViewerWidget::onCopyEmail() {
    copyToClipboard(m_item.email, "Email");
}

void PasswordViewerWidget::onCopyTotp() {
    int digits = m_item.digits.toInt() > 0 ? m_item.digits.toInt() : 6;
    int interval = m_item.interval.toInt() > 0 ? m_item.interval.toInt() : 30;
    QString token = Core::TotpHelper::generateCurrentToken(m_item.secret, digits, interval, m_item.algorithm);
    copyToClipboard(token, "2FA Token");
}

void PasswordViewerWidget::onCopyWebsite() {
    copyToClipboard(m_item.website, "Website URL");
}

void PasswordViewerWidget::onOpenWebsite() {
    QString site = m_item.website.trimmed();
    if (site.isEmpty()) return;
    if (!site.startsWith("http://", Qt::CaseInsensitive) && !site.startsWith("https://", Qt::CaseInsensitive)) {
        site = "https://" + site;
    }
    QDesktopServices::openUrl(QUrl(site));
}

void PasswordViewerWidget::onCopyNotes() {
    copyToClipboard(m_item.notes, "Notes");
}

void PasswordViewerWidget::onEditClicked() {
    if (m_currentIndex >= 0) {
        emit editRequested(m_currentIndex);
    }
}

void PasswordViewerWidget::onDeleteClicked() {
    if (m_currentIndex >= 0) {
        emit deleteRequested(m_currentIndex);
    }
}

void PasswordViewerWidget::onManage2FAClicked() {
    if (m_currentIndex < 0) return;
    Manage2FADialog dlg(m_item, this);
    if (dlg.exec() == QDialog::Accepted) {
        m_item = dlg.getUpdatedItem();
        emit itemModified(m_currentIndex, m_item);
        setItem(m_item, m_currentIndex);
    }
}

void PasswordViewerWidget::onOpenGeneratorClicked() {
    PasswordGeneratorDialog dlg(this);
    dlg.exec();
}

} // namespace EasePass::UI
