#pragma once

#include <QDialog>

namespace EasePass::UI {

class AboutDialog : public QDialog {
    Q_OBJECT
public:
    explicit AboutDialog(QWidget* parent = nullptr);
};

} // namespace EasePass::UI
