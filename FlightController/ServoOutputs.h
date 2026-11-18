#pragma once

#include <Arduino.h>
#include "ControlProtocol.h"

namespace ServoOutputs {
// Enable after checking the pin assignments and servo directions on the bench.
constexpr bool ENABLED = false;
constexpr uint8_t PINS[] = {13, 14, 32};  // Roll, pitch, yaw.
constexpr bool REVERSED[] = {false, false, false};
constexpr int CENTRE_US = 1500;
constexpr int TRAVEL_US = 400;
constexpr uint8_t PWM_BITS = 16;
constexpr uint32_t PWM_HZ = 50;
constexpr uint32_t PERIOD_US = 1000000UL / PWM_HZ;

bool ready = false;

void setPulse(uint8_t pin, int pulseUs) {
  uint32_t duty = (uint32_t(pulseUs) * (1UL << PWM_BITS) + PERIOD_US / 2)
                  / PERIOD_US;
  ledcWrite(pin, duty);
}

bool begin() {
  if (!ENABLED) return true;
  for (uint8_t index = 0; index < 3; index++) {
    if (!ledcAttach(PINS[index], PWM_HZ, PWM_BITS)) {
      for (uint8_t previous = 0; previous < index; previous++) {
        ledcDetach(PINS[previous]);
        pinMode(PINS[previous], OUTPUT);
        digitalWrite(PINS[previous], LOW);
      }
      return false;
    }
    setPulse(PINS[index], CENTRE_US);
  }
  ready = true;
  return true;
}

void update(const ControlLink::Frame &frame) {
  if (!ready) return;
  // Each output is an independent channel. The default mapping is for a
  // conventional aircraft with separate roll, pitch and yaw surfaces.
  const int axes[] = {frame.roll, frame.pitch, frame.yaw};
  for (uint8_t index = 0; index < 3; index++) {
    int offsetUs = frame.calibrated ? axes[index] * TRAVEL_US / 1000 : 0;
    if (REVERSED[index]) offsetUs = -offsetUs;
    setPulse(PINS[index], CENTRE_US + offsetUs);
  }
}
}
