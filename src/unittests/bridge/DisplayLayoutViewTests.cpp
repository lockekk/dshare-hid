/*
 * dshare-hid -- created by dshare.hid@gmail.com
 */

#include "DisplayLayoutViewTests.h"

#include "common/Settings.h"
#include "gui/config/ServerConfig.h"
#include "gui/core/BridgeClientConfigManager.h"
#include "gui/core/BridgeClientManager.h"
#include "gui/dialogs/ServerConfigDialog.h"
#include "gui/layout/DisplayLayoutStore.h"
#include "gui/layout/DisplayLayoutView.h"
#include "gui/layout/DisplayMonitors.h"
#include "gui/widgets/ComputerSetupView.h"
#include "gui/widgets/RemoveComputerWidget.h"
#include "gui/widgets/SettingsDialogButtonBox.h"

#include <QApplication>
#include <QBoxLayout>
#include <QDrag>
#include <QDropEvent>
#include <QFile>
#include <QFontMetricsF>
#include <QGroupBox>
#include <QHelpEvent>
#include <QLineEdit>
#include <QMenu>
#include <QMimeData>
#include <QPushButton>
#include <QSettings>
#include <QSignalSpy>
#include <QTimer>
#include <QToolTip>

using namespace deskflow::gui;
using enum LayoutSide;

namespace {

const QString kServer = QStringLiteral("srv");
const QList<QRect> kOffset = {{0, 458, 1512, 982}, {1512, 0, 2560, 1440}};

const LayoutClient *find(const DisplayLayoutView *view, const QString &name)
{
  const auto &clients = view->state().clients;
  const auto found = std::ranges::find(clients, name, &LayoutClient::name);
  return found == clients.end() ? nullptr : &*found;
}

struct Fixture
{
  explicit Fixture(const QStringList &names = {"hp"}, bool show = true)
  {
    for (const auto &name : names)
      config.addClient(name);
    dialog = std::make_unique<ServerConfigDialog>(nullptr, config);
    dialog->resize(900, 760);
    view = dialog->findChild<DisplayLayoutView *>();
    grid = dialog->findChild<ComputerSetupView *>();
    if (show) {
      dialog->show();
      QVERIFY(QTest::qWaitForWindowExposed(dialog.get()));
    }
  }

  void place(const QString &name, const LayoutPlacement &placement) const
  {
    auto stored = DisplayLayoutStore::load();
    stored.saved = true;
    stored.active = true;
    stored.clients = view->state().clients;
    std::ranges::find(stored.clients, name, &LayoutClient::name)->placement = placement;
    QVERIFY(DisplayLayoutStore::save(stored));
    view->reload();
  }

  /// the size a client reports when it connects
  void reportSize(const QString &name, const QSize &size) const
  {
    DisplayLayoutStore::rememberSize(name, size);
    view->reload();
  }

  void drag(const QPoint &from, const QPoint &to) const
  {
    QTest::mousePress(view, Qt::LeftButton, {}, from);
    QTest::mouseMove(view, (from + to) / 2);
    QTest::mouseMove(view, to);
    QTest::mouseRelease(view, Qt::LeftButton, {}, to);
  }

  QPushButton *button(const QString &text) const
  {
    const auto buttons = dialog->findChildren<QPushButton *>();
    for (auto *candidate : buttons) {
      if (candidate->text() == text)
        return candidate;
    }
    return nullptr;
  }

  ServerConfig config;
  std::unique_ptr<ServerConfigDialog> dialog;
  DisplayLayoutView *view = nullptr;
  ComputerSetupView *grid = nullptr;
};

/// presses OK or Cancel in the settings dialog that the next call opens
void answerSettings(const QString &newName = {})
{
  QTimer::singleShot(0, qApp, [newName] {
    auto *modal = QApplication::activeModalWidget();
    if (modal == nullptr)
      return;
    if (!newName.isEmpty())
      modal->findChild<QLineEdit *>(QStringLiteral("lineNameEdit"))->setText(newName);
    auto *dialog = qobject_cast<QDialog *>(modal);
    if (newName.isEmpty())
      dialog->reject();
    else
      dialog->accept();
  });
}

} // namespace

DisplayLayoutViewTests::DisplayLayoutViewTests() = default;

void DisplayLayoutViewTests::initTestCase()
{
  m_arch.init();
  QVERIFY(m_dir.isValid());
  qputenv("XDG_CONFIG_HOME", QFile::encodeName(m_dir.path()));
  qputenv("XDG_STATE_HOME", QFile::encodeName(m_dir.path()));
  Settings::setSettingsFile(m_dir.filePath(QStringLiteral("test.conf")));
  Settings::setValue(Settings::Core::ComputerName, kServer);
}

void DisplayLayoutViewTests::init()
{
  QFile::remove(DisplayLayoutStore::filePath());
  Settings::proxy().remove(QStringLiteral("internalConfig"));
  Settings::setValue(Settings::Server::ExternalConfig, false);
  DisplayMonitors::setReported(kOffset);
}

void DisplayLayoutViewTests::cleanup()
{
  DisplayMonitors::clearReported();
}

void DisplayLayoutViewTests::takesTheGridsPlace()
{
  Fixture f;
  QVERIFY(f.view != nullptr);
  QVERIFY(f.view->isVisible());
  QVERIFY(!f.grid->isVisible());
  QVERIFY(f.grid->model() != nullptr);
}

