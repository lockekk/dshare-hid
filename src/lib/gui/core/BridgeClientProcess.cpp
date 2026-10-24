/*
 * dshare-hid -- created by locke.huang@gmail.com
 */

#include "BridgeClientProcess.h"
#include "common/Constants.h"
#include "platform/bridge/BridgeRequests.h"
#include <QCoreApplication>
#include <QDeadlineTimer>
#include <QDebug>
#include <QTimer>

#include <utility>

namespace deskflow::gui {

namespace requests = deskflow::bridge::requests;

namespace {
// device path -> the started bridge client that owns it (GUI thread only)
QHash<QString, BridgeClientProcess *> s_deviceOwners;

// reading every profile takes one round trip each; allow for a slow device
constexpr int kRequestTimeoutMs = 5000;
} // namespace

const QRegularExpression BridgeClientProcess::s_connectedRegex(
    "connected to server|connected to secure socket|connection established", QRegularExpression::CaseInsensitiveOption
);
const QRegularExpression BridgeClientProcess::s_deviceNameRegex(R"(CDC:\s+firmware device name='([^']+)')");
const QRegularExpression
    BridgeClientProcess::s_activationRegex(R"(CDC:\s+handshake completed.*activation_state=(.*?)(?=\s+\w+=|$))");
const QRegularExpression BridgeClientProcess::s_activeProfileRegex(R"(active_profile=(\d+))");
const QRegularExpression BridgeClientProcess::s_bleRegex(R"(ble=(YES|NO))");
const QRegularExpression BridgeClientProcess::s_handshakeFailRegex(R"(ERROR: CDC: Handshake authentication failed.)");
const QRegularExpression
    BridgeClientProcess::s_handshakeTimeoutRegex(R"(ERROR: CDC: Timed out waiting for handshake ACK)");
const QRegularExpression
    BridgeClientProcess::s_logPrefixRegex(R"(^\[\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}(?:\.\d{3})?\]\s*[A-Z0-9]+:\s*)");
const QRegularExpression BridgeClientProcess::s_bridgePrefixRegex(R"(^\[Bridge\]\s*)");

BridgeClientProcess::BridgeClientProcess(const QString &devicePath, QObject *parent)
    : QObject(parent),
      m_devicePath(devicePath)
{
}

BridgeClientProcess::~BridgeClientProcess()
{
  stop();
}

bool BridgeClientProcess::start(const Config &config)
{
  if (m_process) {
    return false;
  }

  m_process = new QProcess(this);
  m_stdoutBuffer.clear();
  m_stderrBuffer.clear();
  m_abandonedRequests.clear();
  QString appPath = QStringLiteral("%1/%2").arg(QCoreApplication::applicationDirPath(), kCoreBinName);

  QStringList args;
  args << "client";
  args << "--name" << config.screenName;
  args << "--link" << config.devicePath;
  // Point the core at this exact config: its filename can differ from the screen
  // name (e.g. "Bridge-XXX-1.conf"), and the core reads bridge/* keys from it.
  if (!config.configPath.isEmpty()) {
    args << "--settings" << config.configPath;
  }
  args << "--remoteHost" << config.remoteHost;
  args << "--secure" << (config.tlsEnabled ? "true" : "false");
  args << "--log-level" << config.logLevel;
  args << "--screen-width" << QString::number(config.screenWidth);
  args << "--screen-height" << QString::number(config.screenHeight);

  connect(m_process, &QProcess::readyReadStandardOutput, this, &BridgeClientProcess::onReadyRead);
  connect(m_process, &QProcess::readyReadStandardError, this, &BridgeClientProcess::onReadyRead);
  connect(
      m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, &BridgeClientProcess::onFinished
  );

  qInfo() << "Starting bridge client process for" << m_devicePath << "with args:" << args.join(" ");
  m_process->start(appPath, args);

  if (!m_process->waitForStarted(3000)) {
    qWarning() << "Failed to start bridge client process for" << m_devicePath;
    m_process->deleteLater();
    m_process = nullptr;
    return false;
  }

  m_serialNumber = config.serialNumber;
  s_deviceOwners.insert(m_devicePath, this);
  return true;
}

BridgeClientProcess *BridgeClientProcess::forDevice(const QString &devicePath)
{
  return s_deviceOwners.value(devicePath, nullptr);
}

void BridgeClientProcess::releaseDevice()
{
  if (s_deviceOwners.value(m_devicePath) == this) {
    s_deviceOwners.remove(m_devicePath);
  }
}

bool BridgeClientProcess::request(
    const QString &command, const QStringList &arguments, QStringList &fields, QString &error
)
{
  if (!isStarted()) {
    error = tr("the bridge client isn't running");
    return false;
  }
  if (!m_abandonedRequests.isEmpty()) {
    // it hasn't answered an earlier request yet: don't block the GUI on it again
    error = tr("the bridge client isn't answering");
    return false;
  }

  const quint64 id = ++m_lastRequestId;
  m_waitingRequests.insert(id);
  ++m_requestDepth; // waitForBytesWritten() can already read lines
  m_process->write((requests::formatRequest(id, command, arguments) + QLatin1Char('\n')).toUtf8());
  m_process->waitForBytesWritten(1000);

  const QDeadlineTimer deadline(kRequestTimeoutMs);
  while (!m_responses.contains(id) && isStarted() && !deadline.hasExpired()) {
    m_process->waitForReadyRead(static_cast<int>(deadline.remainingTime()));
  }
  --m_requestDepth;
  m_waitingRequests.remove(id);
  if (m_requestDepth == 0 && (!m_deferredLines.isEmpty() || m_pendingExit)) {
    QTimer::singleShot(0, this, &BridgeClientProcess::parseDeferredLines);
  }

  if (!m_responses.contains(id)) {
    if (isStarted()) {
      // the core still carries it out, so watch for its late reply (readLines)
      m_abandonedRequests.insert(id, command);
      error = tr("the bridge client didn't answer");
    } else {
      error = tr("the bridge client stopped");
    }
    qWarning() << "Bridge client request" << command << "for" << m_devicePath << "failed:" << error;
    return false;
  }
  const Response response = m_responses.take(id);
  if (!response.ok) {
    error = response.fields.value(0);
    qWarning() << "Bridge client request" << command << "for" << m_devicePath << "failed:" << error;
    return false;
  }
  fields = response.fields;
  return true;
}

void BridgeClientProcess::stop()
{
  if (m_process) {
    m_process->disconnect(this);
    if (m_process->state() != QProcess::NotRunning) {
      // ask the core to shut down cleanly, which releases keys still held on the device
      // (BridgeClientApp watches stdin for this).  terminate() is only a fallback: on Windows
      // it can't stop a process that has no windows, so it would always end in kill().
      m_process->write("stop\n");
      m_process->waitForBytesWritten(200);
      if (m_process->waitForFinished(1000)) {
        qInfo() << "Bridge client process stopped for" << m_devicePath;
      } else {
        qWarning() << "Bridge client process for" << m_devicePath << "didn't stop on request, terminating";
        m_process->terminate();
        if (!m_process->waitForFinished(500)) {
          m_process->kill();
          m_process->waitForFinished(500);
        }
      }
    }
    m_process->deleteLater();
    m_process = nullptr;
  }
  releaseDevice();
  // stopped on purpose: none of its output or its exit is reported any more
  m_deferredLines.clear();
  m_pendingExit.reset();
  m_abandonedRequests.clear();
}

bool BridgeClientProcess::isStarted() const
{
  return m_process && m_process->state() == QProcess::Running;
}

qint64 BridgeClientProcess::processId() const
{
  return m_process ? m_process->processId() : 0;
}

void BridgeClientProcess::onReadyRead()
{
  if (!m_process)
    return;

  readLines(m_stdoutBuffer, m_process->readAllStandardOutput());
  if (m_process) { // a line's signal may have stopped it
    readLines(m_stderrBuffer, m_process->readAllStandardError());
  }
}

void BridgeClientProcess::readLines(QByteArray &buffer, const QByteArray &data)
{
  buffer += data;
  for (qsizetype end = buffer.indexOf('\n'); end >= 0; end = buffer.indexOf('\n')) {
    const QString line = QString::fromLocal8Bit(buffer.left(end)).trimmed();
    buffer.remove(0, end + 1);
    if (line.isEmpty()) {
      continue;
    }

    quint64 id = 0;
    bool ok = false;
    QStringList fields;
    if (requests::parseResponse(line, id, ok, fields)) {
      if (m_waitingRequests.contains(id)) {
        m_responses.insert(id, {ok, fields});
      } else if (
          const QString command = m_abandonedRequests.take(id);
          ok && (command == requests::kProfiles || requests::changesActiveProfile(command))
      ) {
        // too late for its request, but it still tells what the device is set to now; this
        // may run inside another request's wait, so report it after that
        QTimer::singleShot(0, this, [this, fields] { Q_EMIT profilesReported(fields); });
      }
      continue;
    }

    // only recorded, so it needn't wait for a request
    if (bool entered = false; requests::parseFocus(line, entered)) {
      Q_EMIT focusChanged(entered);
      continue;
    }

    // while a request waits, hold other lines back: their signals could stop this process,
    // or start another request, in the middle of the wait
    if (m_requestDepth > 0) {
      m_deferredLines.append(line);
      continue;
    }
    parseLine(line);
  }
}

void BridgeClientProcess::parseDeferredLines()
{
  const QStringList lines = std::exchange(m_deferredLines, {});
  for (const QString &line : lines) {
    parseLine(line);
  }
  if (const auto exit = std::exchange(m_pendingExit, std::nullopt)) {
    Q_EMIT finished(exit->exitCode, exit->exitStatus);
  }
}

void BridgeClientProcess::parseLine(const QString &line)
{
  QString cleanLine = line.trimmed();

  // Check for specific markers
  if (s_connectedRegex.match(cleanLine).hasMatch()) {
    Q_EMIT connectionEstablished();
  }

  QRegularExpressionMatch deviceNameMatch = s_deviceNameRegex.match(cleanLine);
  if (deviceNameMatch.hasMatch()) {
    Q_EMIT deviceNameDetected(deviceNameMatch.captured(1));
  }

  QRegularExpressionMatch activationMatch = s_activationRegex.match(cleanLine);
  if (activationMatch.hasMatch()) {
    QString activationState = activationMatch.captured(1);
    QRegularExpressionMatch profileMatch = s_activeProfileRegex.match(cleanLine);
    int activeProfile = -1;
    if (profileMatch.hasMatch()) {
      activeProfile = profileMatch.captured(1).toInt();
    }

    // Clean up activation state string
    static const QRegularExpression parenRegex(R"(\(\d+\))");
    activationState.remove(parenRegex);
    activationState = activationState.trimmed();

    Q_EMIT activationStatusDetected(activationState, activeProfile);
  }

  QRegularExpressionMatch bleMatch = s_bleRegex.match(cleanLine);
  if (bleMatch.hasMatch()) {
    bool isBleConnected = (bleMatch.captured(1) == QStringLiteral("YES"));
    Q_EMIT bleStatusDetected(isBleConnected);
  }

  if (s_handshakeFailRegex.match(cleanLine).hasMatch()) {
    Q_EMIT handshakeFailed(QStringLiteral("Factory firmware detected"));
  }

  if (s_handshakeTimeoutRegex.match(cleanLine).hasMatch()) {
    Q_EMIT handshakeFailed(QStringLiteral("Handshake timeout"));
  }

  // Prepare log for output
  QString logLine = cleanLine;
  logLine.remove(s_logPrefixRegex);
  logLine.remove(s_bridgePrefixRegex);
  Q_EMIT logAvailable(logLine);
}

void BridgeClientProcess::onFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
  m_process->deleteLater();
  m_process = nullptr;
  releaseDevice();
  m_abandonedRequests.clear();

  // the core's last lines, which may tell why it quit (e.g. a failed handshake), come
  // before its exit; inside a request's wait they are held back, so the exit waits too
  m_pendingExit = Exit{exitCode, exitStatus};
  if (m_requestDepth == 0) {
    parseDeferredLines();
  }
}

} // namespace deskflow::gui
