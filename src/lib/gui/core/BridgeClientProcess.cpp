/*
 * Deskflow-hid -- created by locke.huang@gmail.com
 */

#include "BridgeClientProcess.h"
#include "common/Constants.h"
#include <QCoreApplication>
#include <QDebug>

namespace deskflow::gui {

const QRegularExpression BridgeClientProcess::s_connectedRegex(
    "connected to server|connection established", QRegularExpression::CaseInsensitiveOption
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
  QString appPath = QStringLiteral("%1/%2").arg(QCoreApplication::applicationDirPath(), kCoreBinName);

  QStringList args;
  args << "client";
  args << "--name" << config.screenName;
  args << "--link" << config.devicePath;
  args << "--remoteHost" << config.remoteHost;
  args << "--secure" << (config.tlsEnabled ? "true" : "false");
  args << "--log-level" << config.logLevel;
  args << "--screen-width" << QString::number(config.screenWidth);
  args << "--screen-height" << QString::number(config.screenHeight);
  args << "--yscroll" << QString::number(config.scrollSpeed);
  args << "--invertScrollDirection" << (config.invertScroll ? "true" : "false");

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

  return true;
}

void BridgeClientProcess::stop()
{
  if (m_process) {
    m_process->disconnect(this);
    m_process->terminate();
    if (!m_process->waitForFinished(1000)) {
      m_process->kill();
      m_process->waitForFinished(500);
    }
    m_process->deleteLater();
    m_process = nullptr;
  }
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

  QByteArray data = m_process->readAllStandardOutput();
  data += m_process->readAllStandardError();

  if (data.isEmpty())
    return;

  QString output = QString::fromLocal8Bit(data);
  QStringList lines = output.split('\n', Qt::SkipEmptyParts);
  for (const QString &line : lines) {
    parseLine(line);
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
  Q_EMIT finished(exitCode, exitStatus);
}

} // namespace deskflow::gui
