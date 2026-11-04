#include "ScreenGeometry.h"

#include <QPainter>
#include <cmath>

namespace EasePass::Core::ScreenGeometry {

QRect unionGeometry(const QList<ScreenInfo> &screens) {
    QRect r;
    for (const ScreenInfo &s : screens)
        r = r.united(s.logicalGeometry);
    return r;
}

qreal maxDevicePixelRatio(const QList<ScreenInfo> &screens) {
    qreal dpr = 1.0;
    for (const ScreenInfo &s : screens)
        dpr = std::max(dpr, s.devicePixelRatio);
    return dpr;
}

QRect logicalToDevice(const QRect &logical, qreal dpr) {
    if (logical.isNull() || dpr <= 0)
        return {};
    const int left = int(std::floor(logical.x() * dpr));
    const int top = int(std::floor(logical.y() * dpr));
    const int right = int(std::ceil((qreal(logical.x()) + logical.width()) * dpr));
    const int bottom = int(std::ceil((qreal(logical.y()) + logical.height()) * dpr));
    return QRect(QPoint(left, top), QPoint(right - 1, bottom - 1));
}

QRect mapSelectionToImage(const QRect &selectionLogical, const QRect &captureLogical, const QSize &imageSize) {
    if (selectionLogical.isEmpty() || captureLogical.isEmpty() || imageSize.isEmpty())
        return {};
    const QRect clipped = selectionLogical.intersected(captureLogical);
    if (clipped.isEmpty())
        return {};
    const qreal sx = qreal(imageSize.width()) / captureLogical.width();
    const qreal sy = qreal(imageSize.height()) / captureLogical.height();
    const qreal x0 = (clipped.x() - captureLogical.x()) * sx;
    const qreal y0 = (clipped.y() - captureLogical.y()) * sy;
    const qreal x1 = (clipped.x() + clipped.width() - captureLogical.x()) * sx;
    const qreal y1 = (clipped.y() + clipped.height() - captureLogical.y()) * sy;
    QRect r(QPoint(int(std::floor(x0)), int(std::floor(y0))), QPoint(int(std::ceil(x1)) - 1, int(std::ceil(y1)) - 1));
    return r.intersected(QRect(QPoint(0, 0), imageSize));
}

QImage composeScreens(const QList<ScreenImage> &screens) {
    QList<ScreenInfo> infos;
    for (const ScreenImage &s : screens)
        infos.append(s.screen);
    const QRect total = unionGeometry(infos);
    if (total.isEmpty())
        return {};
    const qreal scale = maxDevicePixelRatio(infos);
    const QSize size = logicalToDevice(QRect(QPoint(0, 0), total.size()), scale).size();

    QImage out(size, QImage::Format_RGB32);
    out.fill(Qt::black);
    {
        QPainter p(&out);
        p.setRenderHint(QPainter::SmoothPixmapTransform);
        for (const ScreenImage &s : screens) {
            if (s.image.isNull())
                continue;
            const QRect local = s.screen.logicalGeometry.translated(-total.topLeft());
            const QRect target = logicalToDevice(local, scale);
            QImage src = s.image;
            src.setDevicePixelRatio(1.0);
            p.drawImage(target, src);
        }
    }
    out.setDevicePixelRatio(scale);
    return out;
}

} // namespace EasePass::Core::ScreenGeometry
