/*
 * dshare-hid -- created by dshare.hid@gmail.com
 */

#include "DisplayLayoutFollowTests.h"

#include "gui/layout/DisplayLayoutFollow.h"

#include <QGuiApplication>
#include <QScreen>

using namespace deskflow::gui;

namespace {

constexpr int kSettle = 20;
constexpr int kPastSettle = 80;

/// a core with links of its own, and a restart that takes on the wanted ones
struct FakeCore
{
  QString wanted = QStringLiteral("links");
  QString applied = QStringLiteral("links");
  int restarts = 0;
  bool catchesUp = true;

  DisplayLayoutFollow::Hooks hooks()
  {
    return {
        [this] { return wanted; }, [this] { return applied; },
        [this] {
          ++restarts;
          if (catchesUp)
            applied = wanted;
        }
    };
  }
};

} // namespace

void DisplayLayoutFollowTests::nothingHappensWithoutAChange()
{
  FakeCore core;
  core.wanted = QStringLiteral("different");
  DisplayLayoutFollow follow(nullptr, core.hooks(), kSettle);
  QTest::qWait(kPastSettle);
  QCOMPARE(core.restarts, 0);
}

void DisplayLayoutFollowTests::appliesWhenTheLinksChange()
{
  FakeCore core;
  DisplayLayoutFollow follow(nullptr, core.hooks(), kSettle);
  core.wanted = QStringLiteral("different");
  follow.schedule();
  QTRY_COMPARE(core.restarts, 1);
  QCOMPARE(core.applied, core.wanted);
}

void DisplayLayoutFollowTests::aCoreThatNeverCatchesUpIsToldOnce()
{
  FakeCore core;
  core.catchesUp = false;
  DisplayLayoutFollow follow(nullptr, core.hooks(), kSettle);
  core.wanted = QStringLiteral("different");
  follow.schedule();
  QTRY_COMPARE(core.restarts, 1);

  // the links still differ, but nothing changed, so this mustn't turn into a loop
  QTest::qWait(kPastSettle);
  QCOMPARE(core.restarts, 1);
}

void DisplayLayoutFollowTests::leavesTheCoreAloneWhenTheLinksAreTheSame()
{
  FakeCore core;
  DisplayLayoutFollow follow(nullptr, core.hooks(), kSettle);
  follow.schedule();
  QTest::qWait(kPastSettle);
  QCOMPARE(core.restarts, 0);
}

void DisplayLayoutFollowTests::burstOfChangesAppliesOnce()
{
  FakeCore core;
  DisplayLayoutFollow follow(nullptr, core.hooks(), kSettle);
  core.wanted = QStringLiteral("different");
  for (int i = 0; i < 5; ++i)
    follow.schedule();
  QTest::qWait(kPastSettle);
  QCOMPARE(core.restarts, 1);
}

void DisplayLayoutFollowTests::waitsForThingsToSettle()
{
  FakeCore core;
  DisplayLayoutFollow follow(nullptr, core.hooks(), 200);
  core.wanted = QStringLiteral("different");
  follow.schedule();
  QTest::qWait(100);
  follow.schedule();
  QTest::qWait(150);
  // the second change started the wait again
  QCOMPARE(core.restarts, 0);
  QTRY_COMPARE(core.restarts, 1);
}

void DisplayLayoutFollowTests::appliedOnceThenQuiet()
{
  FakeCore core;
  DisplayLayoutFollow follow(nullptr, core.hooks(), kSettle);
  core.wanted = QStringLiteral("different");
  follow.schedule();
  QTRY_COMPARE(core.restarts, 1);

  // the restart brings the client back with the same size
  follow.schedule();
  QTest::qWait(kPastSettle);
  QCOMPARE(core.restarts, 1);
}

void DisplayLayoutFollowTests::dockingADisplay()
{
  FakeCore core;
  DisplayLayoutFollow follow(nullptr, core.hooks(), kSettle);
  core.wanted = QStringLiteral("docked");
  Q_EMIT qGuiApp->screenAdded(QGuiApplication::primaryScreen());
  QTRY_COMPARE(core.restarts, 1);
}

void DisplayLayoutFollowTests::undockingADisplay()
{
  FakeCore core;
  DisplayLayoutFollow follow(nullptr, core.hooks(), kSettle);
  core.wanted = QStringLiteral("undocked");
  Q_EMIT qGuiApp->screenRemoved(QGuiApplication::primaryScreen());
  QTRY_COMPARE(core.restarts, 1);
}

void DisplayLayoutFollowTests::aDisplayChangingSize()
{
  FakeCore core;
  DisplayLayoutFollow follow(nullptr, core.hooks(), kSettle);
  core.wanted = QStringLiteral("resized");
  auto *screen = QGuiApplication::primaryScreen();
  Q_EMIT screen->geometryChanged(screen->geometry());
  QTRY_COMPARE(core.restarts, 1);
}

void DisplayLayoutFollowTests::aNewDisplayIsWatchedToo()
{
  FakeCore core;
  DisplayLayoutFollow follow(nullptr, core.hooks(), kSettle);
  auto *screen = QGuiApplication::primaryScreen();
  Q_EMIT qGuiApp->screenAdded(screen);
  QTest::qWait(kPastSettle);

  core.wanted = QStringLiteral("resized");
  Q_EMIT screen->geometryChanged(screen->geometry());
  QTRY_COMPARE(core.restarts, 1);
}

void DisplayLayoutFollowTests::defaultDelay()
{
  FakeCore core;
  DisplayLayoutFollow follow(nullptr, core.hooks());
  QCOMPARE(DisplayLayoutFollow::kSettleMs, 2000);
}

int main(int argc, char **argv)
{
  if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
  }
  QGuiApplication app(argc, argv);
  DisplayLayoutFollowTests tests;
  return QTest::qExec(&tests, argc, argv);
}
