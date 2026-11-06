/*
 * dshare-hid -- created by dshare.hid@gmail.com
 */

#include <QTest>

class DisplayMonitorsTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void cleanup();

  void logicalUnitsStayAsTheyAre();
  void nativeUnitsScaleTheSize_data();
  void nativeUnitsScaleTheSize();
  void nativeUnitsKeepThePosition();
  void mixedScalingTouchingDisplays();
  void mirroredDisplaysCountOnce();
  void sortedLeftToRightThenTopToBottom();
  void platformUnits();
  void currentComesFromTheSystem();
  void fromQtMatchesItsScreens();
  void reportedListWins();
  void freeEdgesFollowTheSystem();
  void primaryFromQt();
};
