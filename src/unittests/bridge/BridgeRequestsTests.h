/*
 * dshare-hid -- created by locke.huang@gmail.com
 */

#include <QTest>

class BridgeRequestsTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void requestRoundTrip();
  void requestRejectsOtherLines();
  void responseRoundTrip();
  void responseRejectsOtherLines();
  void profileRoundTrip();
  void profileRejectsBadHex();
  void profilesReply();
  void textRoundTrip();
  void queueHandsOverOnce();
  void screenSizeFollowsRotation();
};
