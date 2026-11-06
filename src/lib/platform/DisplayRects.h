/*
 * dshare-hid -- created by dshare.hid@gmail.com
 */

#pragma once

#include <QList>
#include <QRect>

namespace deskflow::platform {

/**
 * @brief The server's displays, in the units of the core's own desktop box
 *
 * The GUI and the core both call this, so a layout drawn in one means the same in the other. It
 * uses the calls behind each platform's getShape(), which is why their union is the core's box.
 *
 * @return Left to right then top to bottom, empty where only the core's session can tell (Wayland)
 */
QList<QRect> displayRects();

/// sorted left to right then top to bottom, with mirrored displays counted once
QList<QRect> sortedDisplayRects(const QList<QRect> &rects);

} // namespace deskflow::platform
