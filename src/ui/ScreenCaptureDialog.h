#pragma once

#include <QDialog>
#include <QImage>
#include <QPoint>
#include <QPointer>

#include "ScreenQrScanner.h"

class QScreen;

namespace EasePass::UI {

/**
 * Full-screen overlay showing an existing capture (ScreenQrScanner::lastCapture()) of one
 * screen, letting the user drag a region to scan. Feedback is drawn in the overlay instead of
 * modal message boxes (P1). Accepts when exactly one QR code is found.
 */
class ScreenCaptureDialog : public QDialog {
    Q_OBJECT
public:
    ScreenCaptureDialog(Core::ScreenQrScanner *scanner, QScreen *screen, const QString &initialMessage,
                        QWidget *parent = nullptr);

    QString scannedResult() const { return m_result; }

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    void onScanFinished(const Core::ScreenQrScanner::Outcome &outcome);

    QPointer<Core::ScreenQrScanner> m_scanner;
    QRect m_screenLogical; ///< logical geometry of the screen this overlay covers
    QImage m_background;   ///< part of the capture for that screen
    QPoint m_startPoint;
    QRect m_selectedRect;
    bool m_isSelecting = false;
    QString m_message;
    QString m_result;
};

} // namespace EasePass::UI
