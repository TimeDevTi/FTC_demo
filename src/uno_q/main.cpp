#include <Arduino.h>
#include <Arduino_RouterBridge.h>
#include <stdint.h>
#include <stdio.h>
#include <link/DriveProtocol.h>
#include <robot/Config.h>

namespace {
teleop_link::DriveCommand latestCommand{0, 0, 0, 0, 0};
teleop_link::DriveStatus latestStatus{0, {0, 0, 0, 0}, 0};
uint8_t rxFrame[teleop_link::STATUS_SIZE];
uint8_t rxLength = 0;
uint8_t nextSequence = 1;
uint32_t lastWebCommandAt = 0;
uint32_t lastR4StatusAt = 0;
uint32_t lastDriveTransmitAt = 0;
uint32_t lastLogAt = 0;
bool haveR4Status = false;

int16_t clampAxis(int value) {
  if (value > 100) return 100;
  if (value < -100) return -100;
  return static_cast<int16_t>(value);
}

// Called through Bridge's safe/main-loop callback so state and Serial1 access
// stay in the UNO Q sketch loop context.
bool setDrive(int forward, int strafeRight, int clockwise, bool requestedDeadman) {
  latestCommand.sequence = nextSequence++;
  latestCommand.forward = static_cast<int8_t>(clampAxis(forward));
  latestCommand.strafe = static_cast<int8_t>(clampAxis(strafeRight));
  latestCommand.turn = static_cast<int8_t>(clampAxis(clockwise));
  latestCommand.flags = requestedDeadman ? teleop_link::FLAG_DEADMAN : 0;
  lastWebCommandAt = millis();
  return true;
}

String getDriveStatus() {
  const uint32_t now = millis();
  const bool fresh = haveR4Status && now - lastR4StatusAt <= robot_config::UNO_Q_LINK_TIMEOUT_MS;
  const bool deadman = fresh && (latestStatus.flags & teleop_link::STATUS_DEADMAN_ACTIVE);
  char json[192];
  snprintf(json, sizeof(json),
      "{\"linkConnected\":%s,\"deadman\":%s,\"ageMs\":%lu,\"sequence\":%u,\"wheelPwm\":[%d,%d,%d,%d]}",
      fresh ? "true" : "false", deadman ? "true" : "false",
      static_cast<unsigned long>(now - lastR4StatusAt),
      latestStatus.sequence, latestStatus.wheel[0], latestStatus.wheel[1],
      latestStatus.wheel[2], latestStatus.wheel[3]);
  return String(json);
}

void receiveR4Status() {
  while (Serial1.available() > 0) {
    const uint8_t value = static_cast<uint8_t>(Serial1.read());
    if (rxLength == 0) {
      if (value == teleop_link::STATUS_MAGIC) rxFrame[rxLength++] = value;
      continue;
    }
    rxFrame[rxLength++] = value;
    if (rxLength == teleop_link::STATUS_SIZE) {
      teleop_link::DriveStatus status{};
      if (teleop_link::decodeStatus(rxFrame, status)) {
        latestStatus = status;
        lastR4StatusAt = millis();
        haveR4Status = true;
      }
      rxLength = 0;
    }
  }
}

void transmitCommand() {
  const uint32_t now = millis();
  const bool browserFresh = now - lastWebCommandAt <= robot_config::DRIVE_TIMEOUT_MS;
  teleop_link::DriveCommand command = latestCommand;
  if (!browserFresh) {
    command.forward = command.strafe = command.turn = 0;
    command.flags = 0;
  }
  if (now - lastDriveTransmitAt < robot_config::CONTROL_PERIOD_MS) return;
  lastDriveTransmitAt = now;
  uint8_t frame[teleop_link::DRIVE_SIZE];
  teleop_link::encodeDrive(command, frame);
  Serial1.write(frame, sizeof(frame));
}

void logLink() {
  if (millis() - lastLogAt < 1000) return;
  lastLogAt = millis();
  Serial.print(F("R4 link="));
  Serial.print(haveR4Status && millis() - lastR4StatusAt <= robot_config::UNO_Q_LINK_TIMEOUT_MS
                   ? F("up") : F("timeout"));
  Serial.print(F("; Web drive lease="));
  Serial.println(millis() - lastWebCommandAt <= robot_config::DRIVE_TIMEOUT_MS
                     ? F("active") : F("expired"));
}
}  // namespace

void setup() {
  Serial.begin(115200);  // RouterBridge monitor on UNO Q, not the D0/D1 UART.
  Serial1.begin(teleop_link::BAUD);  // Physical D0/RX, D1/TX to R4 Serial1.
  Bridge.begin();
  Bridge.provide_safe("set_drive", setDrive);
  Bridge.provide_safe("get_drive_status", getDriveStatus);
  Serial.println(F("UNO Q teleop bridge ready"));
}

void loop() {
  receiveR4Status();
  transmitCommand();
  logLink();
  delay(1);
}
