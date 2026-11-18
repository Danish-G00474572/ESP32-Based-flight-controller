#include "EscOutput.h"
#include "RadioLink.h"
#include "OledDisplay.h"
#include "ServoOutputs.h"

constexpr unsigned long DISPLAY_MS = 100;
constexpr unsigned long REPORT_MS = 500;
unsigned long lastDisplay = 0;
unsigned long lastReport = 0;

void setup() {
  Serial.begin(115200);
  bool pwmReady = startEsc();
  startDisplay();

  if (!pwmReady) {
    Serial.println("ESC PWM setup failed.");
    drawDisplay("PWM SETUP FAILED", false, false, 0, receivedControls);
    while (true) delay(1000);
  }

  if (!ServoOutputs::begin()) Serial.println("Servo PWM setup failed.");
  drawDisplay("WAITING FOR LINK", false, false, outputUs, receivedControls);
  if (!startRadio()) {
    Serial.println("Static IP setup failed.");
    drawDisplay("IP SETUP FAILED", false, false, outputUs, receivedControls);
    while (true) delay(1000);
  }
}

void loop() {
  pollRadio();
  ServoOutputs::update(receivedControls);
  unsigned long now = millis();

  if (now - lastDisplay >= DISPLAY_MS) {
    lastDisplay = now;
    const char *status = "WAITING FOR LINK";
    if (linkActive) {
      status = receivedControls.calibrated ? "LINK OK" : "LINK OK / CAL NEEDED";
    }
    drawDisplay(status, linkActive, armed, outputUs, receivedControls);
  }

  if (now - lastReport >= REPORT_MS) {
    lastReport = now;
    const ControlLink::Frame &frame = receivedControls;
    Serial.printf("Link %s | %s | Y:%+d T:%d.%d%% R:%+d P:%+d | S1-4:%d%d%d%d | ESC:%d us\n",
                  linkActive ? "OK" : "WAIT", armed ? "ARMED" : "DISARMED",
                  frame.yaw, frame.throttle / 10, frame.throttle % 10,
                  frame.roll, frame.pitch, frame.switches & 1,
                  (frame.switches >> 1) & 1, (frame.switches >> 2) & 1,
                  (frame.switches >> 3) & 1, outputUs);
  }
  delay(1);
}
