/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "CdcTransport.h"

#include "base/Log.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <iomanip>
#include <random>
#include <sstream>
#include <thread>

#if defined(Q_OS_UNIX)
#include <cerrno>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#elif defined(Q_OS_WIN)
#include <windows.h>
#endif

namespace deskflow::bridge {

namespace {
constexpr uint16_t kUsbLinkMagic = 0xC35A;
constexpr uint8_t kUsbLinkVersion = 1;
constexpr uint8_t kUsbFrameTypeHid = 0x01;
constexpr uint8_t kUsbFrameTypeHidMouseCompact = 0x02;
constexpr uint8_t kUsbFrameTypeHidKeyCompact = 0x03;
constexpr uint8_t kUsbFrameTypeHidMouseButtonCompact = 0x04;
constexpr uint8_t kUsbFrameTypeHidScrollCompact = 0x05;
constexpr uint8_t kUsbFrameTypeControl = 0x80;

constexpr uint8_t kUsbControlHello = 0x01;
constexpr uint8_t kUsbControlAck = 0x81;

constexpr size_t kAckProtocolVersionIndex = 1;
constexpr size_t kAckHidConnectedIndex = 3;
constexpr size_t kAckScreenWidthIndex = 4;
constexpr size_t kAckScreenHeightIndex = 8;
constexpr size_t kAckScreenRotationIndex = 12;
constexpr size_t kAckMinimumPayloadSize = kAckScreenRotationIndex + 1;

constexpr int kHandshakeTimeoutMs = 2000;
constexpr int kReadPollIntervalMs = 10;

std::string hexDump(const uint8_t *data, size_t length, size_t maxBytes = 64)
{
  if (data == nullptr || length == 0) {
    return {};
  }

  const size_t limit = std::min(length, maxBytes);

  std::ostringstream oss;
  oss << std::hex << std::uppercase << std::setfill('0');
  for (size_t i = 0; i < limit; ++i) {
    if (i > 0)
      oss << ' ';
    oss << std::setw(2) << static_cast<unsigned>(data[i]);
  }

  if (length > maxBytes) {
    oss << " ...";
  }

  return oss.str();
}
} // namespace

CdcTransport::CdcTransport(const QString &devicePath) : m_devicePath(devicePath)
{
  resetState();
}

CdcTransport::~CdcTransport()
{
  close();
}

void CdcTransport::resetState()
{
  m_handshakeComplete = false;
  m_lastNonce = 0;
  m_rxBuffer.clear();
  m_hasDeviceConfig = false;
  m_deviceConfig = PicoConfig{};
}

bool CdcTransport::ensureOpen()
{
  if (isOpen() && m_handshakeComplete) {
    return true;
  }
  if (!open()) {
    return false;
  }
  return true;
}

bool CdcTransport::open()
{
  if (isOpen()) {
    if (m_handshakeComplete) {
      return true;
    }
    return performHandshake();
  }

#if defined(Q_OS_UNIX)
  m_fd = ::open(m_devicePath.toUtf8().constData(), O_RDWR | O_NOCTTY | O_NONBLOCK);
  if (m_fd < 0) {
    m_lastError = "Failed to open device: " + std::string(strerror(errno));
    LOG_ERR("CDC: %s", m_lastError.c_str());
    return false;
  }

  struct termios tty;
  if (tcgetattr(m_fd, &tty) != 0) {
    m_lastError = "Failed to get terminal attributes";
    LOG_ERR("CDC: %s", m_lastError.c_str());
    ::close(m_fd);
    m_fd = -1;
    return false;
  }

  cfmakeraw(&tty);
  cfsetospeed(&tty, B115200);
  cfsetispeed(&tty, B115200);
  tty.c_cc[VMIN] = 0;
  tty.c_cc[VTIME] = 1; // 100ms read timeout

  if (tcsetattr(m_fd, TCSANOW, &tty) != 0) {
    m_lastError = "Failed to set terminal attributes";
    LOG_ERR("CDC: %s", m_lastError.c_str());
    ::close(m_fd);
    m_fd = -1;
    return false;
  }
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

  COMMTIMEOUTS timeouts = {0};
  timeouts.ReadIntervalTimeout = 50;
  timeouts.ReadTotalTimeoutConstant = 100;
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
#else
  m_lastError = "CDC transport not implemented for this platform";
  return false;
#endif

  LOG_INFO("CDC: opened device %s", m_devicePath.toUtf8().constData());
  resetState();
  return performHandshake();
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
  resetState();
  LOG_INFO("CDC: closed device");
}

bool CdcTransport::isOpen() const
{
  return m_fd != -1;
}

bool CdcTransport::performHandshake()
{
  if (!isOpen()) {
    m_lastError = "Device not open";
    return false;
  }

  std::random_device rd;
  std::mt19937 rng(rd());
  std::uniform_int_distribution<uint32_t> dist;
  m_lastNonce = dist(rng);

  std::vector<uint8_t> payload(6);
  payload[0] = kUsbControlHello;
  payload[1] = kUsbLinkVersion;
  payload[2] = static_cast<uint8_t>(m_lastNonce & 0xFF);
  payload[3] = static_cast<uint8_t>((m_lastNonce >> 8) & 0xFF);
  payload[4] = static_cast<uint8_t>((m_lastNonce >> 16) & 0xFF);
  payload[5] = static_cast<uint8_t>((m_lastNonce >> 24) & 0xFF);

  if (!sendUsbFrame(kUsbFrameTypeControl, 0, payload)) {
    return false;
  }

  auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(kHandshakeTimeoutMs);
  while (std::chrono::steady_clock::now() < deadline) {
    uint8_t type = 0;
    uint8_t flags = 0;
    std::vector<uint8_t> framePayload;
    if (!readFrame(type, flags, framePayload, kReadPollIntervalMs)) {
      continue;
    }

    if (type != kUsbFrameTypeControl || framePayload.empty()) {
      continue;
    }

    if (framePayload[0] == kUsbControlAck) {
      m_handshakeComplete = true;

      if (framePayload.size() >= kAckMinimumPayloadSize) {
        const uint32_t screenWidth =
            static_cast<uint32_t>(framePayload[kAckScreenWidthIndex]) |
            (static_cast<uint32_t>(framePayload[kAckScreenWidthIndex + 1]) << 8) |
            (static_cast<uint32_t>(framePayload[kAckScreenWidthIndex + 2]) << 16) |
            (static_cast<uint32_t>(framePayload[kAckScreenWidthIndex + 3]) << 24);

        const uint32_t screenHeight =
            static_cast<uint32_t>(framePayload[kAckScreenHeightIndex]) |
            (static_cast<uint32_t>(framePayload[kAckScreenHeightIndex + 1]) << 8) |
            (static_cast<uint32_t>(framePayload[kAckScreenHeightIndex + 2]) << 16) |
            (static_cast<uint32_t>(framePayload[kAckScreenHeightIndex + 3]) << 24);

        const int32_t screenRotation = static_cast<int32_t>(framePayload[kAckScreenRotationIndex]);

        m_deviceConfig.screenWidth = static_cast<int32_t>(screenWidth);
        m_deviceConfig.screenHeight = static_cast<int32_t>(screenHeight);
        m_deviceConfig.screenRotation = screenRotation;
        m_hasDeviceConfig = true;

        const uint8_t protocolVersion = framePayload[kAckProtocolVersionIndex];
        const uint8_t hidConnected = framePayload[kAckHidConnectedIndex];

        LOG_INFO(
            "CDC: handshake completed version=%u hid=%u screen=%ux%u rot=%d",
            protocolVersion,
            hidConnected,
            screenWidth,
            screenHeight,
            screenRotation
        );
      } else {
        LOG_WARN(
            "CDC: handshake ACK missing display info (payload=%zu)",
            framePayload.size()
        );
        LOG_INFO("CDC: handshake completed");
      }
      return true;
    }
  }

  m_lastError = "Timed out waiting for handshake ACK";
  LOG_ERR("CDC: %s", m_lastError.c_str());
  return false;
}

bool CdcTransport::sendHidEvent(const HidEventPacket &packet)
{
  if (!ensureOpen()) {
    return false;
  }

  auto payload = packet.serialize();
  if (payload.empty()) {
    m_lastError = "Failed to serialize HID event";
    return false;
  }

  return sendUsbFrame(kUsbFrameTypeHid, 0, payload);
}

bool CdcTransport::sendMouseMoveCompact(int16_t dx, int16_t dy)
{
  if (!ensureOpen()) {
    return false;
  }

  constexpr int32_t kDeltaMin = -127;
  constexpr int32_t kDeltaMax = 127;

  const int32_t clampedDx = std::clamp(static_cast<int32_t>(dx), kDeltaMin, kDeltaMax);
  const int32_t clampedDy = std::clamp(static_cast<int32_t>(dy), kDeltaMin, kDeltaMax);

  const uint8_t packedDx = static_cast<uint8_t>(static_cast<int8_t>(clampedDx));
  const uint8_t packedDy = static_cast<uint8_t>(static_cast<int8_t>(clampedDy));

  std::array<uint8_t, 8> frame = {
      static_cast<uint8_t>(kUsbLinkMagic & 0xFF),
      static_cast<uint8_t>((kUsbLinkMagic >> 8) & 0xFF),
      kUsbLinkVersion,
      kUsbFrameTypeHidMouseCompact,
      packedDx,
      packedDy,
      0x00,
      0x00,
  };

  const std::string frameHex = hexDump(frame.data(), frame.size(), 32);
  LOG_DEBUG(
      "CDC: TX compact mouse frame type=0x%02x dx=%d dy=%d bytes=%s",
      kUsbFrameTypeHidMouseCompact,
      clampedDx,
      clampedDy,
      frameHex.c_str()
  );

  return writeAll(frame.data(), frame.size());
}

bool CdcTransport::sendKeyboardCompact(uint8_t modifiers, uint8_t keycode, bool isPress)
{
  if (!ensureOpen()) {
    return false;
  }

  std::array<uint8_t, 8> frame = {
      static_cast<uint8_t>(kUsbLinkMagic & 0xFF),
      static_cast<uint8_t>((kUsbLinkMagic >> 8) & 0xFF),
      kUsbLinkVersion,
      kUsbFrameTypeHidKeyCompact,
      modifiers,
      keycode,
      static_cast<uint8_t>(isPress ? 0x01 : 0x00),
      0x00,
  };

  const std::string frameHex = hexDump(frame.data(), frame.size(), 32);
  LOG_DEBUG(
      "CDC: TX compact key frame type=0x%02x press=%d mods=0x%02x key=0x%02x bytes=%s",
      kUsbFrameTypeHidKeyCompact,
      isPress ? 1 : 0,
      modifiers,
      keycode,
      frameHex.c_str()
  );

  return writeAll(frame.data(), frame.size());
}

bool CdcTransport::sendMouseButtonCompact(uint8_t buttons, bool isPress)
{
  if (!ensureOpen()) {
    return false;
  }

  std::array<uint8_t, 8> frame = {
      static_cast<uint8_t>(kUsbLinkMagic & 0xFF),
      static_cast<uint8_t>((kUsbLinkMagic >> 8) & 0xFF),
      kUsbLinkVersion,
      kUsbFrameTypeHidMouseButtonCompact,
      buttons,
      0x00,
      static_cast<uint8_t>(isPress ? 0x01 : 0x00),
      0x00,
  };

  const std::string frameHex = hexDump(frame.data(), frame.size(), 32);
  LOG_DEBUG(
      "CDC: TX compact mouse button frame type=0x%02x press=%d buttons=0x%02x bytes=%s",
      kUsbFrameTypeHidMouseButtonCompact,
      isPress ? 1 : 0,
      buttons,
      frameHex.c_str()
  );

  return writeAll(frame.data(), frame.size());
}

bool CdcTransport::sendMouseScrollCompact(int8_t delta)
{
  if (!ensureOpen()) {
    return false;
  }

  std::array<uint8_t, 8> frame = {
      static_cast<uint8_t>(kUsbLinkMagic & 0xFF),
      static_cast<uint8_t>((kUsbLinkMagic >> 8) & 0xFF),
      kUsbLinkVersion,
      kUsbFrameTypeHidScrollCompact,
      static_cast<uint8_t>(delta),
      0x00,
      0x00,
      0x00,
  };

  const std::string frameHex = hexDump(frame.data(), frame.size(), 32);
  LOG_DEBUG(
      "CDC: TX compact scroll frame type=0x%02x delta=%d bytes=%s",
      kUsbFrameTypeHidScrollCompact,
      delta,
      frameHex.c_str()
  );

  return writeAll(frame.data(), frame.size());
}

bool CdcTransport::sendUsbFrame(uint8_t type, uint8_t flags, const std::vector<uint8_t> &payload)
{
  return sendUsbFrame(type, flags, payload.data(), static_cast<uint16_t>(payload.size()));
}

bool CdcTransport::sendUsbFrame(uint8_t type, uint8_t flags, const uint8_t *payload, uint16_t length)
{
  if (!isOpen()) {
    m_lastError = "Device not open";
    return false;
  }

  std::vector<uint8_t> frame;
  frame.reserve(8 + length);

  frame.push_back(static_cast<uint8_t>(kUsbLinkMagic & 0xFF));
  frame.push_back(static_cast<uint8_t>((kUsbLinkMagic >> 8) & 0xFF));
  frame.push_back(kUsbLinkVersion);
  frame.push_back(type);
  frame.push_back(flags);
  frame.push_back(0); // reserved
  frame.push_back(static_cast<uint8_t>(length & 0xFF));
  frame.push_back(static_cast<uint8_t>((length >> 8) & 0xFF));

  if (length > 0 && payload != nullptr) {
    frame.insert(frame.end(), payload, payload + length);
  }

  std::string frameHex = hexDump(frame.data(), frame.size(), 128);
  if (!frameHex.empty()) {
    LOG_DEBUG(
        "CDC: TX frame type=0x%02x flags=0x%02x len=%u bytes=%s%s", type, flags, length, frameHex.c_str(),
        frame.size() > 128 ? " ..." : ""
    );
  } else {
    LOG_DEBUG("CDC: TX frame type=0x%02x flags=0x%02x len=%u", type, flags, length);
  }

  if (!writeAll(frame.data(), frame.size())) {
    return false;
  }

  return true;
}

bool CdcTransport::writeAll(const uint8_t *data, size_t length)
{
  size_t offset = 0;
  while (offset < length) {
#if defined(Q_OS_UNIX)
    ssize_t written = ::write(m_fd, data + offset, length - offset);
    if (written < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        std::this_thread::sleep_for(std::chrono::milliseconds(kReadPollIntervalMs));
        continue;
      }
      m_lastError = "Failed to write to device: " + std::string(strerror(errno));
      return false;
    }
    offset += static_cast<size_t>(written);
#elif defined(Q_OS_WIN)
    DWORD written = 0;
    if (!WriteFile(reinterpret_cast<HANDLE>(m_fd), data + offset, static_cast<DWORD>(length - offset), &written, nullptr)) {
      m_lastError = "Failed to write to device";
      return false;
    }
    offset += static_cast<size_t>(written);
#else
    m_lastError = "CDC transport not implemented for this platform";
    return false;
#endif
  }
  return true;
}

