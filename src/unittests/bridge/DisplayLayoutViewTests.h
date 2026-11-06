/*
 * dshare-hid -- created by dshare.hid@gmail.com
 */

#include "arch/Arch.h"
#include "base/Log.h"

#include <QTemporaryDir>
#include <QTest>

class DisplayLayoutViewTests : public QObject
{
  Q_OBJECT
public:
  DisplayLayoutViewTests();

private Q_SLOTS:
  void initTestCase();
  void init();
  void cleanup();

  void takesTheGridsPlace();
  void showsTheRealDisplaysAndComputers();
  void openingChangesNothing();
  void seedsFromTheGrid();

  void dragDocksOnAnEdge();
  void smallMoveIsAClick();
  void dragWithNoFreeSpotPutsItBack();
  void dragToTheShelfUnlinks();
  void dragFromTheShelfDocks();
  void movingTakesWhatHangsOffIt();
  void clickingEmptySpaceDeselects();

  void saveWritesTheLayout();
  void saveEnablesOnALayoutOnlyChange();
  void cancelWritesNothing();
  void resetGoesBackToWhatsSaved();

  void classicGridSwap();
  void classicGridSurvivesSave();
  void backToTheLayout();

  void newComputerDrop();
  void newComputerDropOnTheShelf();
  void newComputerNamesStayUnique();
  void addClientTakesADefaultPlace();
  void dropOfSomethingElseIsIgnored();

  void removeComputer();
  void serverCantBeRemoved();
  void removedBridgeStillPrompts();

  void settingsDialogRename();
  void settingsDialogRenameOfAnAnchor();
  void doubleClickOpensSettings();
  void doubleClickOnADisplayOpensTheServer();

  void arrowKeysSlideAlongTheEdge();
  void shiftMovesFurther();
  void arrowAcrossTheEdgeDoesNothing();
  void deleteKeyUnlinks();
  void keysWithoutASelectionPassThrough();

  void reportedSizeRedrawsToScale();

  void onlyTheMapAndTheShelf();
  void classicButtonIsOffTheMap();
  void labelsAreNeverCut();
  void labelsWrapInANarrowRectangle();
  void labelsDropTheSizeBeforeTheName();
  void labelsKeepTheEndOfALongName();
  void toolTipFromTheModel();
  void contextMenuActions();
  void externalConfigDisablesIt();
  void paintsEveryState();
  void paintsTheFirstDisplayAsMain();
  void paintsBridgeComputers();
  void paintsAPluggedInDevice();
  void renameReachesTheProfilesOnSave();
  void cancelledRenameLeavesTheProfiles();

  void bridgeClientStartsOnTheShelf();
  void otherMouseButtonsDoNothing();
  void doubleClickOnNothingDoesNothing();
  void settingsDialogRenameOfTheServer();
  void shelfWrapsIntoRows();
  void centreOfSomethingThatIsNotThere();
  void unknownComputersAreIgnored();
  void arrowKeysOnEverySide();
  void arrowKeysOnAChainedComputer();
  void arrowKeysOnAParkedComputer();
  void arrowKeysOnTheShelfDoNothing();
  void backspaceUnlinks();
  void trashTakesADraggedComputer();
  void trashIsTheOnlyWayADragRemoves();
  void droppedBackOnTheMap();
  void droppedBackOnTheShelf();
  void droppedBackWithNoFreeSpot();
  void fullGridTakesNoNewComputer();
  void newComputerWithNowhereToGo();
  void gridWithoutALayoutKeepsTheGrid();
  void plainDialogWithoutButtons();
  void pressWithoutAMoveChangesNothing();

  // last: Qt's drag manager stays busy after a drag that no real mouse ended
  void draggingOffTheMapStartsARealDrag();

private:
  Arch m_arch;
  Log m_log;
  QTemporaryDir m_dir;
};
