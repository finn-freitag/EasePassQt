#pragma once

#include <QDialog>
#include <QPixmap>
#include <QRubberBand>
#include <QPoint>

namespace EasePass::UI {

class ScreenCaptureDialog : public QDialog {
    Q_OBJECT
public:
    explicit ScreenCaptureDialog(QWidget* parent = nullptr);

    QString scannedResult() const { return m_result; }

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    QPixmap m_fullScreenShot;
    QPoint m_startPoint;
    QRect m_selectedRect;
    bool m_isSelecting = false;
    QString m_result;
};

} // namespace EasePass::UI
