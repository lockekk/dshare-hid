/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "CdcTransport.h"

#include "base/Log.h"

#include <QByteArray>
#include <QMessageAuthenticationCode>
#include <QRandomGenerator>
#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
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
constexpr uint8_t kUsbControlConfigResponse = 0x82;

constexpr uint8_t kUsbConfigGetDeviceName = 0x02;
constexpr uint8_t kUsbConfigSetDeviceName = 0x03;
constexpr uint8_t kUsbConfigGetSerialNumber = 0x04;


constexpr size_t kAckCoreLen = 16;
constexpr size_t kAckProtocolVersionIndex = 1;
constexpr size_t kAckActivationStateIndex = 2;
constexpr size_t kAckFirmwareVersionIndex = 3;
constexpr size_t kAckHardwareVersionIndex = 4;
constexpr size_t kAckMinimumPayloadSize = 1 + kAckCoreLen;

constexpr int kHandshakeTimeoutMs = 2000;
constexpr int kReadPollIntervalMs = 10;
constexpr int kConfigCommandTimeoutMs = 1000;
constexpr int kKeepAliveTimeoutMs = 1000;
constexpr size_t kMaxDeviceNameBytes = 22;
constexpr size_t kAuthKeyBytes = 32;
constexpr size_t kAuthNonceBytes = 8;
constexpr size_t kAuthTagBytes = 32;
constexpr size_t kHelloPayloadLen = 1 + 1 + kAuthNonceBytes + kAuthTagBytes;
constexpr size_t kAckPayloadLen = kAckCoreLen + kAuthNonceBytes + kAuthTagBytes;
constexpr uint8_t kAuthModeHmacSha256 = 0x01;
constexpr size_t kAckTotalPayloadWithId = 1 + kAckPayloadLen;
constexpr size_t kAckDeviceNonceOffset = 1 + kAckCoreLen;
constexpr size_t kAckTagOffset = kAckDeviceNonceOffset + kAuthNonceBytes;

const uint8_t kHelloLabel[] = {'D', 'F', 'H', 'E', 'L', 'L', 'O'};
const uint8_t kAckLabel[] = {'D', 'F', 'A', 'C', 'K'};

const std::array<uint8_t, kAuthKeyBytes> kDefaultAuthKey = {
    0x9C, 0x3B, 0x1F, 0x04, 0xFE, 0x55, 0x80, 0x12,
    0xD9, 0x47, 0x2A, 0x6C, 0x3F, 0xE5, 0x9B, 0x01,
    0x75, 0xA1, 0x47, 0x33, 0x2D, 0x84, 0x5F, 0x66,
    0x08, 0xBB, 0x3D, 0x12, 0x6A, 0x90, 0x4E, 0xD5,
};

