/*
 * dshare-hid -- created by locke.huang@gmail.com
 */

#include "arch/Arch.h"

#include <QTemporaryDir>
#include <QTest>

class BridgeProfileActionTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void initTestCase();
  void positionInRange();
  void positionRejectsOthers();
  void configReadsHotkey();
  void configRejectsBadPosition();

private:
  // reading a configuration's options resolves the server's address, and reads the settings
  Arch m_arch;
  QTemporaryDir m_settingsDir;
};
