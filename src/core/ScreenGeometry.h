#pragma once

#include <QImage>
#include <QList>
#include <QRect>
#include <QSize>

namespace EasePass::Core {

/**
 * Pure geometry helpers for screen capture and region selection (T9, fixes P1).
 * "Logical" = Qt device-independent pixels in the virtual desktop coordinate system
 * (may have negative origins on multi-monitor layouts); "device"/"image" = physical pixels.
 */
namespace ScreenGeometry {

struct ScreenInfo {
    QRect logicalGeometry;
    qreal devicePixelRatio = 1.0;
};

struct ScreenImage {
    ScreenInfo screen;
    QImage image; ///< device pixels of that screen
};

/// Bounding rect of all screens (logical). Empty list -> null rect.
QRect unionGeometry(const QList<ScreenInfo> &screens);

/// Highest devicePixelRatio of all screens (>= 1.0 if the list is empty).
qreal maxDevicePixelRatio(const QList<ScreenInfo> &screens);

/**
 * Scales a logical rect by @p dpr to device pixels. Edges are rounded outwards
 * (floor for left/top, ceil for right/bottom) so the device rect fully covers the selection.
 */
QRect logicalToDevice(const QRect &logical, qreal dpr);

/**
 * Maps a selection given in global logical coordinates to pixel coordinates in an image that
 * shows @p captureLogical (e.g. a whole-desktop screenshot) at @p imageSize. Scale is derived
 * per axis from the image size, so it works for any capture scale (portal or composite).
 * The result is clipped to the image; returns a null rect if there is no overlap.
 */
QRect mapSelectionToImage(const QRect &selectionLogical, const QRect &captureLogical, const QSize &imageSize);

/**
 * Composes per-screen captures into one image covering unionGeometry() at
 * maxDevicePixelRatio() scale. The returned image has devicePixelRatio set to that scale.
 */
QImage composeScreens(const QList<ScreenImage> &screens);

} // namespace ScreenGeometry
} // namespace EasePass::Core
