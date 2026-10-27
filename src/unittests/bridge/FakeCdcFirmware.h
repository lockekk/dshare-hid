/*
 * dshare-hid -- created by locke.huang@gmail.com
 */

#pragma once

#include "platform/bridge/CdcTransport.h"

#include <QString>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <thread>
#include <vector>

#include <fcntl.h>
#include <poll.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

/**
 * @brief A bridge dongle for tests: a pseudo-terminal whose far end answers CdcTransport's
 * frames as the firmware does, while it lives (Unix only)
 *
 * It answers HELLO (unsigned, so open the transport with allowInsecure), the name and serial
 * reads, GET_PROFILE (with or without an index) and SWITCH_PROFILE.  Profile i is "Host i",
 * (1000 + 100 i) x (2000 + 100 i), portrait.
 */
class FakeCdcFirmware
{
public:
  explicit FakeCdcFirmware(int profileCount, int activeProfile = 0)
      : active(activeProfile),
        reportedCount(profileCount),
        m_profileCount(profileCount)
  {
    m_master = ::posix_openpt(O_RDWR | O_NOCTTY);
    if (m_master < 0 || ::grantpt(m_master) != 0 || ::unlockpt(m_master) != 0) {
      return;
    }
    m_devicePath = QString::fromLocal8Bit(::ptsname(m_master));

    // held open so the line stays up while the transport opens and closes its end
    m_slave = ::open(::ptsname(m_master), O_RDWR | O_NOCTTY);
    termios tty{};
    if (m_slave < 0 || ::tcgetattr(m_slave, &tty) != 0) {
      return;
    }
    ::cfmakeraw(&tty);
    ::tcsetattr(m_slave, TCSANOW, &tty);

    m_thread = std::thread([this] { run(); });
  }

  ~FakeCdcFirmware()
  {
    m_stop = true;
    if (m_thread.joinable()) {
      m_thread.join();
    }
    if (m_slave >= 0) {
      ::close(m_slave);
    }
    if (m_master >= 0) {
      ::close(m_master);
    }
  }

  FakeCdcFirmware(const FakeCdcFirmware &) = delete;
  FakeCdcFirmware &operator=(const FakeCdcFirmware &) = delete;

  /// empty if the pseudo-terminal couldn't be set up
  QString devicePath() const
  {
    return m_thread.joinable() ? m_devicePath : QString();
  }

  static deskflow::bridge::DeviceProfile profile(int index)
  {
    deskflow::bridge::DeviceProfile p;
    std::memset(&p, 0, sizeof(p));
    std::snprintf(p.hostname, sizeof(p.hostname), "Host %d", index);
    p.slot = static_cast<uint8_t>(index);
    p.screenWidth = static_cast<uint16_t>(1000 + 100 * index);
    p.screenHeight = static_cast<uint16_t>(2000 + 100 * index);
    p.speed = 30;
    return p;
  }

  /// the active profile; a switch sets it at once, as the firmware does before it replies
  std::atomic<int> active;
  /// the profile count HELLO reports
  std::atomic<int> reportedCount;
  /// how long after a switch its reply comes
  std::atomic<int> switchReplyDelayMs = 0;
  /// send a bare success ahead of the next profile reply, as a late reply to an earlier command
  std::atomic<bool> lateSuccess = false;
  /// answer profile reads with an error
  std::atomic<bool> failProfileReads = false;
  /// the esp_chip_id_t HELLO reports, ESP32-C3 by default
  std::atomic<int> chipId = 5;

private:
  static constexpr uint8_t kControl = 0x80;
  static constexpr uint8_t kHello = 0x01;
  static constexpr uint8_t kGetName = 0x02;
  static constexpr uint8_t kGetSerial = 0x04;
  static constexpr uint8_t kSwitchProfile = 0x31;
  static constexpr uint8_t kGetProfile = 0x32;
  static constexpr uint8_t kAck = 0x81;
  static constexpr uint8_t kConfigResponse = 0x82;

