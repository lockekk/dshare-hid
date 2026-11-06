/*
 * dshare-hid -- created by dshare.hid@gmail.com
 */

#include <QTest>

/**
 * @brief Fails if a merge from upstream drops or moves a layout hook
 *
 * A hook can vanish in a clean merge, with nothing to warn anyone, so its presence is tested here.
 * Each row matches Scenario U in docs/upgrade_from_upstream_guide.md.
 */
class LayoutHooksTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void markersPerFile_data();
  void markersPerFile();
  void noMarkerOutsideTheList();
  void everyHookedFileIsWatched();
  void hooksSitAtTheirAnchors_data();
  void hooksSitAtTheirAnchors();
  void hooksComeInTheRightOrder_data();
  void hooksComeInTheRightOrder();
};
