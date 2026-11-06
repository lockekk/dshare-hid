/*
 * dshare-hid -- created by dshare.hid@gmail.com
 */

#include "arch/Arch.h"
#include "base/Log.h"

#include <QTest>

class DisplayEdgesTests : public QObject
{
  Q_OBJECT
public:
  DisplayEdgesTests();

private Q_SLOTS:
  void initTestCase();
  void cleanup();

  void displayMovedInsideTheBoxIsSeen();
  void refreshWaitsForItsInterval();
  void refreshIntervalOffLeavesTheListAlone();
  void firstPixelFromEverySide();
  void firstPixelOverAGap();
  void firstPixelPicksTheOutermost();
  void firstPixelEnclosedHoleStaysClosed();
  void firstPixelEmptyList();
  void firstPixelNegativeOrigin();

  void projectsFromEveryFreeEdge_data();
  void projectsFromEveryFreeEdge();
  void projectNeedsALink();
  void projectLeavesTheOuterEdge();
  void projectLeavesTheMiddleOfADisplay();
  void projectUsesTheZone();
  void projectBothEdgesInACorner();
  void projectLeavesAGap();

  void landsOnEveryFreeEdge_data();
  void landsOnEveryFreeEdge();
  void landLeavesTheOuterEdge();
  void landLeavesAGap();
  void landLeavesNoDirection();
  void landKeepsAPositionAlreadyInside();

  void offWithOneDisplay();
  void offWithoutASource();
  void offWhenTheDisplaysDontMatchTheBox();
  void refreshReadsAgain();

private:
  Arch m_arch;
  Log m_log;
};
