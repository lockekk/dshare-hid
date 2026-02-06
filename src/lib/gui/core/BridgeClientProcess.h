/*
 * dshare-hid -- created by locke.huang@gmail.com
 */

#pragma once

#include <QObject>
#include <QProcess>
#include <QRegularExpression>
#include <QString>
#include <QStringList>

namespace deskflow::gui {

class BridgeClientProcess : public QObject
{
  Q_OBJECT

public:
  struct Config
  {
    QString screenName;
    QString devicePath;
    QString remoteHost;
    bool tlsEnabled;
    QString logLevel;
    int screenWidth;
    int screenHeight;
    double yScrollScale;
    bool invertScroll;
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

Q_SIGNALS:
  void connectionEstablished();
  void deviceNameDetected(const QString &name);
  void activationStatusDetected(const QString &state, int profileIndex);
  void bleStatusDetected(bool connected);
  void handshakeFailed(const QString &reason);
  void logAvailable(const QString &line);
  void finished(int exitCode, QProcess::ExitStatus exitStatus);

private Q_SLOTS:
  void onReadyRead();
  void onFinished(int exitCode, QProcess::ExitStatus exitStatus);

private:
  void parseLine(const QString &line);

  QString m_devicePath;
  QProcess *m_process = nullptr;
  bool m_showLogs = false;

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
