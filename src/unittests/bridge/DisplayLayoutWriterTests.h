/*
 * dshare-hid -- created by dshare.hid@gmail.com
 */

#include "arch/Arch.h"
#include "base/EventQueue.h"
#include "base/Log.h"

#include <QTemporaryDir>
#include <QTest>

class DisplayLayoutWriterTests : public QObject
{
  Q_OBJECT
public:
  DisplayLayoutWriterTests();

private Q_SLOTS:
  void initTestCase();
  void init();
  void cleanup();

  void inactiveOutputIsUpstreams();
  void inactiveAfterClassicGrid();
  void activeOutputHasNoGridNeighbours();
  void activeOutputIsAccepted();
  void seededLinksMatchTheGridsLinks();
  void partialRangesReachTheCore();
  void noServerOnTheGridWritesNothing();
  void writtenLinksFollowWhatWasWritten();
  void linkTextMatchesWhatWouldBeWritten();
  void saveAsIsNotWhatTheCoreRead();
  void removedComputerIsNotWritten();
  void stalePlacementIsNotWritten();
  void newComputerGetsADefaultPlace();
  void acceptedUnderACommaLocale();
  void decimalPointWouldHaveBeenRejected();
  void randomLayoutsAllParse();

private:
  Arch m_arch;
  Log m_log;
  EventQueue m_events;
  QTemporaryDir m_dir;
};
