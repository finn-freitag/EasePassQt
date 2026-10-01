#include "AboutDialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QFont>
#include <QIcon>

namespace EasePass::UI {

AboutDialog::AboutDialog(QWidget* parent)
    : QDialog(parent) {
    setWindowTitle("About Ease Pass");
    setFixedSize(400, 320);

    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(14);
    layout->setAlignment(Qt::AlignCenter);

    auto* iconLabel = new QLabel(this);
    QIcon icon(":/assets/appicon.svg");
    if (!icon.isNull()) {
        iconLabel->setPixmap(icon.pixmap(72, 72));
    }
    iconLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(iconLabel);

    auto* titleLabel = new QLabel("Ease Pass", this);
    titleLabel->setFont(QFont("sans-serif", 18, QFont::Bold));
    titleLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(titleLabel);

    auto* versionLabel = new QLabel("Version 1.4.0 (Qt 6 Edition for Linux)", this);
    versionLabel->setStyleSheet("color: gray; font-size: 12px;");
    versionLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(versionLabel);

    auto* descLabel = new QLabel("A fast, secure, and privacy-focused password manager.\n"
                                 "Created by Julius Kirsch & Finn Freitag.", this);
    descLabel->setAlignment(Qt::AlignCenter);
    descLabel->setWordWrap(true);
    layout->addWidget(descLabel);

    auto* linkLabel = new QLabel("<a href=\"https://github.com/FrozenAssassine/EasePass\">GitHub Repository</a>", this);
    linkLabel->setOpenExternalLinks(true);
    linkLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(linkLabel);

    auto* okBtn = new QPushButton("Close", this);
    okBtn->setFixedWidth(100);
    connect(okBtn, &QPushButton::clicked, this, &QDialog::accept);

    auto* btnLayout = new QHBoxLayout();
    btnLayout->addStretch();
    btnLayout->addWidget(okBtn);
    btnLayout->addStretch();
    layout->addLayout(btnLayout);
}

} // namespace EasePass::UI