void DisplayLayoutViewTests::showsTheRealDisplaysAndComputers()
{
  Fixture f({"hp", "pc"});
  QCOMPARE(f.view->state().monitors, kOffset);
  QCOMPARE(f.view->state().serverName, kServer);
  QVERIFY(find(f.view, "hp") != nullptr);
  QVERIFY(find(f.view, "pc") != nullptr);
  QCOMPARE(f.view->state().clients.size(), 2);
}

void DisplayLayoutViewTests::openingChangesNothing()
{
  Fixture f;
  QVERIFY(!f.view->modified());
  QVERIFY(!DisplayLayoutView::isModified(f.dialog.get()));
  QVERIFY(!QFile::exists(DisplayLayoutStore::filePath()));
}

void DisplayLayoutViewTests::seedsFromTheGrid()
{
  Fixture f;
  // the grid puts a first client left of the server
  QCOMPARE(find(f.view, "hp")->placement->side, Left);
  QVERIFY(find(f.view, "hp")->placement->wholeEdge);
}

void DisplayLayoutViewTests::dragDocksOnAnEdge()
{
  Fixture f;
  const auto target = f.view->centreOf("hp") + QPoint(f.view->width(), 0);
  QVERIFY(f.view->solved().rects.contains("hp"));
  const auto right = f.view->rect().right() - 4;
  f.drag(f.view->centreOf("hp"), QPoint(right, target.y()));

  QCOMPARE(find(f.view, "hp")->placement->side, Right);
  QVERIFY(!find(f.view, "hp")->placement->wholeEdge);
  QVERIFY(f.view->modified());
}

void DisplayLayoutViewTests::smallMoveIsAClick()
{
  Fixture f;
  const auto before = *find(f.view, "hp");
  const auto from = f.view->centreOf("hp");
  f.drag(from, from + QPoint(1, 1));
  QCOMPARE(*find(f.view, "hp"), before);
  QCOMPARE(f.view->selected(), QStringLiteral("hp"));
}

void DisplayLayoutViewTests::dragWithNoFreeSpotPutsItBack()
{
  Fixture f;
  DisplayMonitors::setReported({});
  f.view->reload();
  // with no displays there is nothing to dock on, so the computer stays on the shelf
  QVERIFY(f.view->solved().rects.isEmpty());
  const auto before = *find(f.view, "hp");
  const auto from = f.view->centreOf("hp");
  f.drag(from, QPoint(f.view->width() / 2, 40));
  QCOMPARE(find(f.view, "hp")->placement, before.placement);
}

void DisplayLayoutViewTests::dragToTheShelfUnlinks()
{
  Fixture f;
  f.drag(f.view->centreOf("hp"), f.view->shelfArea().center());
  QVERIFY(!find(f.view, "hp")->placement.has_value());
  QVERIFY(f.view->shelfArea().contains(f.view->centreOf("hp")));
}

void DisplayLayoutViewTests::dragFromTheShelfDocks()
{
  Fixture f;
  f.view->unlinkComputer("hp");
  const auto from = f.view->centreOf("hp");
  f.drag(from, QPoint(f.view->rect().right() - 4, f.view->height() / 3));
  QVERIFY(find(f.view, "hp")->placement.has_value());
  QVERIFY(f.view->solved().rects.contains("hp"));
}

void DisplayLayoutViewTests::movingTakesWhatHangsOffIt()
{
  Fixture f({"hp", "pc"});
  f.place("pc", {"hp", Left, 0, false});
  QCOMPARE(find(f.view, "pc")->placement->anchor, QStringLiteral("hp"));

  f.drag(f.view->centreOf("hp"), QPoint(f.view->rect().right() - 4, f.view->height() / 3));
  QCOMPARE(find(f.view, "pc")->placement->anchor, QStringLiteral("hp"));
  QVERIFY(f.view->solved().rects.value("pc").x() != f.view->solved().rects.value("hp").x());
}

void DisplayLayoutViewTests::clickingEmptySpaceDeselects()
{
  Fixture f;
  f.view->select("hp");
  QTest::mouseClick(f.view, Qt::LeftButton, {}, QPoint(2, 2));
  QVERIFY(f.view->selected().isEmpty());
}

void DisplayLayoutViewTests::saveWritesTheLayout()
{
  Fixture f;
  f.view->unlinkComputer("hp");
  f.dialog->accept();

  const auto stored = DisplayLayoutStore::load();
  QVERIFY(stored.saved);
  QVERIFY(stored.active);
  QCOMPARE(stored.clients.size(), 1);
  QVERIFY(!stored.clients.first().placement.has_value());
  QVERIFY(!f.view->modified());
}

void DisplayLayoutViewTests::saveEnablesOnALayoutOnlyChange()
{
  Fixture f;
  auto *save = f.button(QStringLiteral("Save"));
  QVERIFY(save != nullptr);
  QVERIFY(!save->isEnabled());

  f.view->unlinkComputer("hp");
  QVERIFY(DisplayLayoutView::isModified(f.dialog.get()));
  QVERIFY(save->isEnabled());
}

void DisplayLayoutViewTests::cancelWritesNothing()
{
  Fixture f;
  f.view->unlinkComputer("hp");
  f.dialog->reject();
  QVERIFY(!QFile::exists(DisplayLayoutStore::filePath()));
}

