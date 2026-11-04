#include "ScreenCaptureDialog.h"

#include "ScreenGeometry.h"

#include <QGuiApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QScreen>

namespace EasePass::UI {

ScreenCaptureDialog::ScreenCaptureDialog(Core::ScreenQrScanner *scanner, QScreen *screen, const QString &initialMessage,
                                         QWidget *parent)
    : QDialog(parent, Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint), m_scanner(scanner),
      m_message(initialMessage) {
    setCursor(Qt::CrossCursor);
    if (!screen)
        screen = QGuiApplication::primaryScreen();
    if (screen) {
        m_screenLogical = screen->geometry();
        setScreen(screen);
        setGeometry(m_screenLogical);
    }
    if (m_scanner) {
        const auto &cap = m_scanner->lastCapture();
        const QRect px = Core::ScreenGeometry::mapSelectionToImage(m_screenLogical, cap.logicalGeometry, cap.image.size());
        if (!px.isEmpty())
            m_background = cap.image.copy(px);
        connect(m_scanner, &Core::ScreenQrScanner::finished, this, &ScreenCaptureDialog::onScanFinished);
    }
}

void ScreenCaptureDialog::paintEvent(QPaintEvent *) {
    QPainter painter(this);
    if (!m_background.isNull())
        painter.drawImage(rect(), m_background);
    else
        painter.fillRect(rect(), Qt::black);

    QPainterPath dim;
    dim.addRect(rect());
    if (m_selectedRect.isValid()) {
        QPainterPath sel;
        sel.addRect(m_selectedRect);
        dim = dim.subtracted(sel);
    }
    painter.fillPath(dim, QColor(0, 0, 0, 110));
    if (m_selectedRect.isValid()) {
        painter.setPen(QPen(palette().highlight().color(), 2));
        painter.drawRect(m_selectedRect);
    }

    const QString help = tr("Drag a rectangle around the QR code. Press Esc to cancel.");
    const QString text = m_message.isEmpty() ? help : m_message + QLatin1Char('\n') + help;
    QFont f = font();
    f.setPointSizeF(f.pointSizeF() * 1.2);
    f.setBold(true);
    painter.setFont(f);
    const QRect box = painter.fontMetrics()
                          .boundingRect(rect().adjusted(40, 0, -40, 0), Qt::AlignHCenter | Qt::TextWordWrap, text)
                          .adjusted(-16, -10, 16, 10)
                          .translated(0, 30);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0, 0, 0, 190));
    painter.drawRoundedRect(box, 8, 8);
    painter.setPen(Qt::white);
    painter.drawText(box, Qt::AlignCenter | Qt::TextWordWrap, text);
}

void ScreenCaptureDialog::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        m_isSelecting = true;
        m_startPoint = event->position().toPoint();
        m_selectedRect = QRect();
        update();
    }
}

void ScreenCaptureDialog::mouseMoveEvent(QMouseEvent *event) {
    if (m_isSelecting) {
        m_selectedRect = QRect(m_startPoint, event->position().toPoint()).normalized();
        update();
    }
}

void ScreenCaptureDialog::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() != Qt::LeftButton || !m_isSelecting)
        return;
    m_isSelecting = false;
    m_selectedRect = QRect(m_startPoint, event->position().toPoint()).normalized();
    update();
    if (m_selectedRect.width() < 10 || m_selectedRect.height() < 10 || !m_scanner || m_scanner->isBusy())
        return;
    m_message = tr("Scanning…");
    // Overlay-local logical coordinates -> global logical coordinates.
    m_scanner->scanRegion(m_selectedRect.translated(m_screenLogical.topLeft()));
}

void ScreenCaptureDialog::onScanFinished(const Core::ScreenQrScanner::Outcome &outcome) {
    using S = Core::ScreenQrScanner::Status;
    switch (outcome.status) {
    case S::Found:
        m_result = outcome.text;
        accept();
        return;
    case S::Multiple:
        m_message = tr("Several QR codes found. Select only one.");
        break;
    case S::NoneFound:
        m_message = tr("No QR code found in the selected area. Try again.");
        break;
    default:
        m_message = outcome.errorString;
        break;
    }
    update();
}

void ScreenCaptureDialog::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Escape)
        reject();
    else
        QDialog::keyPressEvent(event);
}

} // namespace EasePass::UI