std::string hexDump(const uint8_t *data, size_t length, size_t maxBytes = 64)
{
  if (data == nullptr || length == 0) {
    return {};
  }

  const size_t limit = (std::min)(length, maxBytes);

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

bool parseHexKeyChar(QChar ch, uint8_t &value)
{
  if (ch.isDigit()) {
    value = static_cast<uint8_t>(ch.unicode() - '0');
    return true;
  }
  if (ch >= QChar('a') && ch <= QChar('f')) {
    value = static_cast<uint8_t>(10 + (ch.unicode() - 'a'));
    return true;
  }
  if (ch >= QChar('A') && ch <= QChar('F')) {
    value = static_cast<uint8_t>(10 + (ch.unicode() - 'A'));
    return true;
  }
  return false;
}

bool parseAuthKeyHex(const QString &hex, std::array<uint8_t, kAuthKeyBytes> &out)
{
  const QString trimmed = hex.trimmed();
  if (trimmed.isEmpty()) {
    out = kDefaultAuthKey;
    return true;
  }
  if (trimmed.size() != static_cast<int>(kAuthKeyBytes * 2)) {
    return false;
  }

  for (int i = 0; i < trimmed.size(); i += 2) {
    uint8_t hi = 0;
    uint8_t lo = 0;
    if (!parseHexKeyChar(trimmed[i], hi) || !parseHexKeyChar(trimmed[i + 1], lo)) {
      return false;
    }
    out[static_cast<size_t>(i / 2)] = static_cast<uint8_t>((hi << 4) | lo);
  }
  return true;
}

QByteArray hmacSha256(const QByteArray &message, const std::array<uint8_t, kAuthKeyBytes> &key)
{
  const QByteArray keyBytes(reinterpret_cast<const char *>(key.data()), static_cast<int>(key.size()));
  return QMessageAuthenticationCode::hash(message, keyBytes, QCryptographicHash::Sha256);
}

QByteArray makeHelloHmac(const std::array<uint8_t, kAuthNonceBytes> &hostNonce, uint8_t hostVersion,
                         const std::array<uint8_t, kAuthKeyBytes> &key)
{
  QByteArray msg;
  msg.append(reinterpret_cast<const char *>(kHelloLabel), static_cast<int>(sizeof(kHelloLabel)));
  msg.append(reinterpret_cast<const char *>(hostNonce.data()), static_cast<int>(hostNonce.size()));
  msg.append(static_cast<char>(hostVersion));
  return hmacSha256(msg, key);
}

QByteArray makeAckHmac(const std::array<uint8_t, kAuthNonceBytes> &hostNonce,
                       const uint8_t *deviceNonce,
                       const uint8_t *ackCore,
                       const std::array<uint8_t, kAuthKeyBytes> &key)
{
  QByteArray msg;
  msg.append(reinterpret_cast<const char *>(kAckLabel), static_cast<int>(sizeof(kAckLabel)));
  msg.append(reinterpret_cast<const char *>(hostNonce.data()), static_cast<int>(hostNonce.size()));
  msg.append(reinterpret_cast<const char *>(deviceNonce), static_cast<int>(kAuthNonceBytes));
  msg.append(reinterpret_cast<const char *>(ackCore), static_cast<int>(kAckCoreLen));
  return hmacSha256(msg, key);
}
} // namespace

CdcTransport::CdcTransport(const QString &devicePath) : m_devicePath(devicePath)
{
  m_authKey = kDefaultAuthKey;
  resetState();
}

CdcTransport::~CdcTransport()
{
  close();
}

