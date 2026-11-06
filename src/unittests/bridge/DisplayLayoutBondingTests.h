/*
 * dshare-hid -- created by dshare.hid@gmail.com
 */

#include "arch/Arch.h"
#include "base/Log.h"

#include <QTemporaryDir>
#include <QTest>

class DisplayLayoutBondingTests : public QObject
{
  Q_OBJECT
public:
  DisplayLayoutBondingTests();

private Q_SLOTS:
  void initTestCase();
  void init();

  void exactPlacementPerProfile();
  void profileSwitchMovesTheComputer();
  void sameProfileChangesNothing();
  void profileWithOnlyTheOldOffset();
  void farOffsetMeansNotLinked();
  void exactPlacementBeatsTheOldOffset();
  void placementCanBeNotLinked();
  void oldOffsetStaysForTheClassicGrid();
  void noProfileKeepsNothing();
  void renamingAnAnchorReachesEveryProfile();

private:
  Arch m_arch;
  Log m_log;
  QTemporaryDir m_dir;
  QString m_config;
};
