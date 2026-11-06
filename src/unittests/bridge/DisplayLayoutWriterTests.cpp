/*
 * dshare-hid -- created by dshare.hid@gmail.com
 */

#include "DisplayLayoutWriterTests.h"

#include "common/Settings.h"
#include "gui/config/ServerConfig.h"
#include "gui/layout/DisplayLayoutStore.h"
#include "gui/layout/DisplayMonitors.h"
#include "server/Config.h"

#include <QApplication>
#include <QFile>
#include <QRandomGenerator>
#include <QTemporaryDir>
#include <QTextStream>

#include <clocale>
#include <sstream>

using namespace deskflow::gui;
using enum LayoutSide;
using deskflow::server::Config;

namespace {

const QString kServer = QStringLiteral("srv");
const QList<QRect> kOffset = {{0, 458, 1512, 982}, {1512, 0, 2560, 1440}};

/// a saved config with these computers beside the server, as the dialog would leave it
QString configText(ServerConfig &config)
{
  QString text;
  QTextStream stream(&text);
  stream << config;
  return text;
}

/// as the core's config is written, which is the only write that counts as applied
QString writeForTheCore(const ServerConfig &config)
{
  QFile file(Settings::defaultValue(Settings::Server::ExternalConfigFile).toString());
  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
    return {};
  config.save(file);
  file.close();
  if (!file.open(QIODevice::ReadOnly))
    return {};
  return QString::fromUtf8(file.readAll());
}

void addComputers(const QStringList &names)
{
  Settings::setValue(Settings::Computer::Name.arg(kServer), kServer);
  for (const auto &name : names)
    Settings::setValue(Settings::Computer::Name.arg(name), name);
  Settings::proxy().sync();
}

/// what the core would make of this text
bool parse(IEventQueue *events, const QString &text, Config &config, std::string *error = nullptr)
{
  std::istringstream stream(text.toStdString());
  try {
    stream >> config;
    return true;
  } catch (const std::exception &exception) {
    if (error != nullptr)
      *error = exception.what();
    return false;
  }
}

void activate(const QList<LayoutClient> &clients)
{
  StoredLayout layout;
  layout.saved = true;
  layout.active = true;
  layout.clients = clients;
  QVERIFY(DisplayLayoutStore::save(layout));
}

LayoutClient client(const QString &name, int width, int height, std::optional<LayoutPlacement> placement)
{
  return {name, QSize(width, height), true, false, placement};
}

} // namespace

DisplayLayoutWriterTests::DisplayLayoutWriterTests() = default;

void DisplayLayoutWriterTests::initTestCase()
{
  m_arch.init();
  QVERIFY(m_dir.isValid());
  qputenv("XDG_CONFIG_HOME", QFile::encodeName(m_dir.path()));
  qputenv("XDG_STATE_HOME", QFile::encodeName(m_dir.path()));
  Settings::setSettingsFile(m_dir.filePath(QStringLiteral("test.conf")));
  Settings::setValue(Settings::Core::ComputerName, kServer);
}

void DisplayLayoutWriterTests::init()
{
  QFile::remove(DisplayLayoutStore::filePath());
  Settings::proxy().remove(QStringLiteral("internalConfig"));
  Settings::removeUnknownComputers({});
  DisplayMonitors::setReported(kOffset);
}

void DisplayLayoutWriterTests::cleanup()
{
  DisplayMonitors::clearReported();
}

void DisplayLayoutWriterTests::inactiveOutputIsUpstreams()
{
  ServerConfig config;
  config.addClient("hp");
  const auto before = configText(config);
  QVERIFY(before.contains(QStringLiteral("left = hp")));
  QVERIFY(before.contains(QStringLiteral("right = srv")));

  DisplayLayoutStore::rememberSize("hp", {1920, 1080});
  QCOMPARE(configText(config), before);
}

void DisplayLayoutWriterTests::inactiveAfterClassicGrid()
{
  ServerConfig config;
  config.addClient("hp");
  const auto before = configText(config);

  StoredLayout layout;
  layout.saved = true;
  layout.active = false;
  layout.clients = {client("hp", 1920, 1080, LayoutPlacement{{}, Right, 0, false})};
  QVERIFY(DisplayLayoutStore::save(layout));
  QCOMPARE(configText(config), before);
}

void DisplayLayoutWriterTests::activeOutputHasNoGridNeighbours()
{
  ServerConfig config;
  config.addClient("hp");
  activate({client("hp", 1920, 1080, LayoutPlacement{{}, Right, 0, false})});

  const auto text = configText(config);
  QVERIFY(!text.contains(QStringLiteral("left = hp")));
  QVERIFY(text.contains(QStringLiteral("right")));
}

