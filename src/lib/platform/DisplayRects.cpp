/*
 * dshare-hid -- created by dshare.hid@gmail.com
 */

#include "DisplayRects.h"

#include "common/PlatformInfo.h"

#include <algorithm>
#include <tuple>
#include <vector>

#if defined(Q_OS_MACOS)
#include <CoreGraphics/CoreGraphics.h>
#elif defined(Q_OS_WIN)
#include <windows.h>
#elif WINAPI_XWINDOWS
#include "platform/XWindowsConfig.h"

#include <X11/Xlib.h>
#if HAVE_X11_EXTENSIONS_XINERAMA_H
#include <X11/extensions/Xinerama.h>
#endif
#endif

namespace deskflow::platform {

namespace {

#if defined(Q_OS_MACOS)

QList<QRect> nativeRects()
{
  uint32_t count = 0;
  if (CGGetActiveDisplayList(0, nullptr, &count) != kCGErrorSuccess)
    return {};

  std::vector<CGDirectDisplayID> displays(count);
  if (CGGetActiveDisplayList(count, displays.data(), &count) != kCGErrorSuccess)
    return {};

  QList<QRect> rects;
  for (uint32_t i = 0; i < count; ++i) {
    const auto bounds = CGDisplayBounds(displays.at(i));
    rects.append(QRect(
        static_cast<int>(bounds.origin.x), static_cast<int>(bounds.origin.y), static_cast<int>(bounds.size.width),
        static_cast<int>(bounds.size.height)
    ));
  }
  return rects;
}

#elif defined(Q_OS_WIN)

BOOL CALLBACK appendMonitor(HMONITOR, HDC, LPRECT rect, LPARAM data)
{
  reinterpret_cast<QList<QRect> *>(data)->append(
      QRect(rect->left, rect->top, rect->right - rect->left, rect->bottom - rect->top)
  );
  return TRUE;
}

QList<QRect> nativeRects()
{
  QList<QRect> rects;
  EnumDisplayMonitors(nullptr, nullptr, appendMonitor, reinterpret_cast<LPARAM>(&rects));
  return rects;
}

#elif WINAPI_XWINDOWS

QList<QRect> nativeRects()
{
  // only the portal knows a Wayland session's outputs, and XWayland's would mislead
  if (isWayland())
    return {};

  auto *display = XOpenDisplay(nullptr);
  if (display == nullptr)
    return {};

  QList<QRect> rects;
#if HAVE_X11_EXTENSIONS_XINERAMA_H
  int eventBase = 0;
  int errorBase = 0;
  if (XineramaQueryExtension(display, &eventBase, &errorBase) != 0 && XineramaIsActive(display) != 0) {
    int count = 0;
    if (auto *screens = XineramaQueryScreens(display, &count)) {
      for (int i = 0; i < count; ++i)
        rects.append(QRect(screens[i].x_org, screens[i].y_org, screens[i].width, screens[i].height));
      XFree(screens);
    }
  }
#endif
  if (rects.isEmpty()) {
    const auto *screen = DefaultScreenOfDisplay(display);
    rects.append(QRect(0, 0, WidthOfScreen(screen), HeightOfScreen(screen)));
  }
  XCloseDisplay(display);
  return rects;
}

#else

QList<QRect> nativeRects()
{
  return {};
}

#endif

} // namespace

QList<QRect> sortedDisplayRects(const QList<QRect> &rects)
{
  QList<QRect> sorted;
  for (const auto &rect : rects) {
    if (!sorted.contains(rect))
      sorted.append(rect);
  }
  std::ranges::sort(sorted, [](const QRect &first, const QRect &second) {
    return std::tuple(first.x(), first.y()) < std::tuple(second.x(), second.y());
  });
  return sorted;
}

QList<QRect> displayRects()
{
  return sortedDisplayRects(nativeRects());
}

} // namespace deskflow::platform
