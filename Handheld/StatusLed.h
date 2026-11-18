#pragma once

#include "RadioLink.h"

void updateStatusLed() {
  uint8_t red = 0;
  uint8_t green = 0;
  uint8_t blue = 0;

  switch (stage) {
    case Stage::Limits:
      red = 24;
      green = 10;
      break;
    case Stage::Centre:
      red = 18;
      blue = 24;
      break;
    case Stage::Sampling:
      if ((millis() / 125) % 2 == 0) red = green = blue = 18;
      break;
    case Stage::Display:
      if (!apReady || !calibrationSaved) {
        red = 16;
      } else if (s2Enabled) {
        blue = 22;
      } else {
        green = 14;
      }
      break;
  }

  static uint32_t previous = 0xFFFFFFFF;
  uint32_t colour = (uint32_t(red) << 16) | (uint32_t(green) << 8) | blue;
  if (colour == previous) return;
  previous = colour;

#ifdef RGB_BUILTIN
  rgbLedWrite(RGB_BUILTIN, red, green, blue);
#else
  rgbLedWrite(8, red, green, blue);
#endif
}