bool CdcTransport::readFrame(uint8_t &type, uint8_t &flags, std::vector<uint8_t> &payload, int timeoutMs)
{
  auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);

  while (std::chrono::steady_clock::now() < deadline) {
    if (m_rxBuffer.size() >= 8) {
      uint16_t magic = static_cast<uint16_t>(m_rxBuffer[0]) | (static_cast<uint16_t>(m_rxBuffer[1]) << 8);
      if (magic != kUsbLinkMagic) {
        m_rxBuffer.erase(m_rxBuffer.begin());
        continue;
      }

      uint8_t version = m_rxBuffer[2];
      uint8_t frameType = m_rxBuffer[3];
      uint8_t frameFlags = m_rxBuffer[4];
      uint16_t length = static_cast<uint16_t>(m_rxBuffer[6]) | (static_cast<uint16_t>(m_rxBuffer[7]) << 8);

      const size_t frameSize = 8 + length;
      if (m_rxBuffer.size() < frameSize) {
        // Need more data
      } else {
        // Complete frame
        payload.assign(m_rxBuffer.begin() + 8, m_rxBuffer.begin() + 8 + length);
        m_rxBuffer.erase(m_rxBuffer.begin(), m_rxBuffer.begin() + frameSize);

        if (version != kUsbLinkVersion) {
          continue;
        }

        type = frameType;
        flags = frameFlags;
        return true;
      }
    }

    uint8_t buffer[128];
#if defined(Q_OS_UNIX)
    ssize_t bytesRead = ::read(m_fd, buffer, sizeof(buffer));
    if (bytesRead > 0) {
      m_rxBuffer.insert(m_rxBuffer.end(), buffer, buffer + bytesRead);
    } else if (bytesRead == 0) {
      std::this_thread::sleep_for(std::chrono::milliseconds(kReadPollIntervalMs));
    } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
      std::this_thread::sleep_for(std::chrono::milliseconds(kReadPollIntervalMs));
    } else {
      m_lastError = "Failed to read from device: " + std::string(strerror(errno));
      return false;
    }
#elif defined(Q_OS_WIN)
    DWORD bytesRead = 0;
    if (ReadFile(reinterpret_cast<HANDLE>(m_fd), buffer, sizeof(buffer), &bytesRead, nullptr) && bytesRead > 0) {
      m_rxBuffer.insert(m_rxBuffer.end(), buffer, buffer + bytesRead);
    } else {
      std::this_thread::sleep_for(std::chrono::milliseconds(kReadPollIntervalMs));
    }
#else
    (void)buffer;
    m_lastError = "CDC transport not implemented for this platform";
    return false;
#endif
  }

  return false;
}

} // namespace deskflow::bridge
