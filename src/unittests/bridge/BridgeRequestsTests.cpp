/*
 * dshare-hid -- created by locke.huang@gmail.com
 */

#include "BridgeRequestsTests.h"

#include "platform/bridge/BridgeRequests.h"

#include <cstring>

using namespace deskflow::bridge;

namespace {

DeviceProfile makeProfile(const char *hostname, uint16_t width, uint16_t height, uint8_t rotation)
{
  DeviceProfile profile;
  std::memset(&profile, 0, sizeof(profile));
  std::strncpy(profile.hostname, hostname, sizeof(profile.hostname) - 1);
  profile.screenWidth = width;
  profile.screenHeight = height;
  profile.rotation = rotation;
  profile.yScrollScale = 30;
  return profile;
}

} // namespace

void BridgeRequestsTests::requestRoundTrip()
{
  const QString line = requests::formatRequest(7, requests::kSwitch, {QStringLiteral("1")});
  QCOMPARE(line, QStringLiteral("req 7 switch 1"));

  requests::Request request;
  QVERIFY(requests::parseRequest(line, request));
  QCOMPARE(request.id, quint64(7));
  QCOMPARE(request.command, requests::kSwitch);
  QCOMPARE(request.arguments, QStringList{QStringLiteral("1")});

  QVERIFY(requests::parseRequest(QStringLiteral("req 8 profiles\r"), request));
  QCOMPARE(request.command, requests::kProfiles);
  QVERIFY(request.arguments.isEmpty());
}

void BridgeRequestsTests::requestRejectsOtherLines()
{
  requests::Request request;
  QVERIFY(!requests::parseRequest(QStringLiteral("stop"), request));
  QVERIFY(!requests::parseRequest(QStringLiteral("req"), request));
  QVERIFY(!requests::parseRequest(QStringLiteral("req nonsense"), request));
  QVERIFY(!requests::parseRequest(QStringLiteral("req x profiles"), request));
  QVERIFY(!requests::parseRequest(QStringLiteral("reqs 1 profiles"), request));
}

void BridgeRequestsTests::responseRoundTrip()
{
  quint64 id = 0;
  bool ok = false;
  QStringList fields;

  const QString reply = requests::formatResponse(3, true, {QStringLiteral("1"), QStringLiteral("abcd")});
  QCOMPARE(reply, QStringLiteral("@bridge-rsp 3 ok 1 abcd"));
  QVERIFY(requests::parseResponse(reply, id, ok, fields));
  QCOMPARE(id, quint64(3));
  QVERIFY(ok);
  QCOMPARE(fields, (QStringList{QStringLiteral("1"), QStringLiteral("abcd")}));

  // an error is one message, spaces and all, on one line
  const QString error = requests::formatResponse(4, false, {QStringLiteral("Firmware error\ncode 1")});
  QCOMPARE(error, QStringLiteral("@bridge-rsp 4 error Firmware error code 1"));
  QVERIFY(requests::parseResponse(error, id, ok, fields));
  QCOMPARE(id, quint64(4));
  QVERIFY(!ok);
  QCOMPARE(fields, QStringList{QStringLiteral("Firmware error code 1")});

  // a reply with nothing to say, and one behind a log prefix
  QVERIFY(requests::parseResponse(QStringLiteral("@bridge-rsp 5 ok"), id, ok, fields));
  QVERIFY(ok && fields.isEmpty());
  QVERIFY(requests::parseResponse(QStringLiteral("[2026-10-24T10:00:00] INFO: @bridge-rsp 6 ok 0"), id, ok, fields));
  QCOMPARE(id, quint64(6));
}

void BridgeRequestsTests::responseRejectsOtherLines()
{
  quint64 id = 0;
  bool ok = false;
  QStringList fields;
  QVERIFY(!requests::parseResponse(QStringLiteral("BridgeScreen: keep-alive ack uptime=0s"), id, ok, fields));
  QVERIFY(!requests::parseResponse(QStringLiteral("@bridge-rsp x ok"), id, ok, fields));
  QVERIFY(!requests::parseResponse(QStringLiteral("@bridge-rsp 1 maybe"), id, ok, fields));
  QVERIFY(!requests::parseResponse(QStringLiteral("@bridge-rsp 1"), id, ok, fields));
}

