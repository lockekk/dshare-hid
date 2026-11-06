/*
 * dshare-hid -- created by dshare.hid@gmail.com
 */

#include <QTest>

class PortalBarrierTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void noZones();
  void oneZoneHasNoFreeEdges();
  void sideBySideHaveNoFreeEdges();
  void topOfAShortDisplay();
  void partOfATallDisplaysSide();
  void everySide();
  void inclusiveCoordinates();
  void onlyActiveSides();
  void gapsAreLeftOut();
  void seamsAreLeftOut();
  void enclosedHoleStaysClosed();
  void aZoneBelowDoesntSplitAnEdge();
  void negativeOrigins();
  void kdeOnlyTakesWholeEdges();
  void fitsGnomesRules();
  void allZonesIsTheWholeDesktop();
};
