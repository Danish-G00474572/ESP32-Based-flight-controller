#pragma once

#include <WiFi.h>
#include <WiFiUdp.h>
#include "ControlProtocol.h"
#include "EscOutput.h"

constexpr unsigned long LINK_TIMEOUT_MS = 250;
const IPAddress transmitterIP(192, 168, 4, 1);
const IPAddress localIP(192, 168, 4, 200);
const IPAddress subnet(255, 255, 255, 0);

WiFiUDP udp;
ControlLink::Frame receivedControls;
bool udpReady = false;
bool linkActive = false;
unsigned long lastPacket = 0;
unsigned long lastBindAttempt = 0;

bool startRadio() {
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.setSleep(false);
  if (!WiFi.config(localIP, transmitterIP, subnet)) return false;
  WiFi.begin(ControlLink::AP_NAME, ControlLink::AP_PASSWORD);
  return true;
}

void clearLink() {
  linkActive = false;
  receivedControls = {};
  disarmEsc();
}

void pollRadio() {
  unsigned long now = millis();
  if (linkActive && now - lastPacket >= LINK_TIMEOUT_MS) clearLink();

  if (WiFi.status() != WL_CONNECTED) {
    if (udpReady) udp.stop();
    udpReady = false;
    if (linkActive) clearLink();
    return;
  }

  if (!udpReady && now - lastBindAttempt >= 500) {
    lastBindAttempt = now;
    udpReady = udp.begin(ControlLink::PORT);
  }
  if (!udpReady) return;

  // Bound the number of datagrams handled per pass.
  for (int count = 0; count < 8; count++) {
    int packetSize = udp.parsePacket();
    if (packetSize <= 0) return;

    if (packetSize != ControlLink::SIZE || udp.remoteIP() != transmitterIP) {
      udp.clear();
      continue;
    }

    uint8_t bytes[ControlLink::SIZE];
    ControlLink::Frame frame;
    if (udp.read(bytes, sizeof(bytes)) != int(sizeof(bytes)) ||
        !ControlLink::decode(bytes, frame)) continue;

    now = millis();
    if (linkActive && now - lastPacket >= LINK_TIMEOUT_MS) clearLink();
    lastPacket = now;
    linkActive = true;
    receivedControls = frame;
    applyThrottle(frame, now);
  }
}