void DisplayLayoutViewTests::resetGoesBackToWhatsSaved()
{
  Fixture f;
  f.place("hp", {{}, Right, 100, false});
  f.view->unlinkComputer("hp");
  QVERIFY(f.view->modified());

  Q_EMIT f.dialog->findChild<SettingsDialogButtonBox *>()->reset();
  QCOMPARE(find(f.view, "hp")->placement->side, Right);
  QVERIFY(!f.view->modified());
}

void DisplayLayoutViewTests::classicGridSwap()
{
  Fixture f;
  auto *classic = f.button(QStringLiteral("Use the classic grid"));
  QVERIFY(classic != nullptr);
  classic->click();

  QVERIFY(f.view->isClassic());
  QVERIFY(!f.view->isVisible());
  QVERIFY(f.grid->isVisible());
  QVERIFY(f.view->modified());
}

void DisplayLayoutViewTests::classicGridSurvivesSave()
{
  {
    Fixture f;
    f.view->setClassic(true);
    f.dialog->accept();
  }
  QVERIFY(!DisplayLayoutStore::isActive());
  QVERIFY(DisplayLayoutStore::load().saved);

  Fixture again({});
  QVERIFY(again.view->isClassic());
  QVERIFY(again.grid->isVisible());
}

void DisplayLayoutViewTests::backToTheLayout()
{
  Fixture f;
  f.view->setClassic(true);
  auto *back = f.button(QStringLiteral("Use the to-scale layout"));
  QVERIFY(back != nullptr);
  QVERIFY(back->isVisible());
  back->click();
  QVERIFY(!f.view->isClassic());
  QVERIFY(f.view->isVisible());

  // the same button in the same place, the other way round
  QVERIFY(back->isVisible());
  QCOMPARE(back->text(), QStringLiteral("Use the classic grid"));
}

namespace {

QMimeData *newComputerData(const QString &name)
{
  Computer computer(name);
  QByteArray bytes;
  QDataStream stream(&bytes, QIODevice::WriteOnly);
  stream << -1 << -1 << computer;
  auto *data = new QMimeData;
  data->setData(ComputerSetupModel::mimeType(), bytes);
  return data;
}

void sendDrop(QWidget *view, QMimeData *data, const QPoint &point)
{
  QDragEnterEvent enter(point, Qt::CopyAction, data, Qt::LeftButton, Qt::NoModifier);
  QApplication::sendEvent(view, &enter);
  QDragMoveEvent move(point, Qt::CopyAction, data, Qt::LeftButton, Qt::NoModifier);
  QApplication::sendEvent(view, &move);
  QDropEvent drop(point, Qt::CopyAction, data, Qt::LeftButton, Qt::NoModifier);
  QApplication::sendEvent(view, &drop);
}

} // namespace

void DisplayLayoutViewTests::newComputerDrop()
{
  Fixture f;
  std::unique_ptr<QMimeData> data(newComputerData("Unnamed"));
  sendDrop(f.view, data.get(), QPoint(f.view->rect().right() - 4, f.view->shelfArea().top() / 2));

  QVERIFY(find(f.view, "Unnamed") != nullptr);
  QCOMPARE(find(f.view, "Unnamed")->placement->side, Right);
  QCOMPARE(f.view->selected(), QStringLiteral("Unnamed"));
  QVERIFY(std::as_const(f.config).computerExists(QStringLiteral("hp")));
}

void DisplayLayoutViewTests::newComputerDropOnTheShelf()
{
  Fixture f;
  std::unique_ptr<QMimeData> data(newComputerData("Unnamed"));
  sendDrop(f.view, data.get(), f.view->shelfArea().center());
  QVERIFY(find(f.view, "Unnamed") != nullptr);
  QVERIFY(!find(f.view, "Unnamed")->placement.has_value());
}

void DisplayLayoutViewTests::newComputerNamesStayUnique()
{
  Fixture f;
  for (int i = 0; i < 2; ++i) {
    std::unique_ptr<QMimeData> data(newComputerData("Unnamed"));
    sendDrop(f.view, data.get(), QPoint(f.view->rect().right() - 4, f.view->height() / 3));
  }
  QVERIFY(find(f.view, "Unnamed") != nullptr);
  QVERIFY(find(f.view, "Unnamed2") != nullptr);
}

void DisplayLayoutViewTests::addClientTakesADefaultPlace()
{
  Fixture f;
  QVERIFY(f.dialog->addClient(QStringLiteral("fresh")));
  QCOMPARE(find(f.view, "fresh")->placement->side, Right);
}

void DisplayLayoutViewTests::dropOfSomethingElseIsIgnored()
{
  Fixture f;
  QMimeData data;
  data.setText(QStringLiteral("not a computer"));
  sendDrop(f.view, &data, QPoint(100, 100));
  QCOMPARE(f.view->state().clients.size(), 1);
}

void DisplayLayoutViewTests::removeComputer()
{
  Fixture f;
  QSignalSpy deleted(f.grid, &ComputerSetupView::screenDeleted);
  f.view->select("hp");
  f.view->removeComputer("hp");

  QVERIFY(find(f.view, "hp") == nullptr);
  QVERIFY(f.view->selected().isEmpty());
  QCOMPARE(deleted.size(), 1);
}

void DisplayLayoutViewTests::serverCantBeRemoved()
{
  Fixture f;
  QSignalSpy deleted(f.grid, &ComputerSetupView::screenDeleted);
  f.view->removeComputer(kServer);
  f.view->removeComputer(QStringLiteral("nobody"));
  QCOMPARE(deleted.size(), 0);
}

