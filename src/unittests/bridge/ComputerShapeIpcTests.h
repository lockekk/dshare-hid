/*
 * dshare-hid -- created by dshare.hid@gmail.com
 */

#include <QTest>

class ComputerShapeIpcTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void messageFormat();
  void roundTrip();
  void nameMayHoldDotsAndDashes();
  void rejectsNonsense_data();
  void rejectsNonsense();
  void survivesTheIpcSplit();
};