void CdcTransport::resetState()
{
  m_handshakeComplete = false;
  m_hostNonce.fill(0);
  m_hasHostNonce = false;
  m_rxBuffer.clear();
  m_hasDeviceConfig = false;
  m_deviceConfig = FirmwareConfig{};
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

  m_fd = reinterpret_cast<intptr_t>(handle);

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
    LOG_ERR("CDC: performHandshake aborted because device is not open.");
    return false;
  }

  quint64 randomValue = QRandomGenerator::global()->generate64();
  std::memcpy(m_hostNonce.data(), &randomValue, kAuthNonceBytes);
  m_hasHostNonce = true;

  std::vector<uint8_t> payload(1 + kHelloPayloadLen);
  payload[0] = kUsbControlHello;
  payload[1] = kUsbLinkVersion;
  payload[2] = kAuthModeHmacSha256;
  std::memcpy(payload.data() + 3, m_hostNonce.data(), kAuthNonceBytes);
  const QByteArray helloTag = makeHelloHmac(m_hostNonce, kUsbLinkVersion, m_authKey);
  std::memcpy(payload.data() + 3 + kAuthNonceBytes, helloTag.constData(), kAuthTagBytes);

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
      if (!m_hasHostNonce) {
        m_lastError = "Handshake host nonce not initialized";
        LOG_ERR("CDC: %s", m_lastError.c_str());
        return false;
      }

      if (framePayload.size() < kAckTotalPayloadWithId) {
        m_lastError = "Handshake ACK payload too short";
        LOG_ERR("CDC: %s (size=%zu)", m_lastError.c_str(), framePayload.size());
        return false;
      }

      const uint8_t *ackCore = framePayload.data() + 1;
      const uint8_t *deviceNonce = framePayload.data() + kAckDeviceNonceOffset;
      const uint8_t *ackTag = framePayload.data() + kAckTagOffset;

      const QByteArray computedTag = makeAckHmac(m_hostNonce, deviceNonce, ackCore, m_authKey);
      if (computedTag.size() != static_cast<int>(kAuthTagBytes) ||
          std::memcmp(computedTag.constData(), ackTag, kAuthTagBytes) != 0) {
        m_lastError = "Handshake authentication tag mismatch";
        LOG_ERR("CDC: %s", m_lastError.c_str());
        return false;
      }

      m_handshakeComplete = true;

      if (framePayload.size() >= kAckMinimumPayloadSize) {
        const uint8_t protocolVersion = framePayload[kAckProtocolVersionIndex];
        const ActivationState activationState = static_cast<ActivationState>(framePayload[kAckActivationStateIndex]);
        const uint8_t firmwareBcd = framePayload[kAckFirmwareVersionIndex];
        const uint8_t hardwareBcd = framePayload[kAckHardwareVersionIndex];

        m_deviceConfig.protocolVersion = protocolVersion;
        m_deviceConfig.activationState = activationState;
        m_deviceConfig.firmwareVersionBcd = firmwareBcd;
        m_deviceConfig.hardwareVersionBcd = hardwareBcd;
        m_hasDeviceConfig = true;

        LOG_INFO(
            "CDC: handshake completed version=%u activation_state=%s(%u) fw_bcd=%u hw_bcd=%u",
            protocolVersion,
            activationStateToString(activationState),
            static_cast<unsigned>(framePayload[kAckActivationStateIndex]),
            static_cast<unsigned>(firmwareBcd),
            static_cast<unsigned>(hardwareBcd)
        );

        std::string fetchedName;
        if (fetchDeviceName(fetchedName)) {
          LOG_INFO("CDC: firmware device name='%s'", fetchedName.c_str());
        } else {
          LOG_WARN("CDC: failed to read device name: %s", m_lastError.c_str());
          m_lastError.clear();
        }

        // Also fetch serial number during handshake
        std::string serialNumber;
        if (fetchSerialNumber(serialNumber)) {
          LOG_INFO("CDC: firmware serial number='%s'", serialNumber.c_str());
        } else {
          LOG_WARN("CDC: failed to read serial number: %s", m_lastError.c_str());
          m_lastError.clear();
        }
      } else {
        LOG_WARN(
            "CDC: handshake ACK missing metadata (payload=%zu)",
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


bool CdcTransport::waitForConfigResponse(uint8_t &msgType, uint8_t &status, std::vector<uint8_t> &payload, int timeoutMs)
{
  auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
  while (std::chrono::steady_clock::now() < deadline) {
    uint8_t frameType = 0;
    uint8_t flags = 0;
    std::vector<uint8_t> framePayload;
    if (!readFrame(frameType, flags, framePayload, kReadPollIntervalMs)) {
      continue;
    }
    if (frameType != kUsbFrameTypeControl || framePayload.empty() || framePayload[0] != kUsbControlConfigResponse) {
      continue;
    }
    if (framePayload.size() < 3) {
      m_lastError = "Config response too short";
      return false;
    }
    msgType = framePayload[1];
    status = framePayload[2];
    payload.assign(framePayload.begin() + 3, framePayload.end());
    return true;
  }
  m_lastError = "Timed out waiting for config response";
  return false;
}

bool CdcTransport::waitForControlMessage(uint8_t controlId, std::vector<uint8_t> &payload, int timeoutMs)
{
  auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
  while (std::chrono::steady_clock::now() < deadline) {
    uint8_t frameType = 0;
    uint8_t flags = 0;
    std::vector<uint8_t> framePayload;
    if (!readFrame(frameType, flags, framePayload, kReadPollIntervalMs)) {
      continue;
    }
    if (frameType != kUsbFrameTypeControl || framePayload.empty() || framePayload[0] != controlId) {
      continue;
    }

    payload.assign(framePayload.begin() + 1, framePayload.end());
    return true;
  }

  std::ostringstream oss;
  oss << "Timed out waiting for control message 0x" << std::hex << std::uppercase << std::setw(2) << std::setfill('0')
      << static_cast<unsigned>(controlId);
  m_lastError = oss.str();
  return false;
}

bool CdcTransport::fetchDeviceName(std::string &outName)
{
  if (!ensureOpen()) {
    return false;
  }

  std::vector<uint8_t> payload(1);
  payload[0] = kUsbConfigGetDeviceName;
  if (!sendUsbFrame(kUsbFrameTypeControl, 0, payload)) {
    return false;
  }

  uint8_t msgType = 0;
  uint8_t status = 0;
  std::vector<uint8_t> data;
  if (!waitForConfigResponse(msgType, status, data, kConfigCommandTimeoutMs)) {
    return false;
  }

  if (msgType != kUsbConfigGetDeviceName) {
    m_lastError = "Unexpected config response";
    return false;
  }
  if (status != 0) {
    m_lastError = "Firmware error code " + std::to_string(status);
    return false;
  }

  outName.assign(reinterpret_cast<const char *>(data.data()), data.size());
  m_deviceConfig.deviceName = outName;
  return true;
}

bool CdcTransport::setDeviceName(const std::string &name)
{
  if (!ensureOpen()) {
    return false;
  }
  if (name.size() > kMaxDeviceNameBytes) {
    m_lastError = "Device name must be <= 22 bytes";
    return false;
  }

  std::vector<uint8_t> payload(1 + name.size());
  payload[0] = kUsbConfigSetDeviceName;
  std::copy(name.begin(), name.end(), payload.begin() + 1);

  if (!sendUsbFrame(kUsbFrameTypeControl, 0, payload)) {
    return false;
  }

  uint8_t msgType = 0;
  uint8_t status = 0;
  std::vector<uint8_t> data;
  if (!waitForConfigResponse(msgType, status, data, kConfigCommandTimeoutMs)) {
    return false;
  }

  if (msgType != kUsbConfigSetDeviceName) {
    m_lastError = "Unexpected config response";
    return false;
  }
  if (status != 0) {
    m_lastError = "Firmware error code " + std::to_string(status);
    LOG_ERR("CDC: setDeviceName firmware returned error=%u", status);
    return false;
  }

  m_deviceConfig.deviceName = name;
  return true;
}





bool CdcTransport::fetchSerialNumber(std::string &outSerial)
{
  if (!ensureOpen()) {
    return false;
  }

  std::vector<uint8_t> payload(1);
  payload[0] = kUsbConfigGetSerialNumber;
  if (!sendUsbFrame(kUsbFrameTypeControl, 0, payload)) {
    return false;
  }

  uint8_t msgType = 0;
  uint8_t status = 0;
  std::vector<uint8_t> data;
  if (!waitForConfigResponse(msgType, status, data, kConfigCommandTimeoutMs)) {
    return false;
  }

  if (msgType != kUsbConfigGetSerialNumber) {
    m_lastError = "Unexpected config response";
    return false;
  }
  if (status != 0) {
    m_lastError = "Firmware error code " + std::to_string(status);
    LOG_ERR("CDC: fetchSerialNumber firmware returned error=%u", status);
    return false;
  }

  // Firmware returns any readable string (null-terminated)
  outSerial.assign(reinterpret_cast<const char *>(data.data()), data.size());
  return true;
}

bool CdcTransport::setAuthKeyHex(const QString &hex)
{
  std::array<uint8_t, kAuthKeyBytes> parsed{};
  if (!parseAuthKeyHex(hex, parsed)) {
    m_lastError = "Authentication key must be exactly 64 hexadecimal characters";
    return false;
  }

  m_authKey = parsed;
  return true;
}

bool CdcTransport::isValidAuthKeyHex(const QString &hex)
{
  std::array<uint8_t, kAuthKeyBytes> parsed{};
  return parseAuthKeyHex(hex, parsed);
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