void DisplayLayoutViewTests::removedBridgeStillPrompts()
{
  // the dialog's bridge cleanup hangs off this signal, which the hidden grid can't send itself
  Fixture f;
  QSignalSpy deleted(f.grid, &ComputerSetupView::screenDeleted);
  f.view->removeComputer("hp");
  QCOMPARE(deleted.first().first().toString(), QStringLiteral("hp"));
}

void DisplayLayoutViewTests::settingsDialogRename()
{
  Fixture f;
  f.view->select("hp");
  answerSettings(QStringLiteral("laptop"));
  f.view->openSettings("hp");

  QVERIFY(find(f.view, "hp") == nullptr);
  QVERIFY(find(f.view, "laptop") != nullptr);
  QCOMPARE(f.view->selected(), QStringLiteral("laptop"));
}

void DisplayLayoutViewTests::settingsDialogRenameOfAnAnchor()
{
  Fixture f({"hp", "pc"});
  f.place("pc", {"hp", Left, 0, false});
  answerSettings(QStringLiteral("laptop"));
  f.view->openSettings("hp");
  QCOMPARE(find(f.view, "pc")->placement->anchor, QStringLiteral("laptop"));
}

void DisplayLayoutViewTests::doubleClickOpensSettings()
{
  Fixture f;
  bool opened = false;
  QTimer::singleShot(0, qApp, [&opened] {
    opened = QApplication::activeModalWidget() != nullptr;
    answerSettings();
  });
  QTest::mouseDClick(f.view, Qt::LeftButton, {}, f.view->centreOf("hp"));
  QVERIFY(opened);
}

void DisplayLayoutViewTests::doubleClickOnADisplayOpensTheServer()
{
  Fixture f;
  QString shown;
  QTimer::singleShot(0, qApp, [&shown] {
    if (auto *modal = QApplication::activeModalWidget())
      shown = modal->findChild<QLineEdit *>(QStringLiteral("lineNameEdit"))->text();
    answerSettings();
  });
  // the monitor is the only thing right of the centre of the map, with hp on the left
  QTest::mouseDClick(f.view, Qt::LeftButton, {}, QPoint(f.view->width() * 3 / 4, f.view->shelfArea().top() / 2));
  QCOMPARE(shown, kServer);
}

void DisplayLayoutViewTests::arrowKeysSlideAlongTheEdge()
{
  Fixture f;
  f.place("hp", {{}, Left, 500, false});
  f.view->select("hp");
  f.view->setFocus();
  QTest::keyClick(f.view, Qt::Key_Down);
  QCOMPARE(find(f.view, "hp")->placement->offset, 520);
  QTest::keyClick(f.view, Qt::Key_Up);
  QCOMPARE(find(f.view, "hp")->placement->offset, 500);

  f.place("hp", {{}, Up, 2000, false});
  f.view->select("hp");
  QTest::keyClick(f.view, Qt::Key_Right);
  QCOMPARE(find(f.view, "hp")->placement->offset, 2020);
  QTest::keyClick(f.view, Qt::Key_Left);
  QCOMPARE(find(f.view, "hp")->placement->offset, 2000);
}

void DisplayLayoutViewTests::shiftMovesFurther()
{
  Fixture f;
  f.place("hp", {{}, Left, 500, false});
  f.view->select("hp");
  QTest::keyClick(f.view, Qt::Key_Down, Qt::ShiftModifier);
  QCOMPARE(find(f.view, "hp")->placement->offset, 600);
}

void DisplayLayoutViewTests::arrowAcrossTheEdgeDoesNothing()
{
  Fixture f;
  f.place("hp", {{}, Left, 500, false});
  f.view->select("hp");
  QTest::keyClick(f.view, Qt::Key_Left);
  QTest::keyClick(f.view, Qt::Key_Right);
  QCOMPARE(find(f.view, "hp")->placement->offset, 500);

  f.place("hp", {{}, Up, 2000, false});
  f.view->select("hp");
  QTest::keyClick(f.view, Qt::Key_Up);
  QTest::keyClick(f.view, Qt::Key_Down);
  QCOMPARE(find(f.view, "hp")->placement->offset, 2000);
}

void DisplayLayoutViewTests::deleteKeyUnlinks()
{
  Fixture f;
  f.view->select("hp");
  QTest::keyClick(f.view, Qt::Key_Delete);
  QVERIFY(!find(f.view, "hp")->placement.has_value());
  QVERIFY(find(f.view, "hp") != nullptr);
}

void DisplayLayoutViewTests::keysWithoutASelectionPassThrough()
{
  Fixture f;
  const auto before = *find(f.view, "hp");
  QTest::keyClick(f.view, Qt::Key_Down);
  QTest::keyClick(f.view, Qt::Key_A);
  f.view->select("hp");
  QTest::keyClick(f.view, Qt::Key_A);
  QCOMPARE(*find(f.view, "hp"), before);
}

void DisplayLayoutViewTests::reportedSizeRedrawsToScale()
{
  Fixture f;
  QVERIFY(!find(f.view, "hp")->sizeKnown);
  f.reportSize("hp", {2560, 1440});
  QCOMPARE(find(f.view, "hp")->size, QSize(2560, 1440));
  QVERIFY(find(f.view, "hp")->sizeKnown);
  QCOMPARE(f.view->solved().rects.value("hp").size(), QSize(2560, 1440));
}

