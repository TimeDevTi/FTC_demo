#pragma once

#include <stdint.h>

namespace robot_config {
struct MotorPins {
  uint8_t pwm;
  uint8_t direction;
  bool inverted;
};

// R4 Minima: the dual-channel Cytron driver is provisionally FL/FR;
// the two single-channel drivers are provisionally RL/RR. Confirm the
// channel order and each driver's PWM/DIR mode against the actual labels.
constexpr MotorPins MOTORS[4] = {
    {3, 4, false},  // front-left: PWM D3, DIR D4
    {5, 7, false},  // front-right: PWM D5, DIR D7
    {6, 8, false},  // rear-left: PWM D6, DIR D8
    {9, 10, false}  // rear-right: PWM D9, DIR D10
};
constexpr uint8_t MOTOR_COUNT = 4;
constexpr uint32_t DRIVE_TIMEOUT_MS = 250;
constexpr uint32_t CONTROL_PERIOD_MS = 20;
constexpr uint32_t STATUS_PERIOD_MS = 100;
constexpr uint32_t UNO_Q_LINK_TIMEOUT_MS = 250;
}  // namespace robot_config
