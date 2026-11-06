/*
 * dshare-hid -- created by dshare.hid@gmail.com
 */

#include <QTest>

class DisplayLayoutSeedTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void fourNeighboursLinkWholeEdges();
  void chainFollowsTheGrid();
  void isolatedCellGoesToTheShelf();
  void gridLoopPlacesEveryoneOnce();
  void edgeOfTheGridDoesNotWrap();
  void neighbourBridgeKeepsItsLink();
  void noServerOnTheGrid();
  void linksMatchTheGrid();
  void clearsOldPlacements();
  void cellWithoutAClientIsSkipped();
};