void DisplayLayoutViewTests::onlyTheMapAndTheShelf()
{
  Fixture f;
  f.view->select("hp");
  QVERIFY(f.view->findChildren<QWidget *>().isEmpty());
  QCOMPARE(f.view->shelfArea().bottom(), f.view->rect().bottom());
}

void DisplayLayoutViewTests::classicButtonIsOffTheMap()
{
  Fixture f;
  const auto *classic = f.button(QStringLiteral("Use the classic grid"));
  QVERIFY(classic != nullptr);
  QVERIFY(!f.view->isAncestorOf(classic));
  QVERIFY(classic->geometry().top() > f.view->geometry().bottom());
}

void DisplayLayoutViewTests::labelsAreNeverCut()
{
  const QFont font;
  const QFontMetricsF metrics(font);
  const QStringList names = {
      QStringLiteral("hp"), QStringLiteral("dshare-hid home"), QStringLiteral("Deskflow HID (Bridge-1051DB1ADA60)"),
      QStringLiteral("averyveryveryveryverylongnamewithnospaces")
  };

  for (const auto &name : names) {
    for (int width = 0; width <= 240; width += 3) {
      for (int height = 0; height <= 160; height += 5) {
        const auto lines =
            DisplayLayoutView::labelLines(font, name, QStringLiteral("1080 × 2424"), QSizeF(width, height));
        QVERIFY(lines.size() * metrics.lineSpacing() <= height);
        for (const auto &line : lines)
          QVERIFY(metrics.horizontalAdvance(line) <= width);
      }
    }
  }
}

void DisplayLayoutViewTests::labelsWrapInANarrowRectangle()
{
  const QFont font;
  const QFontMetricsF metrics(font);
  const auto wide = metrics.horizontalAdvance(QStringLiteral("1080 × 2424"));
  const QSizeF narrow(wide * 0.75, metrics.lineSpacing() * 10);

  const auto lines = DisplayLayoutView::labelLines(font, QStringLiteral("hp"), QStringLiteral("1080 × 2424"), narrow);

  // all of it is still there, on more lines
  QCOMPARE(lines.first(), QStringLiteral("hp"));
  QVERIFY(lines.size() > 2);
  QCOMPARE(lines.mid(1).join(QLatin1Char(' ')), QStringLiteral("1080 × 2424"));
}

void DisplayLayoutViewTests::labelsDropTheSizeBeforeTheName()
{
  const QFont font;
  const QFontMetricsF metrics(font);
  const QSizeF oneLine(400, metrics.lineSpacing() * 1.5);
  const auto lines = DisplayLayoutView::labelLines(font, QStringLiteral("hp"), QStringLiteral("1080 × 2424"), oneLine);
  QCOMPARE(lines, QStringList{QStringLiteral("hp")});
}

void DisplayLayoutViewTests::labelsKeepTheEndOfALongName()
{
  const QFont font;
  const QFontMetricsF metrics(font);
  const auto name = QStringLiteral("Deskflow HID (Bridge-1051DB1ADA60)");
  const QSizeF room(metrics.horizontalAdvance(name) / 2, metrics.lineSpacing() * 2.5);

  // two lines can't hold the whole name and its size, so the name is shortened and the size stays
  const auto lines = DisplayLayoutView::labelLines(font, name, QStringLiteral("size unknown"), room);
  QCOMPARE(lines.size(), 2);
  QVERIFY(lines.first().endsWith(QStringLiteral(")")));
  QCOMPARE(lines.last(), QStringLiteral("size unknown"));
}

void DisplayLayoutViewTests::toolTipFromTheModel()
{
  Fixture f;
  const auto send = [&f](const QPoint &point) {
    QHelpEvent event(QEvent::ToolTip, point, f.view->mapToGlobal(point));
    return QApplication::sendEvent(f.view, &event);
  };
  QVERIFY(send(f.view->centreOf("hp")));
  QVERIFY(QToolTip::text().contains(QStringLiteral("hp")));
  QVERIFY(send(QPoint(2, 2)));
  QTRY_VERIFY(!QToolTip::isVisible());
  QVERIFY(send(QPoint(f.view->width() * 3 / 4, f.view->shelfArea().top() / 2)));
  QVERIFY(QToolTip::text().contains(kServer));
}

void DisplayLayoutViewTests::contextMenuActions()
{
  Fixture f;
  QStringList actions;
  QTimer::singleShot(0, qApp, [&actions] {
    auto *menu = qobject_cast<QMenu *>(QApplication::activePopupWidget());
    if (menu == nullptr)
      return;
    for (const auto *action : menu->actions())
      actions << action->text();
    menu->actions().at(1)->trigger();
    menu->close();
  });
  const auto point = f.view->centreOf("hp");
  QContextMenuEvent event(QContextMenuEvent::Mouse, point, f.view->mapToGlobal(point));
  QApplication::sendEvent(f.view, &event);

  QCOMPARE(actions.size(), 3);
  QVERIFY(!find(f.view, "hp")->placement.has_value());

  // nothing under the pointer, nothing to ask
  QContextMenuEvent empty(QContextMenuEvent::Mouse, QPoint(2, 2), f.view->mapToGlobal(QPoint(2, 2)));
  QApplication::sendEvent(f.view, &empty);
}

void DisplayLayoutViewTests::externalConfigDisablesIt()
{
  Fixture f;
  const auto *classic = f.button(QStringLiteral("Use the classic grid"));
  f.dialog->findChild<QGroupBox *>(QStringLiteral("groupExternalConfig"))->setChecked(true);
  QVERIFY(!f.view->isEnabled());
  QVERIFY(!classic->isEnabled());
}

