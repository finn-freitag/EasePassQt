#pragma once

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QProgressBar>
#include <QTimer>
#include "PasswordItem.h"

namespace EasePass::UI {

class PasswordViewerWidget : public QWidget {
    Q_OBJECT
public:
    explicit PasswordViewerWidget(QWidget* parent = nullptr);

    void setItem(const Core::PasswordItem& item, int index);
    void clear();

signals:
    void editRequested(int index);
    void deleteRequested(int index);
    void itemModified(int index, const Core::PasswordItem& item);

private slots:
    void onTogglePassword();
    void onCopyPassword();
    void onCopyUsername();
    void onCopyEmail();
    void onCopyTotp();
    void onCopyWebsite();
    void onOpenWebsite();
    void onCopyNotes();
    void onEditClicked();
    void onDeleteClicked();
    void onManage2FAClicked();
    void onOpenGeneratorClicked();
    void updateTotpTimer();

private:
    void setupUi();
    void copyToClipboard(const QString& text, const QString& label);
    void showCopiedTooltip(QWidget* widget);

    Core::PasswordItem m_item;
    int m_currentIndex = -1;
    bool m_showPassword = false;

    QWidget* m_placeholderWidget = nullptr;
    QWidget* m_contentWidget = nullptr;

    QLabel* m_avatarLabel = nullptr;
    QLabel* m_titleLabel = nullptr;
    QLabel* m_websiteSubtitleLabel = nullptr;

    QLabel* m_usernameValue = nullptr;
    QLabel* m_emailValue = nullptr;
    QLabel* m_passwordValue = nullptr;
    QPushButton* m_togglePasswordBtn = nullptr;

    // 2FA section
    QFrame* m_totpCard = nullptr;
    QLabel* m_totpCodeLabel = nullptr;
    QProgressBar* m_totpProgressBar = nullptr;
    QLabel* m_totpTimeLabel = nullptr;
    QTimer* m_totpTimer = nullptr;

    QLabel* m_websiteValue = nullptr;
    QLabel* m_notesValue = nullptr;
    QWidget* m_tagsContainer = nullptr;
};

} // namespace EasePass::UI