void BridgeRequestsTests::profileRoundTrip()
{
  const DeviceProfile profile = makeProfile("ipad air m3", 1640, 2048, 1);
  const QString field = requests::encodeProfile(profile);
  QCOMPARE(field.size(), qsizetype(2 * sizeof(DeviceProfile)));
  QVERIFY(!field.contains(QLatin1Char(' ')));

  DeviceProfile decoded;
  QVERIFY(requests::decodeProfile(field, decoded));
  QCOMPARE(std::memcmp(&decoded, &profile, sizeof(profile)), 0);
  QVERIFY(requests::decodeProfile(field.toUpper(), decoded));
}

void BridgeRequestsTests::profileRejectsBadHex()
{
  const QString field = requests::encodeProfile(makeProfile("pixel", 1080, 2424, 0));
  DeviceProfile decoded;
  QVERIFY(!requests::decodeProfile(field.left(field.size() - 2), decoded)); // a byte short
  QVERIFY(!requests::decodeProfile(field + QStringLiteral("00"), decoded)); // a byte long
  QVERIFY(!requests::decodeProfile(field.left(field.size() - 1), decoded)); // half a byte
  QString notHex = field;
  notHex[0] = QLatin1Char('z');
  QVERIFY(!requests::decodeProfile(notHex, decoded));
  QVERIFY(!requests::decodeProfile(QString(), decoded));
}

void BridgeRequestsTests::profilesReply()
{
  const QStringList fields{
      QStringLiteral("1"), requests::encodeProfile(makeProfile("pixel", 1080, 2424, 0)),
      requests::encodeProfile(makeProfile("ipad", 1640, 2048, 1))
  };
  int active = -1;
  QList<DeviceProfile> profiles;
  QVERIFY(requests::decodeProfiles(fields, active, profiles));
  QCOMPARE(active, 1);
  QCOMPARE(profiles.size(), qsizetype(2));
  QCOMPARE(QString::fromLatin1(profiles.at(1).hostname), QStringLiteral("ipad"));

  QVERIFY(!requests::decodeProfiles({}, active, profiles));
  QVERIFY(!requests::decodeProfiles({QStringLiteral("one")}, active, profiles));
  QVERIFY(!requests::decodeProfiles({QStringLiteral("0"), QStringLiteral("00")}, active, profiles));

  QVERIFY(requests::changesActiveProfile(requests::kSwitch));
  QVERIFY(requests::changesActiveProfile(requests::kSave));
  QVERIFY(requests::changesActiveProfile(requests::kErase));
  QVERIFY(!requests::changesActiveProfile(requests::kProfiles));
  QVERIFY(!requests::changesActiveProfile(requests::kSetName));
}

void BridgeRequestsTests::textRoundTrip()
{
  const QString name = QStringLiteral("dshare-hid home");
  const QString field = requests::encodeText(name);
  QVERIFY(!field.contains(QLatin1Char(' ')));
  QCOMPARE(requests::decodeText(field), name);
  QCOMPARE(requests::decodeText(QStringLiteral("zz")), QString());
}

void BridgeRequestsTests::queueHandsOverOnce()
{
  requests::Queue queue;
  queue.push("req 1 profiles");
  queue.push("req 2 getname");
  const auto lines = queue.takeAll();
  QCOMPARE(lines.size(), size_t(2));
  QCOMPARE(lines.at(1), std::string("req 2 getname"));
  QVERIFY(queue.takeAll().empty());
}

void BridgeRequestsTests::screenSizeFollowsRotation()
{
  int32_t width = 0;
  int32_t height = 0;

  profileScreenSize(makeProfile("pixel", 1080, 2424, 0), width, height);
  QCOMPARE(width, 1080);
  QCOMPARE(height, 2424);

  profileScreenSize(makeProfile("pixel turned", 2424, 1080, 0), width, height);
  QCOMPARE(width, 1080);
  QCOMPARE(height, 2424);

  profileScreenSize(makeProfile("ipad", 1640, 2048, 1), width, height);
  QCOMPARE(width, 2048);
  QCOMPARE(height, 1640);
}

QTEST_MAIN(BridgeRequestsTests)