void DisplayLayoutViewTests::paintsEveryState()
{
  Fixture f({"hp", "pc"});
  f.place("hp", {{}, Left, 500, false});
  f.place("pc", {"hp", Up, 0, false});
  QVERIFY(!f.view->grab().isNull());

  f.view->select("hp");
  QVERIFY(!f.view->grab().isNull());

  f.place("pc", {"hp", Down, 0, false});
  QVERIFY(!f.view->grab().isNull());
  f.place("pc", {"hp", Left, 0, false});
  QVERIFY(!f.view->grab().isNull());

  // too small on the map for its size to fit under its name
  f.reportSize("pc", {200, 200});
  QVERIFY(!f.view->grab().isNull());

  // mid-drag, over a free spot and then the shelf
  const auto from = f.view->centreOf("hp");
  QTest::mousePress(f.view, Qt::LeftButton, {}, from);
  QTest::mouseMove(f.view, QPoint(f.view->rect().right() - 4, 40));
  QVERIFY(!f.view->grab().isNull());
  QTest::mouseMove(f.view, f.view->shelfArea().center());
  QVERIFY(!f.view->grab().isNull());
  QTest::mouseRelease(f.view, Qt::LeftButton, {}, f.view->shelfArea().center());
  QVERIFY(!f.view->grab().isNull());

  f.view->select("hp");
  QVERIFY(!f.view->grab().isNull());

  f.view->resize(10, 340);
  QVERIFY(!f.view->grab().isNull());

  DisplayMonitors::setReported({});
  f.view->reload();
  QVERIFY(!f.view->grab().isNull());
}

void DisplayLayoutViewTests::paintsTheFirstDisplayAsMain()
{
  Fixture f;
  DisplayMonitors::setReported({*DisplayMonitors::primary()});
  f.view->reload();
  QVERIFY(!f.view->grab().isNull());
}

namespace {

/// makes a computer a bridge client, as a config file in bridge-clients does
QString addBridgeConfig(const QString &name)
{
  const auto path = QStringLiteral("%1/%2.conf").arg(BridgeClientConfigManager::bridgeClientsDir(), name);
  QSettings config(path, QSettings::IniFormat);
  config.setValue(Settings::Core::ComputerName, name);
  config.setValue(Settings::Bridge::SerialNumber, QStringLiteral("serial-%1").arg(name));
  config.sync();
  return path;
}

} // namespace

void DisplayLayoutViewTests::paintsBridgeComputers()
{
  const auto path = addBridgeConfig("dongle");
  Fixture f({"hp", "dongle"});
  QVERIFY(find(f.view, "dongle")->bridge);
  QVERIFY(!f.view->grab().isNull());
  f.place("dongle", {{}, Right, 0, false});
  QVERIFY(!f.view->grab().isNull());
  QVERIFY(QFile::remove(path));
}

void DisplayLayoutViewTests::renameReachesTheProfilesOnSave()
{
  const auto path = addBridgeConfig("dongle");
  BridgeClientConfigManager::writeProfilePlacement(path, 1, QStringLiteral("right|0|0|hp"));

  Fixture f({"hp"});
  answerSettings(QStringLiteral("laptop"));
  f.view->openSettings("hp");
  QCOMPARE(BridgeClientConfigManager::readProfilePlacement(path, 1), QStringLiteral("right|0|0|hp"));

  f.dialog->accept();
  QCOMPARE(BridgeClientConfigManager::readProfilePlacement(path, 1), QStringLiteral("right|0|0|laptop"));
  QVERIFY(QFile::remove(path));
}

void DisplayLayoutViewTests::cancelledRenameLeavesTheProfiles()
{
  const auto path = addBridgeConfig("dongle");
  BridgeClientConfigManager::writeProfilePlacement(path, 1, QStringLiteral("right|0|0|hp"));

  Fixture f({"hp"});
  answerSettings(QStringLiteral("laptop"));
  f.view->openSettings("hp");
  f.view->reload();
  f.dialog->accept();

  QCOMPARE(BridgeClientConfigManager::readProfilePlacement(path, 1), QStringLiteral("right|0|0|hp"));
  QVERIFY(QFile::remove(path));
}

void DisplayLayoutViewTests::paintsAPluggedInDevice()
{
  const auto path = addBridgeConfig("dongle");
  BridgeClientManager manager;
  manager.addClientConfig(path);
  manager.setClientDevice(path, QStringLiteral("/dev/dongle"), QStringLiteral("serial-dongle"));
  manager.setDeviceAvailable(QStringLiteral("/dev/dongle"), QStringLiteral("serial-dongle"), true);
  QVERIFY(!BridgeClientManager::layoutToolTip("dongle").isEmpty());

  Fixture f({"dongle"});
  f.place("dongle", {{}, Right, 0, false});
  QVERIFY(!f.view->grab().isNull());
  QVERIFY(QFile::remove(path));
}

void DisplayLayoutViewTests::bridgeClientStartsOnTheShelf()
{
  const auto path = addBridgeConfig("dongle");
  Fixture f;
  QVERIFY(f.dialog->addClient(QStringLiteral("dongle")));
  QVERIFY(find(f.view, "dongle") != nullptr);
  QVERIFY(!find(f.view, "dongle")->placement.has_value());
  QVERIFY(QFile::remove(path));
}

