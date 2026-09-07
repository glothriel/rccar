#pragma once

#include <Arduino.h>

namespace config {

// TODO / VERIFY ON HARDWARE against the exact XIAO and camera expansion.
constexpr uint8_t kMotorAin1Pin = D0;  // GPIO1
constexpr uint8_t kMotorAin2Pin = D1;  // GPIO2
constexpr uint8_t kSteeringPin = D2;   // GPIO3
constexpr uint8_t kMotorSleepPin = D3; // GPIO4

constexpr unsigned long kSerialBaud = 115200;
constexpr unsigned long kCommandTimeoutMs = 500;
constexpr int kMotorMinimumPwm = 180;

static_assert(kMotorMinimumPwm > 0 && kMotorMinimumPwm <= 255,
              "Motor minimum PWM must be in range 1..255");

// Verified on the current servo and steering mechanism.
// TODO / VERIFY ON HARDWARE: swap min/max mapping if web left/right is reversed.
constexpr int kSteeringMinDegrees = 10;
constexpr int kSteeringCenterDegrees = 90;
constexpr int kSteeringMaxDegrees = 170;
constexpr int kSteeringMaxCommand = 80;

static_assert(kSteeringMinDegrees < kSteeringCenterDegrees &&
                  kSteeringCenterDegrees < kSteeringMaxDegrees,
              "Steering calibration must be ordered min < center < max");
static_assert(kSteeringMaxCommand > 0 && kSteeringMaxCommand <= 100,
              "Steering limit must be in range 1..100");

} // namespace config
