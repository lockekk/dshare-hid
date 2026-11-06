/*
 * dshare-hid -- created by dshare.hid@gmail.com
 */

#include <QTest>

class DisplayRectsTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void sortsLeftToRightThenTopToBottom();
  void mirroredDisplaysCountOnce();
  void emptyStaysEmpty();
  void theSystemHasDisplays();
  void unionIsTheCoresBox();
  void waylandLeavesItToThePortal();
};
