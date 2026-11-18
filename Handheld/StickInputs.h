#pragma once

#include <Arduino.h>
#include <Preferences.h>

constexpr uint8_t AXIS_PINS[] = {2, 3, 0, 1};
constexpr bool AXIS_REVERSED[] = {false, true, true, false};
constexpr uint8_t SWITCH_PINS[] = {21, 20, 19, 18};

constexpr int AXIS_COUNT = 4;
constexpr int THROTTLE_AXIS = 1;
constexpr int S2_INDEX = 1;
constexpr int ADC_MAX = 4095;
constexpr int MIN_SIDE_TRAVEL = 60;
constexpr unsigned long INPUT_MS = 20;
constexpr unsigned long CENTRE_MS = 1000;
constexpr unsigned long SWITCH_ON_MS = 40;

struct AxisCalibration {
  uint16_t minimum;
  uint16_t centre;
  uint16_t maximum;
};

enum class Stage { Display, Limits, Centre, Sampling };

AxisCalibration active[AXIS_COUNT] = {
  {0, 2048, ADC_MAX}, {0, 2048, ADC_MAX},
  {0, 2048, ADC_MAX}, {0, 2048, ADC_MAX}
};
AxisCalibration pending[AXIS_COUNT];

int readings[AXIS_COUNT] = {};
uint32_t centreSums[AXIS_COUNT] = {};
int centreSamples = 0;
bool switchOn[4] = {};
bool s2Timing = false;
bool s2Enabled = false;
unsigned long s2OnSince = 0;

Stage stage = Stage::Display;
int deadzone = 3;
int savedDeadzone = 3;
bool storageReady = false;
bool calibrationSaved = false;
unsigned long centreStart = 0;
String statusMessage = "Using default stick values. Calibrate to enable throttle.";

Preferences storage;

void startInputs() {
  analogReadResolution(12);
  for (uint8_t pin : AXIS_PINS) analogSetPinAttenuation(pin, ADC_11db);
  for (uint8_t pin : SWITCH_PINS) pinMode(pin, INPUT_PULLUP);
}

bool validAxis(const AxisCalibration &axis) {
  return axis.maximum <= ADC_MAX &&
         int(axis.centre) - int(axis.minimum) >= MIN_SIDE_TRAVEL &&
         int(axis.maximum) - int(axis.centre) >= MIN_SIDE_TRAVEL;
}

void loadSettings() {
  storageReady = storage.begin("stick-cal", false);
  if (!storageReady) {
    statusMessage = "Flash unavailable. Throttle is disabled.";
    return;
  }

  uint8_t storedZone = storage.getUChar("deadzone", 3);
  if (storedZone <= 20) deadzone = savedDeadzone = storedZone;

  AxisCalibration stored[AXIS_COUNT];
  if (storage.getBytesLength("axes-v1") != sizeof(stored) ||
      storage.getBytes("axes-v1", stored, sizeof(stored)) != sizeof(stored)) {
    return;
  }

  for (const AxisCalibration &axis : stored) {
    if (!validAxis(axis)) return;
  }

  memcpy(active, stored, sizeof(active));
  calibrationSaved = true;
  statusMessage = "Saved calibration loaded. Check throttle direction with S2 off.";
}

void readInputs(unsigned long now) {
  for (int axis = 0; axis < AXIS_COUNT; axis++) {
    int sum = 0;
    for (int sample = 0; sample < 8; sample++) {
      sum += analogRead(AXIS_PINS[axis]);
    }
    readings[axis] = sum / 8;
  }

  for (int index = 0; index < 4; index++) {
    switchOn[index] = digitalRead(SWITCH_PINS[index]) == LOW;
  }

  // Accept OFF immediately and require a stable ON before enabling.
  if (!switchOn[S2_INDEX]) {
    s2Timing = false;
    s2Enabled = false;
  } else {
    if (!s2Timing) {
      s2Timing = true;
      s2OnSince = now;
    }
    s2Enabled = now - s2OnSince >= SWITCH_ON_MS;
  }
}

int throttleValue() {
  const AxisCalibration &axis = active[THROTTLE_AXIS];
  int reading = constrain(readings[THROTTLE_AXIS],
                          int(axis.minimum), int(axis.maximum));
  int travel = axis.maximum - axis.minimum;
  int position = AXIS_REVERSED[THROTTLE_AXIS]
                 ? axis.maximum - reading : reading - axis.minimum;

  // Use an endpoint deadband for throttle. The slider is for stick centres.
  int idleBand = travel * 3 / 100;
  if (position <= idleBand) return 0;
  return (position - idleBand) * 1000 / (travel - idleBand);
}

void finishCalibration() {
  for (int axis = 0; axis < AXIS_COUNT; axis++) {
    pending[axis].centre = centreSums[axis] / centreSamples;
    if (!validAxis(pending[axis])) {
      stage = Stage::Centre;
      statusMessage = String("GPIO") + AXIS_PINS[axis] +
                      " was too close to an endpoint. Centre it and try again.";
      return;
    }
  }

  stage = Stage::Display;
  if (!storageReady ||
      storage.putBytes("axes-v1", pending, sizeof(pending)) != sizeof(pending)) {
    statusMessage = "Flash save failed. Using the previous stick values.";
    return;
  }

  memcpy(active, pending, sizeof(active));
  calibrationSaved = true;
  statusMessage = "Calibration saved. Check throttle direction with S2 off.";
}

int centredAxis(int index, int outputLimit = 1000) {
  const AxisCalibration &axis = active[index];
  int reading = constrain(readings[index], int(axis.minimum), int(axis.maximum));
  int delta = reading - int(axis.centre);
  int travel = delta < 0 ? axis.centre - axis.minimum : axis.maximum - axis.centre;
  int band = travel * deadzone / 100;
  int distance = abs(delta);
  if (distance <= band) return 0;

  int usable = travel - band;
  int offset = ((distance - band) * outputLimit + usable / 2) / usable;
  if (delta < 0) offset = -offset;
  return AXIS_REVERSED[index] ? -offset : offset;
}

void updateInputs(unsigned long now) {
  readInputs(now);
  if (stage == Stage::Limits) {
    for (int axis = 0; axis < AXIS_COUNT; axis++) {
      if (readings[axis] < pending[axis].minimum) pending[axis].minimum = readings[axis];
      if (readings[axis] > pending[axis].maximum) pending[axis].maximum = readings[axis];
    }
  } else if (stage == Stage::Sampling) {
    for (int axis = 0; axis < AXIS_COUNT; axis++) {
      centreSums[axis] += readings[axis];
    }
    centreSamples++;
    if (now - centreStart >= CENTRE_MS) finishCalibration();
  }
}
