#pragma once

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "ControlProtocol.h"

constexpr uint8_t OLED_SDA = 21;
constexpr uint8_t OLED_SCL = 22;
Adafruit_SSD1306 oled(128, 64, &Wire, -1);
bool oledReady = false;

void startDisplay() {
  Wire.begin(OLED_SDA, OLED_SCL);
  Wire.setTimeOut(10);
  const uint8_t addresses[] = {0x3C, 0x3D};
  for (uint8_t address : addresses) {
    Wire.beginTransmission(address);
    if (Wire.endTransmission() != 0) continue;
    if (!oled.begin(SSD1306_SWITCHCAPVCC, address, false, false)) continue;

    oledReady = true;
    oled.setTextColor(SSD1306_WHITE);
    oled.setTextSize(1);
    Serial.printf("OLED found at 0x%02X\n", unsigned(address));
    return;
  }
  Serial.println("OLED missing. Check SDA 21, SCL 22, 3V3 and GND.");
}

void drawDisplay(const char *status, bool linked, bool escArmed, int pulseUs,
                 const ControlLink::Frame &frame) {
  if (!oledReady) return;

  oled.clearDisplay();
  oled.setCursor(0, 0);
  oled.println(status);
  oled.drawFastHLine(0, 10, 128, SSD1306_WHITE);
  oled.setCursor(0, 14);
  oled.printf("%s  %d us\n", escArmed ? "ARMED" : "DISARMED", pulseUs);

  if (linked) {
    oled.setCursor(0, 26);
    oled.printf("Y:%+5d  T:%3d%%", frame.yaw, frame.throttle / 10);
    oled.setCursor(0, 37);
    oled.printf("R:%+5d  P:%+5d", frame.roll, frame.pitch);
    for (int index = 0; index < 4; index++) {
      oled.setCursor(index * 32, 54);
      oled.printf("S%d:%d", index + 1, (frame.switches >> index) & 1);
    }
  }
  oled.display();
}
