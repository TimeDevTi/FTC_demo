#pragma once

#include <stdint.h>

namespace robot_config {
enum DriverMode : uint8_t {
  PWM_DIR,   // single-channel drivers such as MD10C R3
  DUAL_PWM   // dual-channel drivers such as MDD3A (two PWM inputs per motor)
};

struct MotorPins {
  uint8_t inputA;
  uint8_t inputB;
  DriverMode mode;
  bool inverted;
};

// Proposed from the photo: MDD3A channels drive the front wheels; the two
// MD10C R3 boards drive the rear wheels. MDD3A uses PWM_PWM (A/B inputs).
// MD10C uses sign-magnitude PWM/DIR. Confirm motor/channel order before use.
constexpr MotorPins MOTORS[4] = {
    {3, 5, DUAL_PWM, false},  // FL: MDD3A M1A/M1B on PWM D3/D5
    {6, 9, DUAL_PWM, false},  // FR: MDD3A M2A/M2B on PWM D6/D9
    {10, 4, PWM_DIR, false},  // RL: MD10C PWM D10, DIR D4
    {11, 7, PWM_DIR, false}   // RR: MD10C PWM D11, DIR D7
};
constexpr uint8_t MOTOR_COUNT = 4;
constexpr uint32_t DRIVE_TIMEOUT_MS = 250;
constexpr uint32_t CONTROL_PERIOD_MS = 20;
constexpr uint32_t STATUS_PERIOD_MS = 100;
constexpr uint32_t UNO_Q_LINK_TIMEOUT_MS = 250;
}  // namespace robot_config
