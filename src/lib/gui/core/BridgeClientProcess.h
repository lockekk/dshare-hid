/*
 * dshare-hid -- created by locke.huang@gmail.com
 */

#pragma once

#include <QByteArray>
#include <QHash>
#include <QObject>
#include <QProcess>
#include <QRegularExpression>
#include <QSet>
#include <QString>
#include <QStringList>

#include <optional>

namespace deskflow::gui {

class BridgeClientProcess : public QObject
{
  Q_OBJECT

public:
  struct Config
  {
    QString screenName;
    QString serialNumber;
    QString devicePath;
    QString configPath;
    QString remoteHost;
    bool tlsEnabled;
    QString logLevel;
    int screenWidth;
    int screenHeight;
  };

  explicit BridgeClientProcess(const QString &devicePath, QObject *parent = nullptr);
  ~BridgeClientProcess() override;

  bool start(const Config &config);
  void stop();
  bool isStarted() const;
  qint64 processId() const;
  QString devicePath() const
  {
    return m_devicePath;
  }
  QString serialNumber() const
  {
    return m_serialNumber;
  }

  /**
   * @brief The started bridge client that owns a device, or nullptr
   *
   * From start() until its core has exited, the core holds the device open (exclusively),
   * so the GUI must ask it instead of opening the port.
   */
  static BridgeClientProcess *forDevice(const QString &devicePath);

  /**
   * @brief Have the core carry out a device request (see platform/bridge/BridgeRequests.h)
   *
   * Blocks until the reply arrives, the core stops, or the request times out.
   * @param fields The reply's fields, on success
   * @param error Why it failed, otherwise
   */
  bool request(const QString &command, const QStringList &arguments, QStringList &fields, QString &error);

Q_SIGNALS:
  void connectionEstablished();
  void deviceNameDetected(const QString &name);
  void activationStatusDetected(const QString &state, int profileIndex);
  void bleStatusDetected(bool connected);
  void handshakeFailed(const QString &reason);
  void logAvailable(const QString &line);
  void finished(int exitCode, QProcess::ExitStatus exitStatus);

  /// a reply that came after its request gave up, with the device's profiles as they are now
  /// (see deskflow::bridge::requests::decodeProfiles)
  void profilesReported(const QStringList &fields);

private Q_SLOTS:
  void onReadyRead();
  void onFinished(int exitCode, QProcess::ExitStatus exitStatus);

private:
  struct Response
  {
    bool ok = false;
    QStringList fields;
  };

  struct Exit
  {
    int exitCode = 0;
    QProcess::ExitStatus exitStatus = QProcess::NormalExit;
  };

  void readLines(QByteArray &buffer, const QByteArray &data);
  void parseLine(const QString &line);
  void parseDeferredLines();
  void releaseDevice();

  QString m_devicePath;
  QString m_serialNumber;
  QProcess *m_process = nullptr;
  bool m_showLogs = false;

  // a read can end mid-line, and each channel has its own partial line
  QByteArray m_stdoutBuffer;
  QByteArray m_stderrBuffer;

  quint64 m_lastRequestId = 0;
  QSet<quint64> m_waitingRequests;
  QHash<quint64, Response> m_responses;
  int m_requestDepth = 0;                      // requests waiting for their reply
  QStringList m_deferredLines;                 // other lines that arrived meanwhile
  std::optional<Exit> m_pendingExit;           // the core's exit, reported after those lines
  QHash<quint64, QString> m_abandonedRequests; // id -> command of requests that timed out

  static const QRegularExpression s_connectedRegex;
  static const QRegularExpression s_deviceNameRegex;
  static const QRegularExpression s_activationRegex;
  static const QRegularExpression s_activeProfileRegex;
  static const QRegularExpression s_bleRegex;
  static const QRegularExpression s_handshakeFailRegex;
  static const QRegularExpression s_handshakeTimeoutRegex;
  static const QRegularExpression s_logPrefixRegex;
  static const QRegularExpression s_bridgePrefixRegex;
};

} // namespace deskflow::gui