void DisplayLayoutWriterTests::activeOutputIsAccepted()
{
  addComputers({"hp", "pc", "book"});
  ServerConfig config;
  config.addClient("hp");
  config.addClient("pc");
  config.addClient("book");
  activate({
      client("hp", 1920, 1080, LayoutPlacement{{}, Left, 360, false}),
      client("pc", 2560, 1440, LayoutPlacement{{}, Right, 0, false}),
      client("book", 1440, 900, LayoutPlacement{{}, Up, 2000, false}),
  });

  Config core(&m_events);
  std::string error;
  QVERIFY2(parse(&m_events, configText(config), core, &error), error.c_str());

  float position = 0;
  QCOMPARE(core.getNeighbor("srv", Direction::Right, 0.5f, &position), std::string("pc"));
  QCOMPARE(core.getNeighbor("srv", Direction::Left, 0.7f, &position), std::string("hp"));
  QCOMPARE(core.getNeighbor("srv", Direction::Left, 0.2f, nullptr), std::string());
}

void DisplayLayoutWriterTests::seededLinksMatchTheGridsLinks()
{
  addComputers({"hp", "pc"});
  ServerConfig config;
  config.addClient("hp");
  config.addClient("pc");

  Config grid(&m_events);
  std::string error;
  QVERIFY2(parse(&m_events, configText(config), grid, &error), error.c_str());

  auto state = DisplayLayoutStore::buildState(std::as_const(config).computers(), kServer, {});
  StoredLayout layout;
  layout.saved = true;
  layout.active = true;
  layout.clients = state.clients;
  QVERIFY(DisplayLayoutStore::save(layout));

  Config seeded(&m_events);
  QVERIFY2(parse(&m_events, configText(config), seeded, &error), error.c_str());
  QVERIFY(grid == seeded);
}

void DisplayLayoutWriterTests::partialRangesReachTheCore()
{
  addComputers({"book"});
  ServerConfig config;
  config.addClient("book");
  activate({client("book", 1440, 900, LayoutPlacement{{}, Up, 2000, false})});

  Config core(&m_events);
  QVERIFY(parse(&m_events, configText(config), core));
  // 2000 to 3440 of a 4072-wide box
  QCOMPARE(core.getNeighbor("srv", Direction::Top, 0.40f, nullptr), std::string());
  QCOMPARE(core.getNeighbor("srv", Direction::Top, 0.60f, nullptr), std::string("book"));
  QCOMPARE(core.getNeighbor("srv", Direction::Top, 0.90f, nullptr), std::string());
}

void DisplayLayoutWriterTests::noServerOnTheGridWritesNothing()
{
  // the server only joins the grid once a client or the dialog puts it there
  ServerConfig config;
  QVERIFY(!config.computerExists(kServer));
  activate({client("hp", 1920, 1080, LayoutPlacement{{}, Right, 0, false})});
  QVERIFY(!configText(config).contains(kServer));
}

void DisplayLayoutWriterTests::writtenLinksFollowWhatWasWritten()
{
  addComputers({"hp"});
  ServerConfig config;
  config.addClient("hp");
  activate({client("hp", 1920, 1080, LayoutPlacement{{}, Right, 0, false})});
  const auto text = writeForTheCore(config);
  QVERIFY(!DisplayLayoutStore::writtenLinks().isEmpty());
  QVERIFY(text.contains(DisplayLayoutStore::writtenLinks()));

  // the display changing moves nothing until the links are written again
  const auto written = DisplayLayoutStore::writtenLinks();
  DisplayMonitors::setReported({{0, 0, 1000, 1000}, {1000, 500, 1000, 1000}});
  QCOMPARE(DisplayLayoutStore::writtenLinks(), written);
  QVERIFY(DisplayLayoutStore::linkText(config) != written);
}

void DisplayLayoutWriterTests::linkTextMatchesWhatWouldBeWritten()
{
  addComputers({"hp"});
  ServerConfig config;
  config.addClient("hp");
  QCOMPARE(DisplayLayoutStore::linkText(config), QString());

  activate({client("hp", 1920, 1080, LayoutPlacement{{}, Right, 0, false})});
  const auto wanted = DisplayLayoutStore::linkText(config);
  QVERIFY(!wanted.isEmpty());
  writeForTheCore(config);
  QCOMPARE(DisplayLayoutStore::writtenLinks(), wanted);
}

void DisplayLayoutWriterTests::saveAsIsNotWhatTheCoreRead()
{
  addComputers({"hp"});
  ServerConfig config;
  config.addClient("hp");
  activate({client("hp", 1920, 1080, LayoutPlacement{{}, Right, 0, false})});
  writeForTheCore(config);
  const auto written = DisplayLayoutStore::writtenLinks();

  DisplayMonitors::setReported({{0, 0, 1000, 1000}, {1000, 500, 1000, 1000}});
  QTemporaryDir dir;
  QVERIFY(config.save(dir.filePath(QStringLiteral("saved-as.conf"))));
  QCOMPARE(DisplayLayoutStore::writtenLinks(), written);
  QVERIFY(DisplayLayoutStore::linkText(config) != written);
}

