/*
 * dshare-hid -- created by dshare.hid@gmail.com
 */

#include "DisplayMonitorsTests.h"

#include "gui/layout/DisplayLayout.h"
#include "gui/layout/DisplayMonitors.h"
#include "platform/DisplayRects.h"

#include <QGuiApplication>

using namespace deskflow::gui;
using enum DisplayMonitors::Units;

void DisplayMonitorsTests::cleanup()
{
  DisplayMonitors::clearReported();
}

void DisplayMonitorsTests::logicalUnitsStayAsTheyAre()
{
  QCOMPARE(DisplayMonitors::toCoreUnits({{100, 200}, {1512, 982}, 2}, Logical), QRect(100, 200, 1512, 982));
}

void DisplayMonitorsTests::nativeUnitsScaleTheSize_data()
{
  QTest::addColumn<qreal>("ratio");
  QTest::addColumn<QSize>("logical");
  QTest::addColumn<QSize>("native");
  QTest::newRow("100%") << 1.0 << QSize(1920, 1080) << QSize(1920, 1080);
  QTest::newRow("125%") << 1.25 << QSize(1536, 864) << QSize(1920, 1080);
  QTest::newRow("150%") << 1.5 << QSize(1280, 720) << QSize(1920, 1080);
  QTest::newRow("200%") << 2.0 << QSize(1920, 1080) << QSize(3840, 2160);
}

void DisplayMonitorsTests::nativeUnitsScaleTheSize()
{
  QFETCH(qreal, ratio);
  QFETCH(QSize, logical);
  QFETCH(QSize, native);
  QCOMPARE(DisplayMonitors::toCoreUnits({{0, 0}, logical, ratio}, Native).size(), native);
}

void DisplayMonitorsTests::nativeUnitsKeepThePosition()
{
  QCOMPARE(DisplayMonitors::toCoreUnits({{1920, -200}, {1280, 720}, 1.5}, Native).topLeft(), QPoint(1920, -200));
}

void DisplayMonitorsTests::mixedScalingTouchingDisplays()
{
  // Qt reports the 150% display as smaller, which leaves a gap that native pixels close
  const auto monitors =
      DisplayMonitors::fromScreens({{{0, 0}, {1280, 720}, 1.5}, {{1920, 0}, {1920, 1080}, 1}}, Native);
  QCOMPARE(monitors.at(0).x() + monitors.at(0).width(), monitors.at(1).x());
  QCOMPARE(DisplayLayout::boundingBox(monitors), QRect(0, 0, 3840, 1080));
}

void DisplayMonitorsTests::mirroredDisplaysCountOnce()
{
  const auto monitors = DisplayMonitors::fromScreens({{{0, 0}, {1920, 1080}, 1}, {{0, 0}, {1920, 1080}, 1}}, Logical);
  QCOMPARE(monitors.size(), 1);
}

void DisplayMonitorsTests::sortedLeftToRightThenTopToBottom()
{
  const auto monitors = DisplayMonitors::fromScreens(
      {{{1000, 0}, {100, 100}, 1}, {{0, 500}, {100, 100}, 1}, {{0, 0}, {100, 100}, 1}}, Logical
  );
  QCOMPARE(monitors, QList<QRect>({{0, 0, 100, 100}, {0, 500, 100, 100}, {1000, 0, 100, 100}}));
}

void DisplayMonitorsTests::platformUnits()
{
#if defined(Q_OS_MACOS)
  QCOMPARE(DisplayMonitors::platformUnits(), Logical);
#elif defined(Q_OS_WIN)
  QCOMPARE(DisplayMonitors::platformUnits(), Native);
#else
  qputenv("XDG_SESSION_TYPE", "wayland");
  QCOMPARE(DisplayMonitors::platformUnits(), Logical);
  qputenv("XDG_SESSION_TYPE", "x11");
  QCOMPARE(DisplayMonitors::platformUnits(), Native);
#endif
}

void DisplayMonitorsTests::currentComesFromTheSystem()
{
  const auto system = deskflow::platform::displayRects();
  QCOMPARE(DisplayMonitors::current(), system.isEmpty() ? DisplayMonitors::fromQt() : system);
}

void DisplayMonitorsTests::fromQtMatchesItsScreens()
{
  QCOMPARE(DisplayMonitors::fromQt().size(), QGuiApplication::screens().size());
}

void DisplayMonitorsTests::reportedListWins()
{
  const QList<QRect> monitors = {{0, 0, 10, 10}, {10, 0, 10, 10}};
  DisplayMonitors::setReported(monitors);
  QCOMPARE(DisplayMonitors::current(), monitors);
  DisplayMonitors::clearReported();
  QVERIFY(DisplayMonitors::current() != monitors);
}

void DisplayMonitorsTests::freeEdgesFollowTheSystem()
{
  const auto session = qgetenv("XDG_SESSION_TYPE");
  qputenv("XDG_SESSION_TYPE", "x11");
  QCOMPARE(DisplayMonitors::freeEdges(), !deskflow::platform::displayRects().isEmpty());
  DisplayMonitors::setReported({});
  QVERIFY(DisplayMonitors::freeEdges());
  DisplayMonitors::clearReported();

  // on Wayland the portal's barriers do the crossing, and displays come from Qt
  qputenv("XDG_SESSION_TYPE", "wayland");
  const bool freeEdges = DisplayMonitors::freeEdges();
  qputenv("XDG_SESSION_TYPE", session);
  QVERIFY(freeEdges);
}

void DisplayMonitorsTests::primaryFromQt()
{
  QVERIFY(DisplayMonitors::primary().has_value());
  QVERIFY(DisplayMonitors::fromQt().contains(*DisplayMonitors::primary()));
}

int main(int argc, char **argv)
{
  if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
  }
  QGuiApplication app(argc, argv);
  DisplayMonitorsTests tests;
  return QTest::qExec(&tests, argc, argv);
}
