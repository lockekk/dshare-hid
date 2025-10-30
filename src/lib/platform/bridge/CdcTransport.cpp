/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "CdcTransport.h"

#include "base/Log.h"

#include <QFile>
#include <QThread>

#if defined(Q_OS_UNIX)
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#elif defined(Q_OS_WIN)
#include <windows.h>
#endif

namespace deskflow::bridge {

CdcTransport::CdcTransport(const QString &devicePath) : m_devicePath(devicePath)
{
}

CdcTransport::~CdcTransport()
{
  close();
}

bool CdcTransport::open()
{
  if (isOpen()) {
    return true;
  }

#if defined(Q_OS_UNIX)
  m_fd = ::open(m_devicePath.toUtf8().constData(), O_RDWR | O_NOCTTY | O_NONBLOCK);
  if (m_fd < 0) {
    m_lastError = "Failed to open device: " + std::string(strerror(errno));
    LOG_ERR("CDC: %s", m_lastError.c_str());
    return false;
  }

  // Configure serial port settings
  struct termios tty;
  if (tcgetattr(m_fd, &tty) != 0) {
    m_lastError = "Failed to get terminal attributes";
    LOG_ERR("CDC: %s", m_lastError.c_str());
    ::close(m_fd);
    m_fd = -1;
    return false;
  }

  // Set raw mode
  cfmakeraw(&tty);

  // Set baud rate (115200)
  cfsetospeed(&tty, B115200);
  cfsetispeed(&tty, B115200);

  // Apply settings
  if (tcsetattr(m_fd, TCSANOW, &tty) != 0) {
    m_lastError = "Failed to set terminal attributes";
    LOG_ERR("CDC: %s", m_lastError.c_str());
    ::close(m_fd);
    m_fd = -1;
    return false;
  }

  LOG_INFO("CDC: opened device %s", m_devicePath.toUtf8().constData());
  return true;

#elif defined(Q_OS_WIN)
  HANDLE handle = CreateFileW(
      reinterpret_cast<LPCWSTR>(m_devicePath.utf16()), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING,
      FILE_ATTRIBUTE_NORMAL, nullptr
  );

  if (handle == INVALID_HANDLE_VALUE) {
    m_lastError = "Failed to open device";
    LOG_ERR("CDC: %s", m_lastError.c_str());
    return false;
  }

  m_fd = reinterpret_cast<int>(handle);

  // Configure DCB
  DCB dcb = {0};
  dcb.DCBlength = sizeof(DCB);

  if (!GetCommState(handle, &dcb)) {
    m_lastError = "Failed to get comm state";
    LOG_ERR("CDC: %s", m_lastError.c_str());
    CloseHandle(handle);
    m_fd = -1;
    return false;
  }

  dcb.BaudRate = CBR_115200;
  dcb.ByteSize = 8;
  dcb.StopBits = ONESTOPBIT;
  dcb.Parity = NOPARITY;

  if (!SetCommState(handle, &dcb)) {
    m_lastError = "Failed to set comm state";
    LOG_ERR("CDC: %s", m_lastError.c_str());
    CloseHandle(handle);
    m_fd = -1;
    return false;
  }

  // Set timeouts
  COMMTIMEOUTS timeouts = {0};
  timeouts.ReadIntervalTimeout = 50;
  timeouts.ReadTotalTimeoutConstant = 1000;
  timeouts.ReadTotalTimeoutMultiplier = 10;
  timeouts.WriteTotalTimeoutConstant = 1000;
  timeouts.WriteTotalTimeoutMultiplier = 10;

  if (!SetCommTimeouts(handle, &timeouts)) {
    m_lastError = "Failed to set comm timeouts";
    LOG_ERR("CDC: %s", m_lastError.c_str());
    CloseHandle(handle);
    m_fd = -1;
    return false;
  }

  LOG_INFO("CDC: opened device %s", m_devicePath.toUtf8().constData());
  return true;
#else
  m_lastError = "CDC transport not implemented for this platform";
  LOG_ERR("CDC: %s", m_lastError.c_str());
  return false;
#endif
}

void CdcTransport::close()
{
  if (!isOpen()) {
    return;
  }

#if defined(Q_OS_UNIX)
  ::close(m_fd);
#elif defined(Q_OS_WIN)
  CloseHandle(reinterpret_cast<HANDLE>(m_fd));
#endif

  m_fd = -1;
  LOG_INFO("CDC: closed device");
}

bool CdcTransport::isOpen() const
{
  return m_fd >= 0;
}

PicoConfig CdcTransport::queryConfig()
{
  PicoConfig config;

  if (!isOpen()) {
    m_lastError = "Device not open";
    return config;
  }

  // Query arch
  std::string archResponse;
  if (!sendCommand("GET_ARCH\n", archResponse)) {
    return config;
  }
  config.arch = archResponse;

  // Query screen info
  std::string screenResponse;
  if (!sendCommand("GET_SCREEN\n", screenResponse)) {
    return config;
  }

  // Parse screen response: "width,height,rotation,physWidth,physHeight,scale"
  if (sscanf(
          screenResponse.c_str(), "%d,%d,%d,%f,%f,%f", &config.screenWidth, &config.screenHeight,
          &config.screenRotation, &config.screenPhysicalWidth, &config.screenPhysicalHeight, &config.screenScaleFactor
      ) != 6) {
    m_lastError = "Failed to parse screen info";
    return config;
  }

  LOG_INFO(
      "CDC: config arch=%s screen=%dx%d rotation=%d", config.arch.c_str(), config.screenWidth, config.screenHeight,
      config.screenRotation
  );

  return config;
}

bool CdcTransport::sendHidFrame(const HidFrame &frame)
{
  if (!isOpen()) {
    m_lastError = "Device not open";
    return false;
  }

  std::vector<uint8_t> data = frame.serialize();
  if (data.empty()) {
    m_lastError = "Failed to serialize HID frame";
    return false;
  }

#if defined(Q_OS_UNIX)
  ssize_t written = ::write(m_fd, data.data(), data.size());
  if (written < 0 || static_cast<size_t>(written) != data.size()) {
    m_lastError = "Failed to write HID frame";
    return false;
  }
#elif defined(Q_OS_WIN)
  DWORD written = 0;
  if (!WriteFile(reinterpret_cast<HANDLE>(m_fd), data.data(), data.size(), &written, nullptr) ||
      written != data.size()) {
    m_lastError = "Failed to write HID frame";
    return false;
  }
#else
  m_lastError = "CDC transport not implemented for this platform";
  return false;
#endif

  return true;
}

bool CdcTransport::sendCommand(const std::string &command, std::string &response)
{
  if (!isOpen()) {
    m_lastError = "Device not open";
    return false;
  }

#if defined(Q_OS_UNIX)
  ssize_t written = ::write(m_fd, command.c_str(), command.size());
  if (written < 0 || static_cast<size_t>(written) != command.size()) {
    m_lastError = "Failed to write command";
    return false;
  }
#elif defined(Q_OS_WIN)
  DWORD written = 0;
  if (!WriteFile(
          reinterpret_cast<HANDLE>(m_fd), command.c_str(), command.size(), &written, nullptr
      ) ||
      written != command.size()) {
    m_lastError = "Failed to write command";
    return false;
  }
#else
  m_lastError = "CDC transport not implemented for this platform";
  return false;
#endif

  return readResponse(response);
}

bool CdcTransport::readResponse(std::string &response, int timeoutMs)
{
  response.clear();

  char buffer[256];
  int elapsed = 0;

#if defined(Q_OS_UNIX)
  while (elapsed < timeoutMs) {
    ssize_t bytesRead = ::read(m_fd, buffer, sizeof(buffer) - 1);

    if (bytesRead > 0) {
      buffer[bytesRead] = '\0';
      response += buffer;

      // Check for newline (end of response)
      if (response.find('\n') != std::string::npos) {
        // Remove trailing newline
        if (!response.empty() && response.back() == '\n') {
          response.pop_back();
        }
        if (!response.empty() && response.back() == '\r') {
          response.pop_back();
        }
        return true;
      }
    } else if (bytesRead < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
      m_lastError = "Read error";
      return false;
    }

    // Wait a bit before next read
    QThread::msleep(10);
    elapsed += 10;
  }

  m_lastError = "Timeout waiting for response";
  return false;

#elif defined(Q_OS_WIN)
  DWORD bytesRead = 0;
  while (elapsed < timeoutMs) {
    if (ReadFile(reinterpret_cast<HANDLE>(m_fd), buffer, sizeof(buffer) - 1, &bytesRead, nullptr) &&
        bytesRead > 0) {
      buffer[bytesRead] = '\0';
      response += buffer;

      // Check for newline
      if (response.find('\n') != std::string::npos) {
        if (!response.empty() && response.back() == '\n') {
          response.pop_back();
        }
        if (!response.empty() && response.back() == '\r') {
          response.pop_back();
        }
        return true;
      }
    }

    QThread::msleep(10);
    elapsed += 10;
  }

  m_lastError = "Timeout waiting for response";
  return false;

#else
  m_lastError = "CDC transport not implemented for this platform";
  return false;
#endif
}

} // namespace deskflow::bridge