void DisplayLayoutWriterTests::removedComputerIsNotWritten()
{
  addComputers({"hp"});
  ServerConfig config;
  config.addClient("hp");
  activate({
      client("hp", 1920, 1080, LayoutPlacement{{}, Right, 0, false}),
      client("ghost", 1920, 1080, LayoutPlacement{{}, Left, 0, false}),
  });

  const auto text = configText(config);
  QVERIFY(!text.contains(QStringLiteral("ghost")));
  Config core(&m_events);
  QVERIFY(parse(&m_events, text, core));
}

void DisplayLayoutWriterTests::stalePlacementIsNotWritten()
{
  addComputers({"hp"});
  ServerConfig config;
  config.addClient("hp");
  activate({client("hp", 1920, 1080, LayoutPlacement{"ghost", Right, 0, false})});

  const auto text = configText(config);
  QVERIFY(!text.contains(QStringLiteral("ghost")));
  Config core(&m_events);
  QVERIFY(parse(&m_events, text, core));
}

void DisplayLayoutWriterTests::newComputerGetsADefaultPlace()
{
  addComputers({"hp", "fresh"});
  ServerConfig config;
  config.addClient("hp");
  config.addClient("fresh");
  activate({client("hp", 1920, 1080, LayoutPlacement{{}, Left, 460, false})});

  Config core(&m_events);
  QVERIFY(parse(&m_events, configText(config), core));
  QCOMPARE(core.getNeighbor("srv", Direction::Right, 0.5f, nullptr), std::string("fresh"));
}

void DisplayLayoutWriterTests::acceptedUnderACommaLocale()
{
  addComputers({"hp"});
  ServerConfig config;
  config.addClient("hp");
  activate({client("hp", 1920, 1080, LayoutPlacement{{}, Left, 360, false})});

  const QByteArray saved(std::setlocale(LC_NUMERIC, nullptr));
  if (std::setlocale(LC_NUMERIC, "de_DE.UTF-8") == nullptr)
    QSKIP("no comma locale on this system");

  Config core(&m_events);
  std::string error;
  const bool accepted = parse(&m_events, configText(config), core, &error);
  std::setlocale(LC_NUMERIC, saved.constData());
  QVERIFY2(accepted, error.c_str());
}

void DisplayLayoutWriterTests::decimalPointWouldHaveBeenRejected()
{
  addComputers({"hp"});
  const QByteArray saved(std::setlocale(LC_NUMERIC, nullptr));
  if (std::setlocale(LC_NUMERIC, "de_DE.UTF-8") == nullptr)
    QSKIP("no comma locale on this system");

  Config core(&m_events);
  const bool accepted =
      parse(&m_events, QStringLiteral("section: links\n\tsrv:\n\t\tleft(31.8,100) = hp\nend\n"), core);
  std::setlocale(LC_NUMERIC, saved.constData());
  QVERIFY(!accepted);
}

void DisplayLayoutWriterTests::randomLayoutsAllParse()
{
  const QStringList names = {"c0", "c1", "c2", "c3", "c4", "c5"};
  addComputers(names);
  ServerConfig config;
  for (const auto &name : names)
    config.addClient(name);

  const QList<QList<QRect>> desks = {
      {{0, 0, 1000, 1000}}, kOffset, {{0, 0, 1080, 1920}, {1080, 240, 2560, 1440}, {3640, 420, 1920, 1080}}
  };
  QRandomGenerator random(20261106);
  for (int round = 0; round < 1000; ++round) {
    DisplayMonitors::setReported(desks.at(random.bounded(desks.size())));
    QList<LayoutClient> clients;
    for (int i = 0; i < names.size(); ++i) {
      std::optional<LayoutPlacement> placement;
      if (random.bounded(5) != 0) {
        const auto anchor = i > 0 && random.bounded(3) == 0 ? names.at(random.bounded(i)) : QString();
        placement = LayoutPlacement{
            anchor, static_cast<LayoutSide>(random.bounded(4)), random.bounded(-500, 4000), random.bounded(4) == 0
        };
      }
      clients.append(client(names.at(i), random.bounded(200, 3000), random.bounded(200, 2000), placement));
    }
    StoredLayout layout;
    layout.saved = true;
    layout.active = true;
    layout.clients = clients;
    for (const auto &entry : std::as_const(clients))
      layout.sizes.insert(entry.name, entry.size);
    QVERIFY(DisplayLayoutStore::save(layout));

    Config core(&m_events);
    std::string error;
    QVERIFY2(
        parse(&m_events, configText(config), core, &error),
        qPrintable(QStringLiteral("round %1: %2").arg(round).arg(QString::fromStdString(error)))
    );
  }
}

int main(int argc, char **argv)
{
  // a Computer holds a pixmap, which needs a GUI application
  if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
  }
  QApplication app(argc, argv);
  DisplayLayoutWriterTests tests;
  return QTest::qExec(&tests, argc, argv);
}
