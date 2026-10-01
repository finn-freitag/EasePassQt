#include "ScreenCaptureDialog.h"
#include "QrCodeScanner.h"

#include <QGuiApplication>
#include <QScreen>
#include <QPainter>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QMessageBox>

namespace EasePass::UI {

ScreenCaptureDialog::ScreenCaptureDialog(QWidget* parent)
    : QDialog(parent, Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool) {
    setAttribute(Qt::WA_DeleteOnClose, false);
    setAttribute(Qt::WA_TranslucentBackground, false);
    setCursor(Qt::CrossCursor);

    QScreen* screen = QGuiApplication::primaryScreen();
    if (screen) {
        m_fullScreenShot = screen->grabWindow(0);
        setGeometry(screen->geometry());
    }

    // First attempt: try scanning the whole screen automatically!
    if (!m_fullScreenShot.isNull()) {
        QString text = Core::QrCodeScanner::scanImage(m_fullScreenShot.toImage());
        if (!text.isEmpty()) {
            m_result = text;
            // Found immediately, but if not we still allow user selection
        }
    }
}

void ScreenCaptureDialog::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    if (!m_fullScreenShot.isNull()) {
        painter.drawPixmap(0, 0, m_fullScreenShot);
    }

    // Dark semi-transparent overlay
    painter.fillRect(rect(), QColor(0, 0, 0, 100));

    // Clear the selected rectangle to reveal full brightness
    if (!m_selectedRect.isNull() && m_selectedRect.isValid()) {
        painter.setCompositionMode(QPainter::CompositionMode_Clear);
        painter.fillRect(m_selectedRect, Qt::transparent);

        painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
        QPen pen(QColor(0, 120, 215), 2);
        painter.setPen(pen);
        painter.drawRect(m_selectedRect);
    }

    // Help banner at the top
    painter.setPen(Qt::white);
    painter.setFont(QFont("sans-serif", 12, QFont::Bold));
    painter.drawText(rect(), Qt::AlignTop | Qt::AlignHCenter,
                     "\nClick and drag to select QR code region. Press ESC to cancel.");
}

void ScreenCaptureDialog::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        m_isSelecting = true;
        m_startPoint = event->pos();
        m_selectedRect = QRect(m_startPoint, QSize(0, 0));
        update();
    }
}

void ScreenCaptureDialog::mouseMoveEvent(QMouseEvent* event) {
    if (m_isSelecting) {
        m_selectedRect = QRect(m_startPoint, event->pos()).normalized();
        update();
    }
}

void ScreenCaptureDialog::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton && m_isSelecting) {
        m_isSelecting = false;
        m_selectedRect = QRect(m_startPoint, event->pos()).normalized();
        update();

        if (m_selectedRect.width() > 10 && m_selectedRect.height() > 10) {
            QPixmap cropped = m_fullScreenShot.copy(m_selectedRect);
            QString text = Core::QrCodeScanner::scanImage(cropped.toImage());
            if (!text.isEmpty()) {
                m_result = text;
                accept();
                return;
            } else {
                QMessageBox::warning(this, "QR Code Not Found",
                                     "No QR code was detected in the selected area. Please try selecting again.");
            }
        }
    }
}

void ScreenCaptureDialog::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Escape) {
        reject();
    } else {
        QDialog::keyPressEvent(event);
    }
}

} // namespace EasePass::UI
