/*
 * dshare-hid -- created by dshare.hid@gmail.com
 */

#include "DisplayLayoutFollow.h"

#include <QGuiApplication>
#include <QScreen>

namespace deskflow::gui {

DisplayLayoutFollow::DisplayLayoutFollow(QObject *parent, Hooks hooks, int settleMs)
    : QObject(parent),
      m_hooks(std::move(hooks))
{
  m_timer.setSingleShot(true);
  m_timer.setInterval(settleMs);
  connect(&m_timer, &QTimer::timeout, this, &DisplayLayoutFollow::check);

  const auto screens = QGuiApplication::screens();
  for (auto *screen : screens)
    watch(screen);

  auto *app = qGuiApp;
  connect(app, &QGuiApplication::screenAdded, this, [this](QScreen *screen) {
    watch(screen);
    schedule();
  });
  connect(app, &QGuiApplication::screenRemoved, this, &DisplayLayoutFollow::schedule);
}

void DisplayLayoutFollow::watch(QScreen *screen)
{
  connect(screen, &QScreen::geometryChanged, this, &DisplayLayoutFollow::schedule);
}

void DisplayLayoutFollow::schedule()
{
  m_timer.start();
}

void DisplayLayoutFollow::check()
{
  if (m_hooks.wanted() != m_hooks.applied())
    m_hooks.apply();
}

} // namespace deskflow::gui
