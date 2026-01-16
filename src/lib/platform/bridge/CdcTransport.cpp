/*
 * Deskflow-hid -- created by locke.huang@gmail.com
 */

#include "CdcTransport.h"
#include "platform/OpenSSLCompat.h"

#include "base/Log.h"

#include <QByteArray>
#include <QRandomGenerator>
#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <iomanip>
#include <mutex>
#include <random>
#include <sstream>
#include <thread>

#include <openssl/core_names.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/obj_mac.h>
#include <openssl/param_build.h>
#include <openssl/params.h>
#include <openssl/x509.h>
#include <vector>

#if defined(Q_OS_UNIX)
#include <cerrno>
#include <fcntl.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>
#ifdef Q_OS_MAC
#include <CoreFoundation/CoreFoundation.h>
#endif
#elif defined(Q_OS_WIN)
#include <windows.h>
#endif

namespace deskflow::bridge {

namespace {
constexpr uint16_t kUsbLinkMagic = 0xC35A; // USB Control Link Constants
constexpr uint8_t kUsbLinkVersion = 0x01;
constexpr uint8_t kAuthModeNone = 0x00;

constexpr uint8_t kAuthModeEcdsa = 0x02; // New mode
constexpr uint8_t kUsbFrameTypeHid = 0x01;
constexpr uint8_t kUsbFrameTypeHidMouseCompact = 0x02;
constexpr uint8_t kUsbFrameTypeHidKeyCompact = 0x03;
constexpr uint8_t kUsbFrameTypeHidMouseButtonCompact = 0x04;
constexpr uint8_t kUsbFrameTypeHidScrollCompact = 0x05;
constexpr uint8_t kUsbFrameTypeControl = 0x80;

constexpr uint8_t kUsbControlHello = 0x01;
constexpr uint8_t kUsbControlKeepAlive = 0x09;

constexpr uint8_t kUsbControlAck = 0x81;
constexpr uint8_t kUsbControlConfigResponse = 0x82;

constexpr uint8_t kUsbConfigGetDeviceName = 0x02;
constexpr uint8_t kUsbConfigSetDeviceName = 0x03;
constexpr uint8_t kUsbConfigGetSerialNumber = 0x04;
constexpr uint8_t kUsbConfigActivateDevice = 0x07;
constexpr uint8_t kUsbConfigGotoFactory = 0x08;
constexpr uint8_t kUsbControlUnpairAll = 0x30;
constexpr uint8_t kUsbControlSwitchProfile = 0x31;
constexpr uint8_t kUsbControlGetProfile = 0x32;
constexpr uint8_t kUsbControlSetProfile = 0x33;
constexpr uint8_t kUsbControlEraseProfile = 0x34;
constexpr uint8_t kUsbControlEraseAllProfiles = 0x35;

constexpr size_t kAckCoreLen = 16;
constexpr size_t kAckProtocolVersionIndex = 1;
constexpr size_t kAckActivationStateIndex = 2;
constexpr size_t kAckFirmwareVersionIndex = 3;
constexpr size_t kAckHardwareVersionIndex = 4;
constexpr size_t kAckFirmwareModeIndex = 5;
constexpr size_t kAckOtaPartitionIndex = 9;
constexpr size_t kAckHidModeIndex = 7; // Now Profile Info
constexpr size_t kAckBleConnectionIndex = 8;
constexpr size_t kAckMinimumPayloadSize = 1 + kAckCoreLen;

constexpr int kHandshakeTimeoutMs = 2000; // 2000ms needed for Linux stability (slow device enumeration)
constexpr int kReadPollIntervalMs = 10;
constexpr int kConfigCommandTimeoutMs = 1000;
constexpr int kKeepAliveTimeoutMs = 1000;
constexpr size_t kMaxDeviceNameBytes = 22;

constexpr size_t kAuthNonceBytes = 32;
constexpr size_t kAuthTagBytes = 64; // Signature size (R+S)

constexpr size_t kHelloPayloadLen = 1 + 1 + kAuthNonceBytes + kAuthTagBytes;
constexpr size_t kAckPayloadLen = kAckCoreLen + kAuthNonceBytes + kAuthTagBytes;
constexpr size_t kAckTotalPayloadWithId = 1 + kAckPayloadLen;
constexpr size_t kAckDeviceNonceOffset = 1 + kAckCoreLen;
constexpr size_t kAckTagOffset = kAckDeviceNonceOffset + kAuthNonceBytes;

const uint8_t kHelloLabel[] = {'D', 'F', 'H', 'E', 'L', 'L', 'O'};
const uint8_t kAckLabel[] = {'D', 'F', 'A', 'C', 'K'};

// ECDSA Public Key (X || Y)
static const uint8_t kDevicePublicKey[] = {
#ifdef DESKFLOW_USE_GENERATED_PUBLIC_KEY
#include "deskflow_public_key.inc"
#else
    // Public Key from firmware certs/cdc_auth/cdc_auth_public_key.bin
    0x04, 0xf0, 0xbd, 0xff, 0x02, 0xb7, 0xd1, 0xcc, 0x26, 0x98, 0x99, 0xa6, 0x04, 0x2b, 0xb3, 0x51, 0xa6,
    0x72, 0x1a, 0xc5, 0x8d, 0xa1, 0x1b, 0xbe, 0x8e, 0xc9, 0x3c, 0xd1, 0xcf, 0xf6, 0x32, 0xf9,            // X
    0x72, 0x5b, 0x08, 0x12, 0x47, 0x8f, 0x94, 0x70, 0x1c, 0x4e, 0x51, 0xa7, 0xa3, 0xb1, 0xc4, 0xa2,      // Y part 1
    0x6a, 0x65, 0x7c, 0xe4, 0x99, 0x62, 0x60, 0xb2, 0xcf, 0xc1, 0xb2, 0x2d, 0x7c, 0x64, 0xf4, 0xc2, 0x74 // Y part 2
#endif
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

// Helper to append a DER integer
void appendDerInteger(std::vector<uint8_t> &der, const uint8_t *data, size_t len)
{
  // Skip leading zeros
  while (len > 0 && *data == 0) {
    data++;
    len--;
  }

  if (len == 0) {
    // Zero is encoded as 02 01 00
    der.push_back(0x02);
    der.push_back(0x01);
    der.push_back(0x00);
    return;
  }

  der.push_back(0x02); // INTEGER tag

  // If MSB is set, prepend 0x00 to make it positive
  if (data[0] & 0x80) {
    der.push_back(static_cast<uint8_t>(len + 1));
    der.push_back(0x00);
  } else {
    der.push_back(static_cast<uint8_t>(len));
  }

  der.insert(der.end(), data, data + len);
}

bool verifySignature(
    const uint8_t *hostNonce, const uint8_t *deviceNonce, const uint8_t *ackCore, const uint8_t *signature
)
{
  deskflow::platform::initializeOpenSSL();

  // 1. Reconstruct Message
  std::vector<uint8_t> msg;
  msg.reserve(sizeof(kAckLabel) + kAuthNonceBytes + kAuthNonceBytes + kAckCoreLen);
  // Reconstruct message: HostNonce || DeviceNonce
  // Firmware only signs the concatenated nonces
  msg.insert(msg.end(), hostNonce, hostNonce + kAuthNonceBytes);
  msg.insert(msg.end(), deviceNonce, deviceNonce + kAuthNonceBytes); // 2. Load Public Key (EVP_PKEY)
  // 2. Load Public Key (Via DER SubjectPublicKeyInfo)
  // Construct the ASN.1 structure manually to avoid provider parameter issues.
  // Sequence (id-ecPublicKey, prime256v1), BitString (0x04 | X | Y)
  const uint8_t *pubKeyData = kDevicePublicKey;
  if (sizeof(kDevicePublicKey) != 65 || kDevicePublicKey[0] != 0x04) {
    LOG_ERR("CDC: Invalid public key format (expected 65 bytes with 0x04 prefix)");
    return false;
  }

  // OIDs:
  // id-ecPublicKey: 1.2.840.10045.2.1 -> 06 07 2A 86 48 CE 3D 02 01
  // prime256v1:     1.2.840.10045.3.1.7 -> 06 08 2A 86 48 CE 3D 03 01 07
  static const uint8_t kDerHeader[] = {
      0x30, 0x59,                                                 // SEQUENCE, len=89
      0x30, 0x13,                                                 // SEQUENCE, len=19
      0x06, 0x07, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x02, 0x01,       // id-ecPublicKey
      0x06, 0x08, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x03, 0x01, 0x07, // prime256v1
      0x03, 0x42,                                                 // BIT STRING, len=66
      0x00                                                        // Unused bits padding
  };

  std::vector<uint8_t> derKey;
  derKey.reserve(sizeof(kDerHeader) + 65);
  derKey.insert(derKey.end(), kDerHeader, kDerHeader + sizeof(kDerHeader));
  derKey.insert(derKey.end(), pubKeyData, pubKeyData + 65);

  const uint8_t *p = derKey.data();
  EVP_PKEY *pkey = d2i_PUBKEY(nullptr, &p, static_cast<long>(derKey.size()));

  if (!pkey) {
    char errBuf[256];
    ERR_error_string_n(ERR_peek_last_error(), errBuf, sizeof(errBuf));
    LOG_ERR("CDC: Failed to load public key via d2i_PUBKEY: %s", errBuf);
    return false;
  }

  // 3. Convert Raw Signature (R||S) to DER
  // Signature is 64 bytes: 32 bytes R, 32 bytes S
  if (kAuthTagBytes != 64) {
    LOG_ERR("CDC: Unexpected signature size definition");
    EVP_PKEY_free(pkey);
    return false;
  }

  const uint8_t *r = signature;
  const uint8_t *s = signature + 32;

  std::vector<uint8_t> derSig;
  // Sequence content
  std::vector<uint8_t> seqContent;
  appendDerInteger(seqContent, r, 32);
  appendDerInteger(seqContent, s, 32);

  // Sequence Header
  derSig.push_back(0x30); // SEQUENCE
  // Assuming short length form (< 128 bytes) which is true for P-256 sigs (~70 bytes)
  derSig.push_back(static_cast<uint8_t>(seqContent.size()));
  derSig.insert(derSig.end(), seqContent.begin(), seqContent.end());

  // 4. Verify
  EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
  if (!mdctx) {
    LOG_ERR("CDC: Failed to create MD_CTX");
    EVP_PKEY_free(pkey);
    return false;
  }

  int ret = EVP_DigestVerifyInit(mdctx, nullptr, EVP_sha256(), nullptr, pkey);
  if (ret > 0) {
    ret = EVP_DigestVerify(mdctx, derSig.data(), derSig.size(), msg.data(), msg.size());
  }

  EVP_MD_CTX_free(mdctx);
  EVP_PKEY_free(pkey);

  if (ret != 1) {
    LOG_ERR("CDC: ECDSA verification failed (ret=%d)", ret);
    return false;
  }

  return true;
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
  m_isSecure = false;
  m_hostNonce.fill(0);
  m_hasHostNonce = false;
  m_rxBuffer.clear();
  m_hasDeviceConfig = false;
  m_deviceConfig = FirmwareConfig{};
}

bool CdcTransport::ensureOpen(bool allowInsecure)
{
  if (isOpen()) {
    if (m_handshakeComplete) {
      return true;
    }
    // If open but handshake not complete, try to complete it
    return performHandshake(allowInsecure);
  }
  // If not open, try to open and perform handshake
  if (!open(allowInsecure)) {
    return false;
  }
  return true;
}

bool CdcTransport::open(bool allowInsecure)
{
  if (isOpen()) {
    if (m_handshakeComplete) {
      return true;
    }
    return performHandshake(allowInsecure);
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
  return performHandshake(allowInsecure);
}

void CdcTransport::close()
{
  if (!isOpen()) {
    return;
  }

#if defined(Q_OS_UNIX)
  // Flush any pending data to avoid blocking on close if the device is stuck
  tcflush(m_fd, TCIOFLUSH);
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

bool CdcTransport::performHandshake(bool allowInsecure)
{
  if (!isOpen()) {
    m_lastError = "Device not open";
    LOG_ERR("CDC: performHandshake aborted because device is not open.");
    return false;
  }

  quint32 tempNonce[kAuthNonceBytes / sizeof(quint32)];
  QRandomGenerator::global()->fillRange(tempNonce);
  std::memcpy(m_hostNonce.data(), tempNonce, kAuthNonceBytes);
  m_hasHostNonce = true;

  std::vector<uint8_t> payload(1 + kHelloPayloadLen);
  payload[0] = kUsbControlHello;
  payload[1] = kUsbLinkVersion;

  if (allowInsecure) {
    payload[2] = kAuthModeNone;
    std::memcpy(payload.data() + 3, m_hostNonce.data(), kAuthNonceBytes);
  } else {
    payload[2] = kAuthModeEcdsa; // Request ECDSA
    // Randomize Host Nonce
    QRandomGenerator *gen = QRandomGenerator::global();
    quint32 *nonceWords = reinterpret_cast<quint32 *>(m_hostNonce.data());
    for (size_t i = 0; i < kAuthNonceBytes / 4; ++i) {
      nonceWords[i] = gen->generate();
    }
    std::memcpy(payload.data() + 3, m_hostNonce.data(), kAuthNonceBytes);
    // Zero out the auth tag area (last 64 bytes) since we use ECDSA/None for Hello
    std::memset(payload.data() + 3 + kAuthNonceBytes, 0, kAuthTagBytes);
  }

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

      // Verify tag (only if not in insecure mode and auth was requested)
      if (!allowInsecure) {
        if (!verifySignature(m_hostNonce.data(), deviceNonce, ackCore, ackTag)) {
          m_lastError = "Handshake authentication failed.";
          LOG_ERR("CDC: %s", m_lastError.c_str());
          return false;
        }
        m_isSecure = true;
      } else {
        m_isSecure = false;
      }

      m_handshakeComplete = true;

      if (framePayload.size() >= kAckMinimumPayloadSize) {
        const uint8_t protocolVersion = framePayload[kAckProtocolVersionIndex];
        const ActivationState activationState = static_cast<ActivationState>(framePayload[kAckActivationStateIndex]);
        const uint8_t firmwareBcd = framePayload[kAckFirmwareVersionIndex];
        const uint8_t hardwareBcd = framePayload[kAckHardwareVersionIndex];
        const FirmwareMode firmwareMode = static_cast<FirmwareMode>(framePayload[kAckFirmwareModeIndex]);
        const uint8_t hidMode = framePayload[kAckHidModeIndex];

        m_deviceConfig.protocolVersion = protocolVersion;
        m_deviceConfig.activationState = activationState;
        m_deviceConfig.firmwareVersionBcd = firmwareBcd;
        m_deviceConfig.hardwareVersionBcd = hardwareBcd;
        m_deviceConfig.firmwareMode = firmwareMode;

        // kAckHidModeIndex now contains Profile Info (Total << 4 | Active)
        uint8_t profileInfo = framePayload[kAckHidModeIndex];
        m_deviceConfig.activeProfile = profileInfo & 0x0F;
        m_deviceConfig.totalProfiles = (profileInfo >> 4) & 0x0F;

        m_deviceConfig.isBleConnected = (framePayload[kAckBleConnectionIndex] != 0);

        if (framePayload.size() > kAckOtaPartitionIndex) {
          m_deviceConfig.hasOtaPartition = (framePayload[kAckOtaPartitionIndex] != 0);
        } else {
          // If payload is too short (older firmware), we assume NO OTA partition support
          // This blocks online flashing for old firmware which is safer.
          m_deviceConfig.hasOtaPartition = false;
        }

        m_hasDeviceConfig = true;

        LOG_INFO(
            "CDC: handshake completed version=%u activation_state=%s(%u) fw_bcd=%u hw_bcd=%u fw_mode=%u "
            "active_profile=%u total_profiles=%u ble=%s ota_0=%s",
            protocolVersion, activationStateToString(activationState),
            static_cast<unsigned>(framePayload[kAckActivationStateIndex]), static_cast<unsigned>(firmwareBcd),
            static_cast<unsigned>(hardwareBcd), static_cast<unsigned>(firmwareMode), m_deviceConfig.activeProfile,
            m_deviceConfig.totalProfiles, m_deviceConfig.isBleConnected ? "YES" : "NO",
            m_deviceConfig.hasOtaPartition ? "YES" : "NO"
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
        LOG_DEBUG("CDC: performHandshake calling fetchSerialNumber");
        if (fetchSerialNumber(serialNumber)) {
          LOG_INFO("CDC: firmware serial number='%s'", serialNumber.c_str());
          LOG_DEBUG("CDC: performHandshake fetchSerialNumber done");
        } else {
          LOG_WARN("CDC: failed to read serial number: %s", m_lastError.c_str());
          m_lastError.clear();
        }
        LOG_DEBUG("CDC: performHandshake finished success");
      } else {
        LOG_WARN("CDC: handshake ACK missing metadata (payload=%zu)", framePayload.size());
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

  if (CLOG->getFilter() >= LogLevel::Debug) {
    std::string frameHex = hexDump(frame.data(), frame.size(), 128);
    if (!frameHex.empty()) {
      LOG_DEBUG(
          "CDC: TX frame type=0x%02x flags=0x%02x len=%u bytes=%s%s", type, flags, length, frameHex.c_str(),
          frame.size() > 128 ? " ..." : ""
      );
    } else {
      LOG_DEBUG("CDC: TX frame type=0x%02x flags=0x%02x len=%u", type, flags, length);
    }
  }

  if (!writeAll(frame.data(), frame.size())) {
    return false;
  }

  return true;
}

bool CdcTransport::sendKeepAlive(uint32_t &uptimeSeconds)
{
  if (!ensureOpen()) {
    return false;
  }

  std::vector<uint8_t> payload(1);
  payload[0] = kUsbControlKeepAlive;

  if (!sendUsbFrame(kUsbFrameTypeControl, 0, payload)) {
    LOG_ERR("CDC: failed to send keep-alive command");
    return false;
  }

  // Wait for response
  uint8_t msgType = 0;
  uint8_t status = 0;
  std::vector<uint8_t> responsePayload;

  if (!waitForConfigResponse(msgType, status, responsePayload, 1000)) {
    LOG_ERR("CDC: keep-alive response timeout");
    return false;
  }

  if (msgType != kUsbControlKeepAlive || status != 0) {
    LOG_ERR("CDC: keep-alive failed msgType=%u status=%u", msgType, status);
    return false;
  }

  // Parse uptime from response (4 bytes, little-endian)
  if (responsePayload.size() >= 4) {
    uptimeSeconds = static_cast<uint32_t>(responsePayload[0]) | (static_cast<uint32_t>(responsePayload[1]) << 8) |
                    (static_cast<uint32_t>(responsePayload[2]) << 16) |
                    (static_cast<uint32_t>(responsePayload[3]) << 24);
  } else {
    uptimeSeconds = 0;
  }

  LOG_DEBUG("CDC: keep-alive success uptime=%us", static_cast<unsigned>(uptimeSeconds));
  return true;
}

bool CdcTransport::waitForConfigResponse(
    uint8_t &msgType, uint8_t &status, std::vector<uint8_t> &payload, int timeoutMs
)
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

  LOG_DEBUG("CDC: fetchSerialNumber entry");
  std::vector<uint8_t> payload(1);
  payload[0] = kUsbConfigGetSerialNumber;
  if (!sendUsbFrame(kUsbFrameTypeControl, 0, payload)) {
    return false;
  }

  uint8_t msgType = 0;
  uint8_t status = 0;
  std::vector<uint8_t> data;
  if (!waitForConfigResponse(msgType, status, data, kConfigCommandTimeoutMs)) {
    LOG_ERR("CDC: fetchSerialNumber timeout");
    return false;
  }
  LOG_DEBUG("CDC: fetchSerialNumber got response type=0x%02X status=%u", msgType, status);

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
  LOG_DEBUG("CDC: fetchSerialNumber assigning data size=%zu", data.size());
  outSerial.assign(reinterpret_cast<const char *>(data.data()), data.size());
  LOG_DEBUG("CDC: fetchSerialNumber success");
  return true;
}

bool CdcTransport::activateDevice(const std::string &licenseCode)
{
  if (!ensureOpen()) {
    return false;
  }

  std::vector<uint8_t> payload(1 + licenseCode.size());
  payload[0] = kUsbConfigActivateDevice;
  std::copy(licenseCode.begin(), licenseCode.end(), payload.begin() + 1);

  if (!sendUsbFrame(kUsbFrameTypeControl, 0, payload)) {
    return false;
  }

  uint8_t msgType = 0;
  uint8_t status = 0;
  std::vector<uint8_t> data;
  if (!waitForConfigResponse(msgType, status, data, kConfigCommandTimeoutMs)) {
    return false;
  }

  if (msgType != kUsbConfigActivateDevice) {
    m_lastError = "Unexpected config response";
    return false;
  }
  if (status != 0) {
    m_lastError = "Firmware error code " + std::to_string(status);
    LOG_ERR("CDC: activateDevice firmware returned error=%u", status);
    return false;
  }

  return true;
}

bool CdcTransport::gotoFactory()
{
  if (!ensureOpen()) {
    return false;
  }

  LOG_INFO("CDC: Sending gotoFactory command (0x%02X)", kUsbConfigGotoFactory);

  // kUsbConfigGotoFactory = 0x08
  std::vector<uint8_t> payload(1);
  payload[0] = kUsbConfigGotoFactory;

  if (!sendUsbFrame(kUsbFrameTypeControl, 0, payload)) {
    LOG_ERR("CDC: Failed to send gotoFactory command");
    return false;
  }

  uint8_t msgType = 0;
  uint8_t status = 0;
  std::vector<uint8_t> data;
  if (!waitForConfigResponse(msgType, status, data, kConfigCommandTimeoutMs)) {
    LOG_ERR("CDC: Timeout waiting for gotoFactory response");
    return false;
  }

  if (msgType != kUsbConfigGotoFactory) {
    m_lastError = "Unexpected config response";
    LOG_ERR("CDC: Unexpected response type 0x%02X for gotoFactory", msgType);
    return false;
  }
  if (status != 0) {
    m_lastError = "Firmware error code " + std::to_string(status);
    LOG_ERR("CDC: gotoFactory firmware returned error=%u", status);
    return false;
  }

  LOG_INFO("CDC: gotoFactory success");
  return true;
}

bool CdcTransport::unpairAll()
{
  if (!ensureOpen()) {
    return false;
  }

  LOG_INFO("CDC: Sending unpairAll command (0x%02X)", kUsbControlUnpairAll);

  std::vector<uint8_t> payload(1);
  payload[0] = kUsbControlUnpairAll;

  if (!sendUsbFrame(kUsbFrameTypeControl, 0, payload)) {
    LOG_ERR("CDC: Failed to send unpairAll command");
    return false;
  }

  std::vector<uint8_t> response;
  if (!waitForControlMessage(kUsbControlAck, response, kConfigCommandTimeoutMs)) {
    LOG_ERR("CDC: Timeout waiting for unpairAll response");
    return false;
  }

  // Response payload is just [status]
  if (response.size() < 1) {
    m_lastError = "Invalid unpairAll response";
    LOG_ERR("CDC: %s", m_lastError.c_str());
    return false;
  }

  uint8_t status = response[0];
  if (status == 1) {
    LOG_INFO("CDC: unpairAll success (no bonds found)");
    return true;
  }
  if (status != 0) {
    m_lastError = "Firmware error code " + std::to_string(status);
    LOG_ERR("CDC: unpairAll firmware returned error=%u", status);
    return false;
  }

  LOG_INFO("CDC: unpairAll success");
  return true;
}

bool CdcTransport::getProfile(uint8_t index, DeviceProfile &outProfile)
{
  if (!ensureOpen()) {
    return false;
  }

  std::vector<uint8_t> payload(2);
  payload[0] = kUsbControlGetProfile;
  payload[1] = index;

  if (!sendUsbFrame(kUsbFrameTypeControl, 0, payload)) {
    LOG_ERR("CDC: Failed to send getProfile command");
    return false;
  }

  std::vector<uint8_t> response;
  if (!waitForControlMessage(kUsbControlAck, response, kConfigCommandTimeoutMs)) {
    LOG_ERR("CDC: Timeout waiting for getProfile response");
    return false;
  }

  // Response: [Status(1), ProfileData(52)]
  if (response.size() < 1 + sizeof(DeviceProfile)) {
    m_lastError = "Invalid getProfile response size";
    return false;
  }

  uint8_t status = response[0];
  if (status != 0) {
    m_lastError = "Firmware error code " + std::to_string(status);
    return false;
  }

  std::memcpy(&outProfile, response.data() + 1, sizeof(DeviceProfile));
  return true;
}

bool CdcTransport::setProfile(uint8_t index, const DeviceProfile &profile)
{
  if (!ensureOpen()) {
    return false;
  }

  // Payload: [Cmd(1), Index(1), ProfileData(39)]
  std::vector<uint8_t> payload(2 + sizeof(DeviceProfile));
  payload[0] = kUsbControlSetProfile;
  payload[1] = index;
  std::memcpy(payload.data() + 2, &profile, sizeof(DeviceProfile));

  if (!sendUsbFrame(kUsbFrameTypeControl, 0, payload)) {
    LOG_ERR("CDC: Failed to send setProfile command");
    return false;
  }

  std::vector<uint8_t> response;
  if (!waitForControlMessage(kUsbControlAck, response, kConfigCommandTimeoutMs)) {
    LOG_ERR("CDC: Timeout waiting for setProfile response");
    return false;
  }

  if (response.empty() || response[0] != 0) {
    m_lastError = response.empty() ? "Empty response" : "Firmware error code " + std::to_string(response[0]);
    return false;
  }

  return true;
}

bool CdcTransport::switchProfile(uint8_t index)
{
  if (!ensureOpen()) {
    return false;
  }

  std::vector<uint8_t> payload(2);
  payload[0] = kUsbControlSwitchProfile;
  payload[1] = index;

  if (!sendUsbFrame(kUsbFrameTypeControl, 0, payload)) {
    LOG_ERR("CDC: Failed to send switchProfile command");
    return false;
  }

  std::vector<uint8_t> response;
  if (!waitForControlMessage(kUsbControlAck, response, kConfigCommandTimeoutMs)) {
    LOG_ERR("CDC: Timeout waiting for switchProfile response");
    return false;
  }

  if (response.empty() || response[0] != 0) {
    m_lastError = response.empty() ? "Empty response" : "Firmware error code " + std::to_string(response[0]);
    return false;
  }

  return true;
}

bool CdcTransport::eraseProfile(uint8_t index)
{
  if (!ensureOpen()) {
    return false;
  }

  std::vector<uint8_t> payload(2);
  payload[0] = kUsbControlEraseProfile;
  payload[1] = index;

  if (!sendUsbFrame(kUsbFrameTypeControl, 0, payload)) {
    LOG_ERR("CDC: Failed to send eraseProfile command");
    return false;
  }

  std::vector<uint8_t> response;
  if (!waitForControlMessage(kUsbControlAck, response, kConfigCommandTimeoutMs)) {
    LOG_ERR("CDC: Timeout waiting for eraseProfile response");
    return false;
  }

  if (response.empty() || response[0] != 0) {
    m_lastError = response.empty() ? "Empty response" : "Firmware error code " + std::to_string(response[0]);
    return false;
  }

  return true;
}

bool CdcTransport::eraseAllProfiles()
{
  if (!ensureOpen()) {
    return false;
  }

  std::vector<uint8_t> payload(1);
  payload[0] = kUsbControlEraseAllProfiles;

  if (!sendUsbFrame(kUsbFrameTypeControl, 0, payload)) {
    LOG_ERR("CDC: Failed to send eraseAllProfiles command");
    return false;
  }

  std::vector<uint8_t> response;
  if (!waitForControlMessage(kUsbControlAck, response, kConfigCommandTimeoutMs)) {
    LOG_ERR("CDC: Timeout waiting for eraseAllProfiles response");
    return false;
  }

  if (response.empty() || response[0] != 0) {
    m_lastError = response.empty() ? "Empty response" : "Firmware error code " + std::to_string(response[0]);
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
    if (!WriteFile(
            reinterpret_cast<HANDLE>(m_fd), data + offset, static_cast<DWORD>(length - offset), &written, nullptr
        )) {
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
      // LOG_INFO("CDC: readFrame magic=%04x len=%u size=%zu", magic, length, m_rxBuffer.size());
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
