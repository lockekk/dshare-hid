/*
 * dshare-hid -- created by dshare.hid@gmail.com
 */

#include <QTest>

class DisplayLayoutFollowTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void nothingHappensWithoutAChange();
  void appliesWhenTheLinksChange();
  void aCoreThatNeverCatchesUpIsToldOnce();
  void leavesTheCoreAloneWhenTheLinksAreTheSame();
  void burstOfChangesAppliesOnce();
  void waitsForThingsToSettle();
  void appliedOnceThenQuiet();
  void dockingADisplay();
  void undockingADisplay();
  void aDisplayChangingSize();
  void aNewDisplayIsWatchedToo();
  void defaultDelay();
};
