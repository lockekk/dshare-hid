/*
 * dshare-hid -- created by locke.huang@gmail.com
 */

#pragma once

#include "CdcTransport.h"

#include <QList>
#include <QString>
#include <QStringList>

#include <functional>
#include <mutex>
#include <string>
#include <vector>

/**
 * @brief Device requests the GUI sends a running bridge core
 *
 * While a bridge core runs it holds its device open exclusively, so the GUI asks the core to
 * talk to the device instead of opening the port.  One line each way, over the core's
 * stdin and stdout:
 *
 *   GUI -> core:  req <id> <command> [<argument>...]
 *   core -> GUI:  @bridge-rsp <id> ok [<field>...]
 *                 @bridge-rsp <id> error <message>
 *
 *   command                  reply fields
 *   profiles                 <active index> <profile>...  (every profile, by index)
 *   switch <index>           as profiles, once switched
 *   save <index> <profile>   as profiles, once saved
 *   erase <index>            as profiles, once erased
 *   getname                  <name>  (none when the name is empty)
 *   setname <name>           -
 *
 * A profile travels as the hex of its DeviceProfile bytes and a name as the hex of its UTF-8,
 * so no field contains a space.  While no core runs, the GUI carries out the same commands on
 * the device itself (execute()), so both ways answer alike.
 *
 * Unasked, the core also tells the GUI when the server's cursor enters or leaves the bridge's
 * computer, which the profile shortcuts switch (see gui/core/BridgeProfileShortcuts):
 *
 *   core -> GUI:  @bridge-focus in|out
 */
namespace deskflow::bridge::requests {

inline const QString kRequest = QStringLiteral("req");
inline const QString kResponse = QStringLiteral("@bridge-rsp");
inline const QString kFocus = QStringLiteral("@bridge-focus");

inline const QString kProfiles = QStringLiteral("profiles");
inline const QString kSwitch = QStringLiteral("switch");
inline const QString kSave = QStringLiteral("save");
inline const QString kErase = QStringLiteral("erase");
inline const QString kGetName = QStringLiteral("getname");
inline const QString kSetName = QStringLiteral("setname");

struct Request
{
  quint64 id = 0;
  QString command;
  QStringList arguments;
};

QString formatRequest(quint64 id, const QString &command, const QStringList &arguments = {});
bool parseRequest(const QString &line, Request &request);

/// on failure, fields holds just the error message
QString formatResponse(quint64 id, bool ok, const QStringList &fields);
bool parseResponse(const QString &line, quint64 &id, bool &ok, QStringList &fields);

QString formatFocus(bool entered);
bool parseFocus(const QString &line, bool &entered);

/**
 * @brief Carry out a command on an open transport
 * @param fields The reply's fields, on success
 * @param error Why it failed, otherwise
 */
bool execute(
    CdcTransport &transport, const QString &command, const QStringList &arguments, QStringList &fields, QString &error
);

/// whether the command, once carried out, may have changed the active profile's settings
bool changesActiveProfile(const QString &command);

QString encodeProfile(const DeviceProfile &profile);
bool decodeProfile(const QString &field, DeviceProfile &profile);
/// fields of a profiles reply
bool decodeProfiles(const QStringList &fields, int &activeIndex, QList<DeviceProfile> &profiles);
QString encodeText(const QString &text);
QString decodeText(const QString &field);

/**
 * @brief Request lines handed from the core's stdin reader thread to its event thread
 */
class Queue
{
public:
  void push(std::string line);
  std::vector<std::string> takeAll();

  /// run post() (which posts an event) unless close() was called: the reader thread can
  /// outlive the event queue and the log, which the core destroys on exit
  void postUnlessClosed(const std::function<void()> &post);
  void close();

private:
  std::mutex m_mutex;
  std::vector<std::string> m_lines;
  bool m_closed = false;
};

} // namespace deskflow::bridge::requests
