#pragma once

#include <WebServer.h>
#include "StickInputs.h"

WebServer server(80);

const char PAGE[] PROGMEM = R"HTML(
<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Stick calibration</title>
<style>
  body { max-width: 34rem; margin: 2rem auto; padding: 0 1rem;
         background: #111827; color: #f3f4f6; font: 16px system-ui, sans-serif; }
  section { background: #1f2937; padding: 1rem; border-radius: 12px; margin: 1rem 0; }
  button, input { font: inherit; }
  button { padding: .65rem; margin: .2rem; }
  input { width: 100%; margin: .75rem 0; }
  #error { color: #fca5a5; }
</style>
</head>
<body>
<h1>Stick calibration</h1>
<section>
  <p id="status" role="status">Connecting…</p>
  <button id="start" disabled>Start calibration</button>
  <button id="limits" disabled>Record limits</button>
  <button id="centre" disabled>Capture centre</button>
  <button id="cancel" disabled>Cancel</button>
  <p id="error" role="alert"></p>
</section>
<section>
  <label for="zone">Centre deadzone <strong id="value">3%</strong></label>
  <input id="zone" type="range" min="0" max="20" step="1" value="3" disabled>
</section>
<section>
  <strong>Toggle switches</strong>
  <p id="switches">Waiting for readings…</p>
  <p id="throttle"></p>
</section>
<script>
const get = id => document.getElementById(id);
const slider = get('zone');
const actions = ['start', 'limits', 'centre', 'cancel'];
let busy = false;
let refreshing = false;
let editingZone = false;
let zoneTimer;
let zoneRequests = Promise.resolve();

async function refresh() {
  if (busy || refreshing) return;
  refreshing = true;
  try {
    const response = await fetch('/state', {cache: 'no-store'});
    if (!response.ok) throw Error('Status unavailable.');
    const state = await response.json();
    if (busy) return;

    get('status').textContent = state.message;
    get('switches').textContent = state.switches
      .map((on, i) => `S${i + 1} ${on ? 'ON' : 'OFF'}`).join(' | ');
    get('throttle').textContent = `Throttle stick ${state.throttle / 10}%`;
    get('start').disabled = state.stage !== 0;
    get('limits').disabled = state.stage !== 1;
    get('centre').disabled = state.stage !== 2;
    get('cancel').disabled = state.stage === 0;
    slider.disabled = false;
    if (!editingZone) {
      slider.value = state.deadzone;
      get('value').textContent = slider.value + '%';
    }
  } catch {
    get('status').textContent = 'Connection lost. Reconnect to Stick-Cal.';
    for (const id of [...actions, 'zone']) get(id).disabled = true;
  } finally {
    refreshing = false;
  }
}

async function post(path, fields) {
  const response = await fetch(path, {
    method: 'POST',
    body: new URLSearchParams(fields)
  });
  if (!response.ok) throw Error(await response.text());
}

for (const action of actions) {
  get(action).onclick = async () => {
    if (busy) return;
    busy = true;
    for (const id of actions) get(id).disabled = true;
    get('error').textContent = '';
    try {
      await post('/calibrate', {action});
    } catch (error) {
      get('error').textContent = error.message;
    } finally {
      busy = false;
      refresh();
    }
  };
}

function setZone(save) {
  const percent = slider.value;
  zoneRequests = zoneRequests.then(async () => {
    try {
      await post('/deadzone', {percent, save: save ? '1' : '0'});
      get('error').textContent = '';
    } catch (error) {
      get('error').textContent = error.message;
    }
  });
  return zoneRequests;
}

slider.oninput = () => {
  editingZone = true;
  get('value').textContent = slider.value + '%';
  clearTimeout(zoneTimer);
  zoneTimer = setTimeout(() => setZone(false), 150);
};

slider.onchange = async () => {
  clearTimeout(zoneTimer);
  const selected = slider.value;
  await setZone(true);
  if (slider.value === selected) editingZone = false;
};

refresh();
setInterval(refresh, 400);
</script>
</body>
</html>
)HTML";

void handleCalibration() {
  String action = server.arg("action");

  if (action == "start" && stage == Stage::Display) {
    if (switchOn[S2_INDEX]) {
      server.send(409, "text/plain", "Switch S2 off before calibration.");
      return;
    }
    for (AxisCalibration &axis : pending) axis = {ADC_MAX, 0, 0};
    stage = Stage::Limits;
    statusMessage = "Sweep both sticks to all four corners, then tap Record limits.";
  } else if (action == "limits" && stage == Stage::Limits) {
    for (int axis = 0; axis < AXIS_COUNT; axis++) {
      if (int(pending[axis].maximum) - int(pending[axis].minimum) <
          2 * MIN_SIDE_TRAVEL) {
        server.send(409, "text/plain",
                    String("Move GPIO") + AXIS_PINS[axis] + " through more of its travel.");
        return;
      }
    }
    stage = Stage::Centre;
    statusMessage = "Centre both sticks, hold throttle halfway and tap Capture centre.";
  } else if (action == "centre" && stage == Stage::Centre) {
    for (uint32_t &sum : centreSums) sum = 0;
    centreSamples = 0;
    centreStart = millis();
    stage = Stage::Sampling;
    statusMessage = "Hold the sticks still for one second.";
  } else if (action == "cancel" && stage != Stage::Display) {
    stage = Stage::Display;
    statusMessage = "Calibration cancelled. Using the previous stick values.";
  } else {
    server.send(409, "text/plain", "That action is unavailable at this step.");
    return;
  }

  server.send(204);
}

void handleDeadzone() {
  String value = server.arg("percent");
  if (value.length() < 1 || value.length() > 2) {
    server.send(400, "text/plain", "Use a whole percentage from 0 to 20.");
    return;
  }

  for (unsigned int i = 0; i < value.length(); i++) {
    if (value[i] < '0' || value[i] > '9') {
      server.send(400, "text/plain", "Use a whole percentage from 0 to 20.");
      return;
    }
  }

  int requested = value.toInt();
  if (requested > 20) {
    server.send(400, "text/plain", "Maximum deadzone is 20 percent.");
    return;
  }

  deadzone = requested;
  if (server.arg("save") == "1" && deadzone != savedDeadzone) {
    if (!storageReady || storage.putUChar("deadzone", deadzone) != 1) {
      server.send(500, "text/plain", "Deadzone applied. Flash save failed.");
      return;
    }
    savedDeadzone = deadzone;
  }

  server.send(204);
}

void handleState() {
  String json = "{\"stage\":";
  json += int(stage);
  json += ",\"message\":\"";
  json += statusMessage;
  json += "\",\"deadzone\":";
  json += deadzone;
  json += ",\"throttle\":";
  json += throttleValue();
  json += ",\"switches\":[";
  for (int index = 0; index < 4; index++) {
    if (index > 0) json += ',';
    json += switchOn[index] ? "true" : "false";
  }
  json += "]}";

  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", json);
}

void startCalibrationWeb() {
  server.on("/", HTTP_GET, []() {
    server.sendHeader("Cache-Control", "no-store");
    server.send_P(200, "text/html; charset=utf-8", PAGE);
  });
  server.on("/state", HTTP_GET, handleState);
  server.on("/calibrate", HTTP_POST, handleCalibration);
  server.on("/deadzone", HTTP_POST, handleDeadzone);
  server.begin();
}
