/*
 * dshare-hid -- created by locke.huang@gmail.com
 */

#include "BridgeProfileActionTests.h"

#include "common/Settings.h"
#include "server/BridgeProfileAction.h"
#include "server/Config.h"

#include <QFile>

#include <sstream>

using namespace deskflow::server;

namespace {

// a configuration with one hotkey, as gui/core/BridgeProfileShortcuts writes it
std::string configWithHotkey(const std::string &action)
{
  return "section: options\n"
         "\tkeystroke(Control+Alt+Shift+3) = " +
         action +
         "\n"
         "end\n";
}

} // namespace

void BridgeProfileActionTests::initTestCase()
{
  // before the settings are first read, which would otherwise open the user's own file
  // (Windows doesn't look at these, so it only gets the file swapped below)
  QVERIFY(m_settingsDir.isValid());
  qputenv("XDG_CONFIG_HOME", QFile::encodeName(m_settingsDir.path()));
  qputenv("XDG_STATE_HOME", QFile::encodeName(m_settingsDir.path()));
  Settings::setSettingsFile(m_settingsDir.filePath(QStringLiteral("test.conf")));
  Settings::setStateFile(m_settingsDir.filePath(QStringLiteral("test.state")));
  m_arch.init();
}

void BridgeProfileActionTests::positionInRange()
{
  int position = 0;
  QVERIFY(BridgeProfileAction::parsePosition("1", position));
  QCOMPARE(position, 1);
  QVERIFY(BridgeProfileAction::parsePosition("6", position));
  QCOMPARE(position, 6);
}

void BridgeProfileActionTests::positionRejectsOthers()
{
  int position = 0;
  QVERIFY(!BridgeProfileAction::parsePosition("", position));
  QVERIFY(!BridgeProfileAction::parsePosition("0", position));
  QVERIFY(!BridgeProfileAction::parsePosition("7", position));
  QVERIFY(!BridgeProfileAction::parsePosition("-1", position));
  QVERIFY(!BridgeProfileAction::parsePosition("2x", position));
  QVERIFY(!BridgeProfileAction::parsePosition(" 2", position));
}

void BridgeProfileActionTests::configReadsHotkey()
{
  Config config(nullptr);
  std::istringstream text(configWithHotkey("switchBridgeProfile(3)"));
  text >> config;

  InputFilter *filter = config.getInputFilter();
  QCOMPARE(filter->getNumRules(), 1u);
  const InputFilter::Rule &rule = filter->getRule(0);
  QCOMPARE(rule.getNumActions(true), 1u);
  QCOMPARE(rule.getAction(true, 0).format(), std::string("switchBridgeProfile(3)"));
  QCOMPARE(rule.getNumActions(false), 0u);
}

void BridgeProfileActionTests::configRejectsBadPosition()
{
  for (const char *action :
       {"switchBridgeProfile(0)", "switchBridgeProfile(7)", "switchBridgeProfile()", "switchBridgeProfile(1,2)"}) {
    Config config(nullptr);
    std::istringstream text(configWithHotkey(action));
    QVERIFY_THROWS_EXCEPTION(ServerConfigReadException, text >> config);
  }
}

QTEST_MAIN(BridgeProfileActionTests)
