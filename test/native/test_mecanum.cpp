#include <assert.h>
#include <stdint.h>
#include <robot/Mecanum.h>
#include <robot/Config.h>
#include <link/DriveProtocol.h>

static void testDriverPinMap() {
  assert(robot_config::MOTOR_COUNT == 4);
  assert(robot_config::MOTORS[0].inputA == 3 && robot_config::MOTORS[0].inputB == 5);
  assert(robot_config::MOTORS[0].mode == robot_config::DUAL_PWM);
  assert(robot_config::MOTORS[1].inputA == 6 && robot_config::MOTORS[1].inputB == 9);
  assert(robot_config::MOTORS[1].mode == robot_config::DUAL_PWM);
  assert(robot_config::MOTORS[2].inputA == 10 && robot_config::MOTORS[2].inputB == 4);
  assert(robot_config::MOTORS[2].mode == robot_config::PWM_DIR);
  assert(robot_config::MOTORS[3].inputA == 11 && robot_config::MOTORS[3].inputB == 7);
  assert(robot_config::MOTORS[3].mode == robot_config::PWM_DIR);
}

static void testMecanumAxes() {
  mecanum::WheelPowers wheels = mecanum::mix(100, 0, 0);
  assert(wheels.frontLeft == 100 && wheels.frontRight == 100);
  assert(wheels.rearLeft == 100 && wheels.rearRight == 100);

  wheels = mecanum::mix(0, 100, 0);
  assert(wheels.frontLeft == 100 && wheels.frontRight == -100);
  assert(wheels.rearLeft == -100 && wheels.rearRight == 100);

  wheels = mecanum::mix(0, 0, 100);
  assert(wheels.frontLeft == -100 && wheels.frontRight == 100);
  assert(wheels.rearLeft == -100 && wheels.rearRight == 100);

  wheels = mecanum::mix(-100, 0, 0);
  assert(wheels.frontLeft == -100 && wheels.frontRight == -100);
  assert(wheels.rearLeft == -100 && wheels.rearRight == -100);
}

static void testMecanumNormalization() {
  const mecanum::WheelPowers wheels = mecanum::mix(100, 100, -100);
  const int16_t output[] = {wheels.frontLeft, wheels.frontRight,
                            wheels.rearLeft, wheels.rearRight};
  for (uint8_t i = 0; i < 4; ++i) assert(output[i] >= -100 && output[i] <= 100);
  assert(wheels.frontLeft == 100);
  assert(wheels.frontRight == -33);
  assert(wheels.rearLeft == 33);
  assert(wheels.rearRight == 33);

  const mecanum::WheelPowers clipped = mecanum::mix(300, -300, 0);
  assert(clipped.frontLeft == 0 && clipped.frontRight == 100);
  assert(clipped.rearLeft == 100 && clipped.rearRight == 0);
}

static void testDriveFrames() {
  teleop_link::DriveCommand source{42, 70, -35, 18, teleop_link::FLAG_DEADMAN};
  uint8_t frame[teleop_link::DRIVE_SIZE];
  teleop_link::encodeDrive(source, frame);
  teleop_link::DriveCommand decoded{};
  assert(teleop_link::decodeDrive(frame, decoded));
  assert(decoded.sequence == source.sequence && decoded.forward == source.forward);
  assert(decoded.strafe == source.strafe && decoded.turn == source.turn);
  assert(decoded.flags == source.flags);
  frame[3] ^= 0x01;
  assert(!teleop_link::decodeDrive(frame, decoded));

  source.forward = 101;
  teleop_link::encodeDrive(source, frame);
  assert(!teleop_link::decodeDrive(frame, decoded));
}

static void testStatusFrames() {
  teleop_link::DriveStatus source{9, {-100, -10, 0, 100},
      static_cast<uint8_t>(teleop_link::STATUS_COMMAND_FRESH |
                           teleop_link::STATUS_DEADMAN_ACTIVE)};
  uint8_t frame[teleop_link::STATUS_SIZE];
  teleop_link::encodeStatus(source, frame);
  teleop_link::DriveStatus decoded{};
  assert(teleop_link::decodeStatus(frame, decoded));
  assert(decoded.sequence == 9 && decoded.wheel[0] == -100 && decoded.wheel[3] == 100);
  frame[6] |= 0x80;
  frame[7] = teleop_link::crc8(frame, teleop_link::STATUS_SIZE - 1);
  assert(!teleop_link::decodeStatus(frame, decoded));
}

int main() {
  testDriverPinMap();
  testMecanumAxes();
  testMecanumNormalization();
  testDriveFrames();
  testStatusFrames();
}
