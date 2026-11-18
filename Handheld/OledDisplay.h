#pragma once

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "StickInputs.h"

constexpr uint8_t OLED_SDA = 6;
constexpr uint8_t OLED_SCL = 7;
constexpr unsigned long OLED_MS = 50;
bool oledReady = false;
Adafruit_SSD1306 oled(128, 64, &Wire, -1);

void startDisplay() {
  Wire.begin(OLED_SDA, OLED_SCL);
  for (uint8_t address : {0x3C, 0x3D}) {
    Wire.beginTransmission(address);
    if (Wire.endTransmission() == 0) {
      oledReady = oled.begin(SSD1306_SWITCHCAPVCC, address, false, false);
      if (oledReady) break;
    }
  }
  if (oledReady) {
    oled.setTextSize(1);
    oled.setTextColor(SSD1306_WHITE);
  }
}

void drawStick(int left, const char *label, int x, int y) {
  constexpr int TOP = 12;
  constexpr int SIZE = 41;
  int centreX = left + SIZE / 2;
  int centreY = TOP + SIZE / 2;

  oled.setCursor(left, 1);
  oled.print(label);
  oled.drawRect(left, TOP, SIZE, SIZE, SSD1306_WHITE);
  for (int offset = 4; offset < SIZE - 1; offset += 4) {
    oled.drawPixel(left + offset, centreY, SSD1306_WHITE);
    oled.drawPixel(centreX, TOP + offset, SSD1306_WHITE);
  }
  oled.fillCircle(centreX + x, centreY - y, 2, SSD1306_WHITE);
}

void drawOled() {
  if (!oledReady) return;
  oled.clearDisplay();
  oled.setCursor(0, 4);

  switch (stage) {
    case Stage::Display:
      drawStick(9, "LEFT", centredAxis(0, 17), centredAxis(1, 17));
      drawStick(78, "RIGHT", centredAxis(2, 17), centredAxis(3, 17));
      break;
    case Stage::Limits:
      oled.println("Sweep both sticks");
      oled.println("Tap Record limits");
      break;
    case Stage::Centre:
      oled.println("Throttle halfway");
      oled.println("Tap Capture centre");
      break;
    case Stage::Sampling:
      oled.println("Hold sticks still");
      oled.println("Measuring centre...");
      break;
  }

  for (int index = 0; index < 4; index++) {
    oled.setCursor(index * 32, 56);
    oled.printf("S%d:%d", index + 1, switchOn[index] ? 1 : 0);
  }
  oled.display();
}