  void run()
  {
    std::vector<uint8_t> buffer;
    while (!m_stop) {
      pollfd pending{m_master, POLLIN, 0};
      if (::poll(&pending, 1, 10) <= 0) {
        continue;
      }
      uint8_t chunk[256];
      const ssize_t n = ::read(m_master, chunk, sizeof(chunk));
      if (n <= 0) {
        continue;
      }
      buffer.insert(buffer.end(), chunk, chunk + n);

      // [magic 0xC35A LE][version][type][flags][reserved][length LE][payload]
      while (buffer.size() >= 8) {
        if (buffer[0] != 0x5A || buffer[1] != 0xC3) {
          buffer.erase(buffer.begin());
          continue;
        }
        const size_t length = buffer[6] | (buffer[7] << 8);
        if (buffer.size() < 8 + length) {
          break;
        }
        const uint8_t type = buffer[3];
        const auto end = buffer.begin() + static_cast<std::ptrdiff_t>(8 + length);
        const std::vector<uint8_t> payload(buffer.begin() + 8, end);
        buffer.erase(buffer.begin(), end);
        if (type == kControl && !payload.empty()) {
          answer(payload);
        }
      }
    }
  }

  void answer(const std::vector<uint8_t> &command)
  {
    switch (command[0]) {
    case kHello: {
      // [ACK][core 16][device nonce 32][tag 64]; the core is read from the frame's byte 1 on
      std::vector<uint8_t> ack(1 + 16 + 32 + 64, 0);
      ack[0] = kAck;
      ack[1] = 1;   // protocol version
      ack[2] = 3;   // activated
      ack[3] = 121; // firmware 1.2.1
      ack[4] = 10;  // hardware 1.0
      ack[5] = 1;   // app mode
      ack[6] = 1;   // OTA_0 holds a valid image
      ack[7] = static_cast<uint8_t>((reportedCount << 4) | (active & 0x0F));
      ack[8] = 1; // Bluetooth connected
      ack[10] = static_cast<uint8_t>(chipId.load());
      send(ack);
      break;
    }
    case kGetName:
      sendConfig(kGetName, "Fake HID");
      break;
    case kGetSerial:
      sendConfig(kGetSerial, "Bridge-FAKE");
      break;
    case kGetProfile: {
      const int index = command.size() > 1 ? command[1] : active.load();
      if (lateSuccess.exchange(false)) {
        send({kAck, 0});
      }
      if (failProfileReads || index < 0 || index >= m_profileCount) {
        send({kAck, 1});
        break;
      }
      const auto p = profile(index);
      std::vector<uint8_t> reply{kAck, 0};
      reply.insert(
          reply.end(), reinterpret_cast<const uint8_t *>(&p), reinterpret_cast<const uint8_t *>(&p) + sizeof(p)
      );
      send(reply);
      break;
    }
    case kSwitchProfile:
      if (command.size() < 2 || command[1] >= m_profileCount) {
        send({kAck, 1});
        break;
      }
      active = command[1];
      std::this_thread::sleep_for(std::chrono::milliseconds(switchReplyDelayMs.load()));
      send({kAck, 0});
      break;
    default:
      break;
    }
  }

  void sendConfig(uint8_t request, const char *text)
  {
    std::vector<uint8_t> reply{kConfigResponse, request, 0};
    reply.insert(reply.end(), text, text + std::strlen(text));
    send(reply);
  }

  void send(const std::vector<uint8_t> &payload)
  {
    std::vector<uint8_t> frame{
        0x5A,
        0xC3,
        0x01,
        kControl,
        0,
        0,
        static_cast<uint8_t>(payload.size() & 0xFF),
        static_cast<uint8_t>(payload.size() >> 8)
    };
    frame.insert(frame.end(), payload.begin(), payload.end());
    size_t written = 0;
    while (written < frame.size()) {
      const ssize_t n = ::write(m_master, frame.data() + written, frame.size() - written);
      if (n <= 0) {
        return;
      }
      written += static_cast<size_t>(n);
    }
  }

  int m_profileCount;
  int m_master = -1;
  int m_slave = -1;
  QString m_devicePath;
  std::atomic<bool> m_stop = false;
  std::thread m_thread;
};
