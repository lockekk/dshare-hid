/*
 * dshare-hid -- created by dshare.hid@gmail.com
 */

#include "arch/Arch.h"
#include "base/Log.h"

#include <QTemporaryDir>
#include <QTest>

class DisplayLayoutStoreTests : public QObject
{
  Q_OBJECT
public:
  DisplayLayoutStoreTests();

private Q_SLOTS:
  void initTestCase();
  void init();

  void missingFileIsInactive();
  void savesAndLoads();
  void saveKeepsKnownSizes();
  void unknownSizeIsNotStored();
  void corruptFileIsIgnored();
  void truncatedFileIsIgnored();
  void newerVersionIsIgnored();
  void notAnObjectIsIgnored();
  void badPlacementGoesToTheShelf();
  void clientWithoutANameIsDropped();
  void saveFailsWhenTheFolderIsGone();
  void isNextToTheSettings();

  void rememberSizeBeforeALayoutExists();
  void rememberSizeIgnoresNothingUseful();
  void rememberSizeSkipsAnUnchangedSize();
  void rememberShapeFromTheCore();
  void rememberShapeIgnoresNonsense();
  void renameMovesEverything();

  void placementTextRoundTrips();
  void placementTextRejectsNonsense();
  void gridOffsetMeansASide();
  void placementOfUnknownComputer();
  void applyPlacementReportsAChange();
  void onlyAServerOnItsOwnConfigUsesTheLinks();

private:
  Arch m_arch;
  Log m_log;
  QTemporaryDir m_dir;
};
