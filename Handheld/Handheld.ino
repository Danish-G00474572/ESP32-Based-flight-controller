#include "StickInputs.h"
#include "OledDisplay.h"
#include "RadioLink.h"
#include "CalibrationWeb.h"
#include "StatusLed.h"

unsigned long lastInput = 0;
unsigned long lastOled = 0;

void setup() {
  startInputs();
  startDisplay();
  loadSettings();
  readInputs(millis());
  drawOled();

  startRadio();
  if (apReady) startCalibrationWeb();
  updateStatusLed();
}

void loop() {
  unsigned long now = millis();
  if (now - lastInput >= INPUT_MS) {
    lastInput = now;
    updateInputs(now);
    sendControls();
  }

  if (now - lastOled >= OLED_MS) {
    lastOled = now;
    drawOled();
    updateStatusLed();
  }

  if (apReady) server.handleClient();
  delay(1);
}
