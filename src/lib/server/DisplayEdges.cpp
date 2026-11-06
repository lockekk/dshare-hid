/*
 * dshare-hid -- created by dshare.hid@gmail.com
 */

#include "DisplayEdges.h"

#include "base/Log.h"

#include <QElapsedTimer>

#include <algorithm>

namespace deskflow::server {

namespace {

#ifdef Q_OS_WIN
constexpr int kRefreshMs = 1000;
#else
constexpr int kRefreshMs = 0;
#endif

DisplayEdges::Source s_source = nullptr;
QList<QRect> s_displays;
bool s_warned = false;
int s_refreshMs = kRefreshMs;
QElapsedTimer s_sinceRead;

bool usable(const QRect &box)
{
  if (s_displays.size() < 2)
    return false;

  QRect united;
  for (const auto &display : std::as_const(s_displays))
    united = united.isNull() ? display : united.united(display);
  if (united == box)
    return true;

  if (!s_warned) {
    s_warned = true;
    LOG_WARN("displays don't match the desktop, inner edges are off");
  }
  return false;
}

} // namespace

void DisplayEdges::setSource(Source source)
{
  s_source = source;
  refresh();
}

void DisplayEdges::refresh()
{
  s_sinceRead.start();
  auto displays = s_source != nullptr ? s_source() : QList<QRect>();
  if (displays == s_displays)
    return;
  s_displays = std::move(displays);
  s_warned = false;
}

void DisplayEdges::setRefreshInterval(int milliseconds)
{
  s_refreshMs = milliseconds;
}

const QList<QRect> &DisplayEdges::displays()
{
  return s_displays;
}

std::optional<int32_t> DisplayEdges::firstPixel(const QList<QRect> &displays, Direction side, int32_t along)
{
  using enum Direction;
  const bool rows = side == Left || side == Right;
  const bool outwardIsSmaller = side == Left || side == Top;

  std::optional<int32_t> first;
  for (const auto &display : displays) {
    const int32_t start = rows ? display.y() : display.x();
    const int32_t length = rows ? display.height() : display.width();
    if (along < start || along >= start + length)
      continue;

    const int32_t origin = rows ? display.x() : display.y();
    const int32_t size = rows ? display.width() : display.height();
    const int32_t pixel = outwardIsSmaller ? origin : origin + size - 1;
    if (!first.has_value())
      first = pixel;
    else
      first = outwardIsSmaller ? std::min(*first, pixel) : std::max(*first, pixel);
  }
  return first;
}

void DisplayEdges::project(const QRect &box, int32_t &x, int32_t &y, int32_t zone, const Linked &linked)
{
  if (s_refreshMs > 0 && s_sinceRead.hasExpired(s_refreshMs))
    refresh();

  if (!usable(box))
    return;

  using enum Direction;
  const int32_t startX = x;
  const int32_t startY = y;
  const auto onLinkedFreeEdge = [&](Direction side, int32_t along, int32_t position, int32_t outer, int32_t inward) {
    const auto pixel = firstPixel(s_displays, side, along);
    return pixel.has_value() && *pixel != outer && (position - *pixel) * inward < zone && linked(side, startX, startY);
  };

  const int32_t left = box.x();
  const int32_t right = box.x() + box.width() - 1;
  const int32_t top = box.y();
  const int32_t bottom = box.y() + box.height() - 1;
  if (onLinkedFreeEdge(Left, startY, startX, left, 1))
    x = left;
  if (onLinkedFreeEdge(Right, startY, startX, right, -1))
    x = right;
  if (onLinkedFreeEdge(Top, startX, startY, top, 1))
    y = top;
  if (onLinkedFreeEdge(Bottom, startX, startY, bottom, -1))
    y = bottom;
}

void DisplayEdges::land(const QRect &box, Direction side, int32_t zone, int32_t &x, int32_t &y)
{
  if (!usable(box))
    return;

  using enum Direction;
  const auto clearZone = [&](Direction face, int32_t along, int32_t &position, int32_t outer, int32_t inward) {
    const auto pixel = firstPixel(s_displays, face, along);
    if (pixel.has_value() && *pixel != outer && (position - *pixel) * inward < zone)
      position = *pixel + zone * inward;
  };

  // moving down enters through the top of a display, and so on round
  if (side == Bottom)
    clearZone(Top, x, y, box.y(), 1);
  else if (side == Top)
    clearZone(Bottom, x, y, box.y() + box.height() - 1, -1);
  else if (side == Right)
    clearZone(Left, y, x, box.x(), 1);
  else if (side == Left)
    clearZone(Right, y, x, box.x() + box.width() - 1, -1);
}

} // namespace deskflow::server
