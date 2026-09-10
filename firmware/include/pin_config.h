#pragma once

#include <cstdint>

#include "driver/gpio.h"

namespace config {

constexpr gpio_num_t kMotorAin1Pin = GPIO_NUM_1;
constexpr gpio_num_t kMotorAin2Pin = GPIO_NUM_2;
constexpr gpio_num_t kSteeringPin = GPIO_NUM_3;
constexpr gpio_num_t kMotorSleepPin = GPIO_NUM_4;

constexpr std::uint32_t kCommandTimeoutMs = 500;
constexpr int kMotorMinimumPwm = 180;
constexpr std::uint32_t kMotorPwmFrequencyHz = 1000;
constexpr int kMotorPwmResolutionBits = 8;

static_assert(kMotorMinimumPwm > 0 && kMotorMinimumPwm <= 255,
              "Motor minimum PWM must be in range 1..255");

// Verified on the current servo and steering mechanism.
// TODO / VERIFY ON HARDWARE: swap min/max mapping if web left/right is reversed.
constexpr int kSteeringMinDegrees = 10;
constexpr int kSteeringCenterDegrees = 90;
constexpr int kSteeringMaxDegrees = 170;
constexpr int kSteeringMaxCommand = 80;
constexpr std::uint32_t kSteeringFrequencyHz = 50;
constexpr int kSteeringPwmResolutionBits = 10;
constexpr int kSteeringMinimumPulseUs = 544;
constexpr int kSteeringMaximumPulseUs = 2400;

static_assert(kSteeringMinDegrees < kSteeringCenterDegrees &&
                  kSteeringCenterDegrees < kSteeringMaxDegrees,
              "Steering calibration must be ordered min < center < max");
static_assert(kSteeringMaxCommand > 0 && kSteeringMaxCommand <= 100,
              "Steering limit must be in range 1..100");

} // namespace config
