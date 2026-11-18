#pragma once

#include <Arduino.h>
#include "ControlProtocol.h"

constexpr uint8_t ESC_PIN = 25;
constexpr int ESC_MIN_US = 1000;
constexpr int ESC_MAX_US = 2000;
constexpr uint32_t PWM_HZ = 50;
constexpr uint8_t PWM_BITS = 16;
constexpr uint32_t PWM_PERIOD_US = 1000000UL / PWM_HZ;
constexpr unsigned long ARM_HOLD_MS = 500;
constexpr int IDLE_LIMIT = 20;

bool armed = false;
bool idleTiming = false;
bool armPermitted = false;
unsigned long idleSince = 0;
int outputUs = ESC_MIN_US;

void setEscPulse(int pulseUs) {
  outputUs = pulseUs;
  uint32_t duty = (uint32_t(pulseUs) * (1UL << PWM_BITS) +
                   PWM_PERIOD_US / 2) / PWM_PERIOD_US;
  ledcWrite(ESC_PIN, duty);
}

bool startEsc() {
  pinMode(ESC_PIN, OUTPUT);
  digitalWrite(ESC_PIN, LOW);
  if (!ledcAttach(ESC_PIN, PWM_HZ, PWM_BITS)) return false;
  setEscPulse(ESC_MIN_US);
  return true;
}

void disarmEsc() {
  armed = false;
  idleTiming = false;
  armPermitted = false;
  setEscPulse(ESC_MIN_US);
}

void applyThrottle(const ControlLink::Frame &frame, unsigned long now) {
  if (!frame.calibrated) {
    disarmEsc();
    return;
  }

  const bool s2On = (frame.switches & ControlLink::S2_MASK) != 0;
  const bool atIdle = frame.throttle <= IDLE_LIMIT;
  if (!s2On) {
    armed = false;
    setEscPulse(ESC_MIN_US);
    if (atIdle && !idleTiming) idleSince = now;
    idleTiming = atIdle;
    armPermitted = atIdle && now - idleSince >= ARM_HOLD_MS;
    return;
  }

  // An unstable ON cannot start, or count towards, the S2-off idle hold.
  if (!frame.s2Stable) {
    armed = false;
    setEscPulse(ESC_MIN_US);
    idleTiming = false;
    if (!atIdle) armPermitted = false;
    return;
  }

  if (!armed) {
    armed = atIdle && armPermitted;
    idleTiming = false;
    armPermitted = false;
  }

  int pulseUs = ESC_MIN_US;
  if (armed && !atIdle) {
    pulseUs += frame.throttle * (ESC_MAX_US - ESC_MIN_US) / 1000;
  }
  setEscPulse(pulseUs);
}