void DisplayLayoutViewTests::otherMouseButtonsDoNothing()
{
  Fixture f;
  const auto from = f.view->centreOf("hp");
  QTest::mousePress(f.view, Qt::RightButton, {}, from);
  QTest::mouseMove(f.view, from + QPoint(50, 0));
  QTest::mouseRelease(f.view, Qt::RightButton, {}, from + QPoint(50, 0));
  QVERIFY(f.view->selected().isEmpty());
  QVERIFY(!f.view->modified());

  // a settings dialog here would hang the test
  QTest::mouseDClick(f.view, Qt::RightButton, {}, from);
  QVERIFY(!f.view->modified());
}

void DisplayLayoutViewTests::doubleClickOnNothingDoesNothing()
{
  Fixture f;
  QTest::mouseDClick(f.view, Qt::LeftButton, {}, QPoint(2, 2));
  QVERIFY(f.view->selected().isEmpty());
}

void DisplayLayoutViewTests::settingsDialogRenameOfTheServer()
{
  Fixture f;
  answerSettings(QStringLiteral("newserver"));
  f.view->openSettings(kServer);
  const auto name = f.view->state().serverName;
  Settings::setValue(Settings::Core::ComputerName, kServer);
  QCOMPARE(name, QStringLiteral("newserver"));
  QVERIFY(find(f.view, "hp") != nullptr);
}

void DisplayLayoutViewTests::shelfWrapsIntoRows()
{
  QStringList names;
  for (int i = 0; i < 14; ++i)
    names << QStringLiteral("a-computer-with-a-long-name-%1").arg(i);
  Fixture f(names);
  const auto oneRow = f.view->shelfArea().height();
  for (const auto &name : std::as_const(names))
    f.view->unlinkComputer(name);
  QVERIFY(f.view->shelfArea().height() > oneRow);
  QVERIFY(f.view->centreOf(names.last()).y() > f.view->centreOf(names.first()).y());
  QVERIFY(f.view->shelfArea().contains(f.view->centreOf(names.last())));
}

void DisplayLayoutViewTests::centreOfSomethingThatIsNotThere()
{
  Fixture f({"hp", "pc"});
  f.view->unlinkComputer("hp");
  f.view->unlinkComputer("pc");
  QVERIFY(f.view->centreOf("pc") != f.view->centreOf("hp"));
  QCOMPARE(f.view->centreOf("nobody"), QPoint());
}

void DisplayLayoutViewTests::unknownComputersAreIgnored()
{
  Fixture f;
  f.view->unlinkComputer("nobody");
  f.view->openSettings("nobody");
  f.view->dropOwnComputer("nobody", QPoint(10, 10));
  QVERIFY(!f.view->modified());
}

void DisplayLayoutViewTests::arrowKeysOnEverySide()
{
  Fixture f;
  f.place("hp", {{}, Right, 100, false});
  f.view->select("hp");
  QTest::keyClick(f.view, Qt::Key_Down);
  QCOMPARE(find(f.view, "hp")->placement->offset, 120);

  f.place("hp", {{}, Down, 300, false});
  f.view->select("hp");
  QTest::keyClick(f.view, Qt::Key_Right);
  QCOMPARE(find(f.view, "hp")->placement->offset, 320);
}

void DisplayLayoutViewTests::arrowKeysOnAChainedComputer()
{
  Fixture f({"hp", "pc"});
  f.place("hp", {{}, Right, 0, false});
  f.place("pc", {"hp", Right, 0, false});
  f.view->select("pc");
  QTest::keyClick(f.view, Qt::Key_Down);
  QCOMPARE(find(f.view, "pc")->placement->offset, 20);
}

void DisplayLayoutViewTests::arrowKeysOnAParkedComputer()
{
  Fixture f;
  f.place("hp", {{}, Right, 100, false});
  DisplayMonitors::setReported({});
  f.view->reload();
  f.view->select("hp");
  QTest::keyClick(f.view, Qt::Key_Down);
  QCOMPARE(find(f.view, "hp")->placement->offset, 120);
}

void DisplayLayoutViewTests::arrowKeysOnTheShelfDoNothing()
{
  Fixture f;
  f.view->unlinkComputer("hp");
  f.view->select("hp");
  QTest::keyClick(f.view, Qt::Key_Down);
  QVERIFY(!find(f.view, "hp")->placement.has_value());
}

void DisplayLayoutViewTests::backspaceUnlinks()
{
  Fixture f;
  f.view->select("hp");
  QTest::keyClick(f.view, Qt::Key_Backspace);
  QVERIFY(!find(f.view, "hp")->placement.has_value());
}

void DisplayLayoutViewTests::trashTakesADraggedComputer()
{
  Fixture f;
  QSignalSpy deleted(f.grid, &ComputerSetupView::screenDeleted);
  auto *trash = f.dialog->findChild<RemoveComputerWidget *>();
  QSignalSpy removed(trash, &RemoveComputerWidget::computerRemoved);

  const auto *model = f.grid->model();
  std::unique_ptr<QMimeData> data;
  for (int row = 0; row < model->rowCount() && data == nullptr; ++row) {
    for (int column = 0; column < model->columnCount() && data == nullptr; ++column) {
      const auto index = model->index(row, column);
      if (model->data(index, Qt::DisplayRole).toString() == QStringLiteral("hp"))
        data.reset(model->mimeData({index}));
    }
  }
  QVERIFY(data != nullptr);
  sendDrop(trash, data.get(), trash->rect().center());
  QCOMPARE(removed.size(), 1);

  f.view->finishExternalDrag("hp", Qt::MoveAction);
  QVERIFY(find(f.view, "hp") == nullptr);
  QCOMPARE(deleted.size(), 1);
}

