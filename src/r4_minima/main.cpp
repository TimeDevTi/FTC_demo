#include <Arduino.h>
#include <stdint.h>
#include <robot/Config.h>
#include <robot/Mecanum.h>
#include <link/DriveProtocol.h>

namespace {
uint8_t rxFrame[teleop_link::DRIVE_SIZE];
uint8_t rxLength = 0;
uint32_t lastValidFrameAt = 0;
uint32_t lastStatusAt = 0;
uint32_t lastUsbLogAt = 0;
uint8_t lastSequence = 0;
bool haveCommand = false;
bool deadman = false;
int8_t requested[4] = {0, 0, 0, 0};
int8_t applied[4] = {0, 0, 0, 0};

void writePwm(uint8_t pin, uint8_t duty) {
  analogWrite(pin, duty);
}

void applyMotor(uint8_t index, int8_t power) {
  const robot_config::MotorPins &motor = robot_config::MOTORS[index];
  int16_t signedPower = power;
  if (motor.inverted) signedPower = -signedPower;
  const uint8_t magnitude = static_cast<uint8_t>(signedPower < 0 ? -signedPower : signedPower);
  const uint8_t duty = static_cast<uint8_t>((static_cast<uint16_t>(magnitude) * 255u) / 100u);

  if (motor.mode == robot_config::DUAL_PWM) {
    // MDD3A PWM_PWM: drive only one input for a direction; zero holds both low.
    writePwm(motor.inputA, signedPower > 0 ? duty : 0);
    writePwm(motor.inputB, signedPower < 0 ? duty : 0);
    return;
  }

  // MD10C R3 sign-magnitude mode: PWM controls speed; DIR controls direction.
  digitalWrite(motor.inputB, signedPower >= 0 ? HIGH : LOW);
  writePwm(motor.inputA, duty);
}

void stopDrive() {
  deadman = false;
  for (uint8_t i = 0; i < robot_config::MOTOR_COUNT; ++i) {
    requested[i] = 0;
    applied[i] = 0;
    applyMotor(i, 0);
  }
}

void acceptFrame(const uint8_t (&frame)[teleop_link::DRIVE_SIZE]) {
  teleop_link::DriveCommand command{};
  if (!teleop_link::decodeDrive(frame, command)) return;
  const uint32_t now = millis();
  haveCommand = true;
  lastSequence = command.sequence;
  lastValidFrameAt = now;
  deadman = (command.flags & teleop_link::FLAG_DEADMAN) != 0;
  const mecanum::WheelPowers wheels = mecanum::mix(command.forward, command.strafe, command.turn);
  requested[0] = wheels.frontLeft;
  requested[1] = wheels.frontRight;
  requested[2] = wheels.rearLeft;
  requested[3] = wheels.rearRight;
}

void receiveFrames() {
  while (Serial1.available() > 0) {
    const uint8_t value = static_cast<uint8_t>(Serial1.read());
    if (rxLength == 0) {
      if (value == teleop_link::DRIVE_MAGIC) rxFrame[rxLength++] = value;
      continue;
    }
    rxFrame[rxLength++] = value;
    if (rxLength == teleop_link::DRIVE_SIZE) {
      acceptFrame(rxFrame);
      rxLength = 0;
    }
  }
}

bool applyFailsafes() {
  const uint32_t now = millis();
  const bool linkFresh = haveCommand && now - lastValidFrameAt <= robot_config::UNO_Q_LINK_TIMEOUT_MS;
  const bool safeToMove = linkFresh && deadman &&
      now - lastValidFrameAt <= robot_config::DRIVE_TIMEOUT_MS;
  if (!safeToMove) {
    stopDrive();
    return linkFresh;
  }
  for (uint8_t i = 0; i < robot_config::MOTOR_COUNT; ++i) {
    applied[i] = requested[i];
    applyMotor(i, applied[i]);
  }
  return true;
}

void sendStatus(bool commandFresh) {
  teleop_link::DriveStatus status{};
  status.sequence = lastSequence;
  for (uint8_t i = 0; i < robot_config::MOTOR_COUNT; ++i) status.wheel[i] = applied[i];
  if (commandFresh) status.flags |= teleop_link::STATUS_COMMAND_FRESH;
  if (deadman && commandFresh) status.flags |= teleop_link::STATUS_DEADMAN_ACTIVE;
  uint8_t frame[teleop_link::STATUS_SIZE];
  teleop_link::encodeStatus(status, frame);
  Serial1.write(frame, sizeof(frame));
}

void printStatus(bool commandFresh) {
  if (millis() - lastUsbLogAt < 1000) return;
  lastUsbLogAt = millis();
  Serial.print(F("link=")); Serial.print(commandFresh ? F("up") : F("timeout"));
  Serial.print(F(" deadman=")); Serial.print(deadman ? F("on") : F("off"));
  Serial.print(F(" FL/FR/RL/RR="));
  for (uint8_t i = 0; i < robot_config::MOTOR_COUNT; ++i) {
    if (i) Serial.print('/');
    Serial.print(applied[i]);
  }
  Serial.println();
}
}  // namespace

void setup() {
  // Force every driver input low before opening the serial link or USB console.
  for (uint8_t i = 0; i < robot_config::MOTOR_COUNT; ++i) {
    const robot_config::MotorPins &motor = robot_config::MOTORS[i];
    pinMode(motor.inputA, OUTPUT);
    pinMode(motor.inputB, OUTPUT);
    digitalWrite(motor.inputA, LOW);
    digitalWrite(motor.inputB, LOW);
  }
  analogWriteResolution(8);  // match the 0-255 duty conversion in applyMotor().
  Serial.begin(115200);
  Serial1.begin(teleop_link::BAUD);
  Serial.println(F("Mecanum teleop R4 ready; waiting for UNO Q"));
}

void loop() {
  receiveFrames();
  const bool commandFresh = applyFailsafes();

  const uint32_t now = millis();
  if (now - lastStatusAt >= robot_config::STATUS_PERIOD_MS) {
    lastStatusAt = now;
    sendStatus(commandFresh);
    printStatus(commandFresh);
  }
  delay(robot_config::CONTROL_PERIOD_MS);
}
