/*
 * dshare-hid -- created by dshare.hid@gmail.com
 */

#include <QTest>

class DisplayLayoutTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  // reach
  void reachSingleDisplay();
  void reachSideBySide();
  void reachOffsetOuterEdgesOnly();
  void reachOffsetFreeEdges();
  void reachStacked();
  void reachThreeDisplays();
  void reachMirrored();
  void reachNegativeOrigin();
  void reachEnclosedHoleStaysClosed();
  void reachGapBetweenDisplays();
  void reachEmptyList();

  // solve
  void solveAllFourSides();
  void solveChainThreeDeep();
  void solveMissingAnchorParks();
  void solveAnchorLoopParks();
  void solveInvalidSizeParks();
  void solveNoRoomParks();
  void solveReturnsWhenRoomComesBack();
  void solveKeepsTheEdgeWhenAResolutionChanges();
  void solveSlidesWhenADisplayGoesAndComesBack();
  void solveVeryWideDesktop();
  void solveFifteenComputers();
  void solveNeverEntersTheBoxWithoutFreeEdges();

  // links
  void linksPartialEdge();
  void linksTwoOnOneSide();
  void linksRangesOnlyTouch();
  void linksOverlapTooShortIsSkipped();
  void linksSmallClientStillLinks();
  void linksChainBetweenClients();
  void linksWholeEdgeAlone();
  void linksWholeEdgeOfAClient();
  void linksWholeEdgeThenASecondClient();
  void linksWholeEdgeIsIgnoredBesideANotch();
  void linksStayOffTheGapInTheBox();
  void wholeEdgeNeedsTheFacingSideToo();
  void wholeEdgeOfAClientTouchingTheServer();

  // free edges
  void freeEdgeNotchTop();
  void freeEdgeNotchCornerLinksBothSides();
  void freeEdgeTooWideForTheNotch();

  // text
  void formatPercentHasNoDecimalPoint();
  void formatPercentIsTheSameUnderACommaLocale();
  void linkTextGolden();
  void linkTextLeavesOutTheFullRange();
  void linkTextLeavesOutParkedComputers();

  // snap
  void snapNearestEdge();
  void snapMagnets();
  void snapSkipsItsOwnSubtree();
  void snapCanDockOnAClient();
  void snapFindsNoFreeSpot();

  // editing
  void subtreeFollowsAnchors();
  void unlinkTakesWhatHangsOffIt();
  void removeClientLeavesTheRestPlaced();
  void renameClientUpdatesAnchors();

  // default placement
  void defaultPlacementFirstFreeSide();
  void defaultPlacementSharesASideWhenAllAreTaken();
  void defaultPlacementBridgeGoesToTheShelf();
  void defaultPlacementInvalidSize();
  void defaultPlacementNoRoom();
  void defaultPlacementSeesOnlyWhatIsOnTheServer();

  // every combination the core must accept
  void randomLayoutsAreAlwaysValid();
};
