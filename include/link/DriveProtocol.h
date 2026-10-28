#pragma once

#include <stdint.h>

namespace teleop_link {
constexpr uint32_t BAUD = 115200;
constexpr uint8_t DRIVE_MAGIC = 0xD3;
constexpr uint8_t STATUS_MAGIC = 0x3D;
constexpr uint8_t DRIVE_SIZE = 7;
constexpr uint8_t STATUS_SIZE = 8;
constexpr uint8_t FLAG_DEADMAN = 0x01;
constexpr uint8_t STATUS_COMMAND_FRESH = 0x01;
constexpr uint8_t STATUS_DEADMAN_ACTIVE = 0x02;

struct DriveCommand {
  uint8_t sequence;
  int8_t forward;
  int8_t strafe;
  int8_t turn;
  uint8_t flags;
};

struct DriveStatus {
  uint8_t sequence;
  int8_t wheel[4];  // FL, FR, RL, RR; signed PWM percent actually sent
  uint8_t flags;
};

inline uint8_t crc8(const uint8_t *data, uint8_t length) {
  uint8_t crc = 0;
  for (uint8_t i = 0; i < length; ++i) {
    crc ^= data[i];
    for (uint8_t bit = 0; bit < 8; ++bit)
      crc = (crc & 0x80u) ? static_cast<uint8_t>((crc << 1u) ^ 0x07u)
                          : static_cast<uint8_t>(crc << 1u);
  }
  return crc;
}

inline void encodeDrive(const DriveCommand &command, uint8_t (&frame)[DRIVE_SIZE]) {
  frame[0] = DRIVE_MAGIC;
  frame[1] = command.sequence;
  frame[2] = static_cast<uint8_t>(command.forward);
  frame[3] = static_cast<uint8_t>(command.strafe);
  frame[4] = static_cast<uint8_t>(command.turn);
  frame[5] = command.flags;
  frame[6] = crc8(frame, DRIVE_SIZE - 1);
}

inline bool decodeDrive(const uint8_t (&frame)[DRIVE_SIZE], DriveCommand &command) {
  if (frame[0] != DRIVE_MAGIC || frame[6] != crc8(frame, DRIVE_SIZE - 1)) return false;
  command.sequence = frame[1];
  command.forward = static_cast<int8_t>(frame[2]);
  command.strafe = static_cast<int8_t>(frame[3]);
  command.turn = static_cast<int8_t>(frame[4]);
  command.flags = frame[5];
  return command.forward >= -100 && command.forward <= 100 &&
         command.strafe >= -100 && command.strafe <= 100 &&
         command.turn >= -100 && command.turn <= 100 &&
         (command.flags & static_cast<uint8_t>(~FLAG_DEADMAN)) == 0;
}

inline void encodeStatus(const DriveStatus &status, uint8_t (&frame)[STATUS_SIZE]) {
  frame[0] = STATUS_MAGIC;
  frame[1] = status.sequence;
  for (uint8_t i = 0; i < 4; ++i) frame[2 + i] = static_cast<uint8_t>(status.wheel[i]);
  frame[6] = status.flags;
  frame[7] = crc8(frame, STATUS_SIZE - 1);
}

inline bool decodeStatus(const uint8_t (&frame)[STATUS_SIZE], DriveStatus &status) {
  if (frame[0] != STATUS_MAGIC || frame[7] != crc8(frame, STATUS_SIZE - 1)) return false;
  status.sequence = frame[1];
  for (uint8_t i = 0; i < 4; ++i) {
    status.wheel[i] = static_cast<int8_t>(frame[2 + i]);
    if (status.wheel[i] < -100 || status.wheel[i] > 100) return false;
  }
  status.flags = frame[6];
  return (status.flags & static_cast<uint8_t>(~(STATUS_COMMAND_FRESH | STATUS_DEADMAN_ACTIVE))) == 0;
}
}  // namespace teleop_link
