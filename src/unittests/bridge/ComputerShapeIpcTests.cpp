/*
 * dshare-hid -- created by dshare.hid@gmail.com
 */

#include "ComputerShapeIpcTests.h"

#include "common/ComputerShapeMessage.h"

using namespace deskflow;

void ComputerShapeIpcTests::messageFormat()
{
  QCOMPARE(computerShapeMessage("hp", 1920, 1080), QStringLiteral("1920,1080,hp"));
}

void ComputerShapeIpcTests::roundTrip()
{
  const auto shape = parseComputerShape(computerShapeMessage("hp", 2560, 1440));
  QVERIFY(shape.has_value());
  QCOMPARE(shape->name, QStringLiteral("hp"));
  QCOMPARE(shape->size, QSize(2560, 1440));
}

void ComputerShapeIpcTests::nameMayHoldDotsAndDashes()
{
  const auto shape = parseComputerShape(computerShapeMessage("Lockes-MBP.local", 1, 1));
  QCOMPARE(shape->name, QStringLiteral("Lockes-MBP.local"));
}

void ComputerShapeIpcTests::rejectsNonsense_data()
{
  QTest::addColumn<QString>("message");
  QTest::newRow("empty") << "";
  QTest::newRow("no commas") << "1920";
  QTest::newRow("one comma") << "1920,1080";
  QTest::newRow("no name") << "1920,1080,";
  QTest::newRow("bad width") << "wide,1080,hp";
  QTest::newRow("bad height") << "1920,tall,hp";
  QTest::newRow("zero width") << "0,1080,hp";
  QTest::newRow("zero height") << "1920,0,hp";
  QTest::newRow("negative") << "-1920,1080,hp";
}

void ComputerShapeIpcTests::rejectsNonsense()
{
  QFETCH(QString, message);
  QVERIFY(!parseComputerShape(message).has_value());
}

void ComputerShapeIpcTests::survivesTheIpcSplit()
{
  // IpcClient splits a message on '=' into a command and its arguments
  const auto message = QStringLiteral("computerShape=%1").arg(computerShapeMessage("hp", 1920, 1080));
  const auto parts = message.split('=');
  QCOMPARE(parts.size(), 2);
  QVERIFY(parseComputerShape(parts.at(1)).has_value());
}

QTEST_GUILESS_MAIN(ComputerShapeIpcTests)
