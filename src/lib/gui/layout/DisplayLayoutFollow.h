/*
 * dshare-hid -- created by dshare.hid@gmail.com
 */

#pragma once

#include <QObject>
#include <QTimer>

#include <functional>

class QScreen;

namespace deskflow::gui {

/**
 * @brief Puts the core's links right when the displays or a client's size change
 *
 * A restart drops every client, and on GNOME before 51 it asks for input capture again, so it
 * happens only if the links would really change. Changes come in bursts, as docking a laptop does,
 * so it waits for them to settle first.
 */
class DisplayLayoutFollow : public QObject
{
  Q_OBJECT

public:
  /// how long changes have to stop for
  static constexpr int kSettleMs = 2000;

  /// what the links are now, what the core has, and how to give it the new ones
  struct Hooks
  {
    std::function<QString()> wanted;
    std::function<QString()> applied;
    std::function<void()> apply;
  };

  DisplayLayoutFollow(QObject *parent, Hooks hooks, int settleMs = kSettleMs);

  /// something changed, so check once things settle
  void schedule();

private:
  void watch(QScreen *screen);
  void check();

  Hooks m_hooks;
  QTimer m_timer;
};

} // namespace deskflow::gui
