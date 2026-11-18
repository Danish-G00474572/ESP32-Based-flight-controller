#pragma once

#include <Arduino.h>

namespace ControlLink {
constexpr char AP_NAME[] = "Stick-Cal";
constexpr char AP_PASSWORD[] = "sticksetup";
constexpr uint16_t PORT = 4210;
constexpr uint8_t VERSION = 2;
constexpr uint8_t SIZE = 13;
constexpr uint8_t SWITCH_MASK = 0x0F;
constexpr uint8_t S2_MASK = 1 << 1;
constexpr uint8_t CALIBRATED = 1 << 0;
constexpr uint8_t S2_STABLE = 1 << 1;

struct Frame {
  int yaw = 0;
  int throttle = 0;
  int roll = 0;
  int pitch = 0;
  uint8_t switches = 0;
  bool calibrated = false;
  bool s2Stable = false;
};

// Axis wire order is yaw, throttle, roll, pitch. Each value is big endian.
// Centred channels use a +1000 bias on the wire. Throttle is unsigned.
inline void encode(const Frame &frame, uint8_t (&bytes)[SIZE]) {
  bytes[0] = 'S';
  bytes[1] = 'T';
  bytes[2] = VERSION;
  bytes[3] = frame.switches;
  bytes[4] = (frame.calibrated ? CALIBRATED : 0) |
             (frame.s2Stable ? S2_STABLE : 0);

  const int values[] = {frame.yaw + 1000, frame.throttle,
                        frame.roll + 1000, frame.pitch + 1000};
  for (int index = 0; index < 4; index++) {
    bytes[5 + index * 2] = uint8_t(values[index] >> 8);
    bytes[6 + index * 2] = uint8_t(values[index] & 0xFF);
  }
}

inline bool decode(const uint8_t (&bytes)[SIZE], Frame &frame) {
  if (bytes[0] != 'S' || bytes[1] != 'T' || bytes[2] != VERSION ||
      (bytes[3] & ~SWITCH_MASK) || (bytes[4] & ~(CALIBRATED | S2_STABLE))) {
    return false;
  }
  if ((bytes[4] & S2_STABLE) && !(bytes[3] & S2_MASK)) return false;

  int values[4];
  for (int index = 0; index < 4; index++) {
    values[index] = (int(bytes[5 + index * 2]) << 8) | bytes[6 + index * 2];
    if (values[index] > (index == 1 ? 1000 : 2000)) return false;
  }

  frame.yaw = values[0] - 1000;
  frame.throttle = values[1];
  frame.roll = values[2] - 1000;
  frame.pitch = values[3] - 1000;
  frame.switches = bytes[3];
  frame.calibrated = (bytes[4] & CALIBRATED) != 0;
  frame.s2Stable = (bytes[4] & S2_STABLE) != 0;
  return true;
}
}
