#pragma once

#include <WiFi.h>
#include <WiFiUdp.h>
#include "ControlProtocol.h"
#include "StickInputs.h"

bool apReady = false;
WiFiUDP udp;
const IPAddress flightControllerIP(192, 168, 4, 200);

void startRadio() {
  WiFi.mode(WIFI_AP);
  apReady = WiFi.softAP(ControlLink::AP_NAME, ControlLink::AP_PASSWORD, 6);
}

void sendControls() {
  if (!apReady) return;

  ControlLink::Frame frame;
  frame.yaw = centredAxis(0);
  frame.throttle = throttleValue();
  frame.roll = centredAxis(2);
  frame.pitch = centredAxis(3);
  frame.calibrated = calibrationSaved && stage == Stage::Display;
  frame.s2Stable = s2Enabled;
  for (int index = 0; index < 4; index++) {
    if (switchOn[index]) frame.switches |= 1 << index;
  }

  uint8_t bytes[ControlLink::SIZE];
  ControlLink::encode(frame, bytes);
  if (udp.beginPacket(flightControllerIP, ControlLink::PORT)) {
    udp.write(bytes, sizeof(bytes));
    udp.endPacket();
  }
}