void DisplayLayoutViewTests::trashIsTheOnlyWayADragRemoves()
{
  Fixture f;
  f.view->finishExternalDrag("hp", Qt::IgnoreAction);
  QVERIFY(find(f.view, "hp") != nullptr);

  f.view->dropOwnComputer("hp", QPoint(f.view->rect().right() - 4, f.view->shelfArea().top() / 2));
  f.view->finishExternalDrag("hp", Qt::MoveAction);
  QVERIFY(find(f.view, "hp") != nullptr);
}

void DisplayLayoutViewTests::droppedBackOnTheMap()
{
  Fixture f;
  f.view->dropOwnComputer("hp", QPoint(f.view->rect().right() - 4, f.view->shelfArea().top() / 2));
  QCOMPARE(find(f.view, "hp")->placement->side, Right);
}

void DisplayLayoutViewTests::droppedBackOnTheShelf()
{
  Fixture f;
  f.view->dropOwnComputer("hp", f.view->shelfArea().center());
  QVERIFY(!find(f.view, "hp")->placement.has_value());
}

void DisplayLayoutViewTests::droppedBackWithNoFreeSpot()
{
  Fixture f;
  DisplayMonitors::setReported({});
  f.view->reload();
  const auto before = find(f.view, "hp")->placement;
  f.view->dropOwnComputer("hp", QPoint(f.view->width() / 2, 40));
  QCOMPARE(find(f.view, "hp")->placement, before);
}

void DisplayLayoutViewTests::fullGridTakesNoNewComputer()
{
  QStringList names;
  for (int i = 0; i < 14; ++i)
    names << QStringLiteral("c%1").arg(i);
  Fixture f(names);
  QCOMPARE(f.view->state().clients.size(), 14);
  std::unique_ptr<QMimeData> data(newComputerData("Unnamed"));
  sendDrop(f.view, data.get(), QPoint(f.view->rect().right() - 4, f.view->height() / 3));
  QCOMPARE(f.view->state().clients.size(), 14);
}

void DisplayLayoutViewTests::newComputerWithNowhereToGo()
{
  Fixture f;
  DisplayMonitors::setReported({});
  f.view->reload();
  std::unique_ptr<QMimeData> data(newComputerData("Unnamed"));
  sendDrop(f.view, data.get(), QPoint(f.view->width() / 2, 40));
  QVERIFY(find(f.view, "Unnamed") != nullptr);
  QVERIFY(!find(f.view, "Unnamed")->placement.has_value());
}

void DisplayLayoutViewTests::gridWithoutALayoutKeepsTheGrid()
{
  QDialog dialog;
  ComputerList computers;
  computers.resize(15);
  ComputerSetupModel model(computers, 5, 3);
  auto *grid = new ComputerSetupView(&dialog);
  grid->setModel(&model);
  QVERIFY(DisplayLayoutView::install(&dialog, grid, model) == nullptr);
  QVERIFY(!DisplayLayoutView::isModified(&dialog));
}

void DisplayLayoutViewTests::plainDialogWithoutButtons()
{
  QDialog dialog;
  auto *layout = new QVBoxLayout(&dialog);
  ComputerList computers;
  computers.resize(15);
  computers[7] = Computer(kServer);
  computers[7].markAsServer();
  computers[8] = Computer(QStringLiteral("hp"));
  ComputerSetupModel model(computers, 5, 3);
  auto *grid = new ComputerSetupView(&dialog);
  layout->addWidget(grid);
  grid->setModel(&model);

  auto *view = DisplayLayoutView::install(&dialog, grid, model);
  QVERIFY(view != nullptr);
  view->removeComputer("hp");
  QVERIFY(find(view, "hp") == nullptr);

  // a view on its own has no button to bring the layout back
  DisplayLayoutView bare(&dialog, grid, model);
  bare.reload();
  bare.setClassic(true);
  QVERIFY(bare.isClassic());
}

void DisplayLayoutViewTests::pressWithoutAMoveChangesNothing()
{
  Fixture f;
  const auto from = f.view->centreOf("hp");
  QTest::mousePress(f.view, Qt::LeftButton, {}, from);
  QVERIFY(!f.view->grab().isNull());

  // another button letting go doesn't end the drag
  QTest::mouseRelease(f.view, Qt::RightButton, {}, from);
  QTest::mouseRelease(f.view, Qt::LeftButton, {}, from);
  QVERIFY(!f.view->modified());
}

void DisplayLayoutViewTests::draggingOffTheMapStartsARealDrag()
{
  Fixture f;
  // QDrag runs its own event loop, and the timer ends it from inside
  QTimer::singleShot(50, qApp, [] { QDrag::cancel(); });
  const auto from = f.view->centreOf("hp");
  QTest::mousePress(f.view, Qt::LeftButton, {}, from);
  QTest::mouseMove(f.view, QPoint(f.view->width() / 2, -40));
  QVERIFY(find(f.view, "hp") != nullptr);
  QTest::mouseRelease(f.view, Qt::LeftButton, {}, QPoint(f.view->width() / 2, -40));
}

int main(int argc, char **argv)
{
  if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
  }
  QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
  QApplication app(argc, argv);
  DisplayLayoutViewTests tests;
  return QTest::qExec(&tests, argc, argv);
}
