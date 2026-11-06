/*
 * dshare-hid -- created by dshare.hid@gmail.com
 */

#include "DisplayRectsTests.h"

#include "platform/DisplayRects.h"

#if defined(Q_OS_MACOS)
#include <CoreGraphics/CoreGraphics.h>
#endif

using namespace deskflow::platform;

void DisplayRectsTests::sortsLeftToRightThenTopToBottom()
{
  QCOMPARE(
      sortedDisplayRects({{1000, 0, 100, 100}, {0, 500, 100, 100}, {0, 0, 100, 100}}),
      QList<QRect>({{0, 0, 100, 100}, {0, 500, 100, 100}, {1000, 0, 100, 100}})
  );
}

void DisplayRectsTests::mirroredDisplaysCountOnce()
{
  QCOMPARE(sortedDisplayRects({{0, 0, 100, 100}, {0, 0, 100, 100}}), QList<QRect>({{0, 0, 100, 100}}));
}

void DisplayRectsTests::emptyStaysEmpty()
{
  QVERIFY(sortedDisplayRects({}).isEmpty());
}

void DisplayRectsTests::theSystemHasDisplays()
{
  const auto rects = displayRects();
  if (rects.isEmpty())
    QSKIP("no displays here");
  for (const auto &rect : rects)
    QVERIFY(!rect.isEmpty());
  QCOMPARE(sortedDisplayRects(rects), rects);
}

void DisplayRectsTests::unionIsTheCoresBox()
{
#if defined(Q_OS_MACOS)
  // the sum OSXComputer::updateScreenShape() does
  uint32_t count = 0;
  QCOMPARE(CGGetActiveDisplayList(0, nullptr, &count), kCGErrorSuccess);
  if (count == 0)
    QSKIP("no displays here");
  std::vector<CGDirectDisplayID> displays(count);
  QCOMPARE(CGGetActiveDisplayList(count, displays.data(), &count), kCGErrorSuccess);
  CGRect box = CGRectNull;
  for (const auto display : displays)
    box = CGRectUnion(box, CGDisplayBounds(display));

  QRect rects;
  for (const auto &rect : displayRects())
    rects = rects.isNull() ? rect : rects.united(rect);
  QCOMPARE(
      rects, QRect(
                 static_cast<int>(box.origin.x), static_cast<int>(box.origin.y), static_cast<int>(box.size.width),
                 static_cast<int>(box.size.height)
             )
  );
#else
  QSKIP("checked on macOS, and by hand elsewhere");
#endif
}

void DisplayRectsTests::waylandLeavesItToThePortal()
{
#if defined(Q_OS_LINUX)
  qputenv("XDG_SESSION_TYPE", "wayland");
  QVERIFY(displayRects().isEmpty());
#else
  QSKIP("only Linux has Wayland");
#endif
}

QTEST_GUILESS_MAIN(DisplayRectsTests)
